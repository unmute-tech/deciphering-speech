#!/bin/bash

# Begin configuration section.
stage=0
nj=40
ssl_model=xls_r_300m
ssl_layer=18
nnet_dir="exp/multilingual/chain/tdnnf_768x90_xls_r_300m_18/"
graph_dir="$nnet_dir/phone_bigram_graph_no_spn/"
# End configuration section.

echo "$0 $@"  # Print the command line for logging

. path.sh
. parse_options.sh || exit 1;
set -e

if [ $# != 3 ]; then
   echo "Usage: $0 [options] <input-dir> <data-dir> <phone-decode-dir>"
   echo ""
   echo "main options (for others, see top of script file)"
   echo "  --stage <stage>                          # stage"
   echo "  --nj <nj>                                # number of parallel jobs"
   exit 1;
fi

input_dir=$1
data_dir=$2
phone_decode_dir=$3
decode_dir="$nnet_dir/phone_bigram_decode"

if [ $stage -le 0 ]; then
  mkdir -p $data_dir
  rm -f $data_dir/{wav.scp,text,utt2spk,spk2utt}

  for f in $input_dir/*.wav; do
    name=`basename $f .wav`
    echo "$name sox -t wav $f -c 1 -b 16 -t wav - rate 16000 |" >> $data_dir/wav.scp
    echo "$name" >> $data_dir/text
    echo "$name $name" >> $data_dir/utt2spk
    echo "$name $name" >> $data_dir/spk2utt
  done

  utils/validate_data_dir.sh --no-feats $data_dir
fi

if [ $stage -le 1 ]; then
  local/multilingual_phone_recogniser/make_xlsr.sh --nj 4 --model xls_r_300m --layer 18 $data_dir
  steps/compute_cmvn_stats.sh $data_dir
fi

if [ $stage -le 2 ]; then
  steps/nnet3/decode.sh --acwt 1.0 --post-decode-acwt 10.0 --lattice-beam 4.0 \
    --nj $((nj/4)) --num-threads 4 \
    --skip-scoring true --skip-diagnostics true \
    $graph_dir $data_dir $decode_dir || exit 1;

  lattice-scale --inv-acoustic-scale=10 "ark:gunzip -c $decode_dir/lat.*.gz|" ark:- 2> /dev/null | \
    lattice-best-path ark:- ark:/dev/null ark:- 2> /dev/null | \
    ali-to-phones $nnet_dir/final.mdl ark:- ark,t:- | \
    utils/int2sym.pl -f 2- $graph_dir/phones.txt | \
    sed 's/oU/o u/g;s/aI/a i/g;s/eI/e i/g'> $decode_dir/phones.txt

  mv $decode_dir $phone_decode_dir
fi
