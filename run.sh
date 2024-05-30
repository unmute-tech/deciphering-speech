#!/bin/bash

# Begin configuration section.
stage=0
nj=40
num_minutes=20
num_restarts=50
# End configuration section.

echo "$0 $@"  # Print the command line for logging

. path.sh
. parse_options.sh || exit 1;
set -e

if [ $# != 4 ]; then
   echo "Usage: $0 [options] <lang_iso> <script> <input-dir> <work-dir>"
   echo ""
   echo "main options (for others, see top of script file)"
   echo "  --stage <stage>                          # stage"
   echo "  --nj <nj>                                # number of parallel jobs"
   echo "  --num-minutes <num_minutes>              # amount of data for decipherment training"
   echo "  --num-restarts <num-restarts>            # number of random restarts"
   exit 1;
fi

lang=$1
script=$2
input_dir=$3
work_dir=$4

data_dir=$work_dir/data
phone_decode_dir=$work_dir/phone_decode
decipher_dir=$work_dir/decipher_${num_minutes}minutes
src_lang_dir=exp/multilingual/chain/tdnnf_768x90_xls_r_300m_18/phone_bigram_graph_no_spn
tgt_lang_dir=$work_dir/lang_bpe_10000_p07
rescore_lang_dir=$work_dir/lang_bpe_10000_p09
decode_dir=$decipher_dir/decode

if [ $stage -le 0 ]; then
  echo "Preparing language models for decipherment"

  local/lang/prepare_lang.sh $lang $script $work_dir
fi

if [ $stage -le 1 ]; then
  echo "Decoding input wav files with a multilingual phone recogniser"

  local/multilingual_phone_recogniser/decode.sh --nj $nj \
    $input_dir $data_dir $phone_decode_dir
fi

if [ $stage -le 2 ]; then
  echo "Training the decipherment model"

  local/decipherment/prepare_input.sh \
    --num-minutes $num_minutes \
    --utt2dur $work_dir/data/utt2dur \
    $phone_decode_dir $src_lang_dir $decipher_dir

  local/decipherment/train.sh \
    --num-threads $nj \
    --num-restarts $num_restarts \
    $src_lang_dir $tgt_lang_dir $decipher_dir 2>&1 | tee -a $decipher_dir/log || exit 1;
fi

if [ $stage -le 3 ]; then
  echo "Decoding the input data with the decipherment model"

  local/decipherment/prepare_input.sh \
    $phone_decode_dir $src_lang_dir $decode_dir

  local/decipherment/decode.sh --nj $nj \
    $decipher_dir $tgt_lang_dir $rescore_lang_dir $decode_dir
fi
