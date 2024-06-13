#!/bin/bash

# Begin configuration section.
utt2dur=
num_minutes=
use_sausages=false
# End configuration section.

echo "$0 $@"  # Print the command line for logging

. path.sh
. parse_options.sh || exit 1;
set -e

if [ $# != 3 ]; then
   echo "Usage: $0 [options] <phone-decode-dir> <lang-dir> <dir>"
   echo ""
   echo "main options (for others, see top of script file)"
   echo "  --utt2dur <utt2dur>                      # utt2dur file"
   echo "  --num-minutes <num_minutes>              # amount of data for decipherment training"
   echo "  --use-sausages <true|false>              # use sausages (default: false)"
   exit 1;
fi

phone_decode_dir=$1
lang_dir=$2
dir=$3

mkdir -p $dir

if [ ! -z $num_minutes ]; then
  if [ $use_sausages == true ]; then
    echo "Sausages can be used only for decoding"
    exit 1;
  fi

  sort -k 2,2n $utt2dur | \
    awk -v num_minutes=$num_minutes '{print $1; total += $2; if(total  >= num_minutes * 60) exit}' | \
    sort > $dir/train_utts
  
  sort $phone_decode_dir/phones.txt | \
    join - $dir/train_utts | \
    utils/sym2int.pl -f 2- $lang_dir/phones.txt | \
    awk '{print NF - 1, $0}' | \
    sort -k 1,1nr | \
    cut -f 2- -d ' ' | \
    transcripts-to-fsts ark,t:- ark,scp:$dir/input.ark,$dir/input.scp
else
  if [ $use_sausages == true ]; then
    nj=`cat $phone_decode_dir/num_jobs`
    run.pl JOB=1:$nj $phone_decode_dir/pcn/log/lattices-to-phone-fsts.JOB.txt \
      lattice-prune --acoustic-scale=0.1 --beam=4.0 "ark:gunzip -c $phone_decode_dir/lat.JOB.gz |" ark:- \| \
      lattices-to-phone-fsts --acoustic-scale=0.1 --lm-scale=1.0 ark:- ark,scp:$phone_decode_dir/pcn/fsts.JOB.ark,$phone_decode_dir/pcn/fsts.JOB.scp

    cat $phone_decode_dir/pcn/fsts.*.scp > $dir/input.scp
  else
    utils/sym2int.pl -f 2- $lang_dir/phones.txt $phone_decode_dir/phones.txt | \
      transcripts-to-fsts ark,t:- ark,scp:$dir/input.ark,$dir/input.scp
  fi
fi
