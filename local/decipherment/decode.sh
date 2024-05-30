#!/bin/bash

# Begin configuration section.
stage=0
nj=40
# End configuration section.

echo "$0 $@"  # Print the command line for logging

. path.sh
. parse_options.sh || exit 1;
set -e

if [ $# != 4 ]; then
   echo "Usage: $0 [options] <decipher-dir> <lang-dir> <rescore-lang-dir> <decode-dir>"
   echo ""
   echo "main options (for others, see top of script file)"
   echo "  --nj <nj>                                # number of parallel jobs"
   exit 1;
fi

decipher_dir=$1
tgt_lang_dir=$2
rescore_lang_dir=$3
decode_dir=$4

if [ $stage -le 0 ]; then
  mkdir -p $decode_dir/split${nj}

  split -n l/$nj --numeric-suffixes=1 $decode_dir/input.scp $decode_dir/split${nj}/input.scp.
  for x in `seq 1 9`; do
    mv $decode_dir/split${nj}/input.scp.{0${x},${x}}
  done
fi

if [ $stage -le 1 ]; then
  src_syms="scp:$decode_dir/split${nj}/input.scp.JOB"
  fsts_syms="ark:| gzip -c > $decode_dir/fsts.JOB.gz"
  run.pl JOB=1:$nj $decode_dir/log/decipherment_apply.JOB.txt \
    decipherment-apply \
      --power=1 \
      --prune_beam=12 \
      --steps-threshold=10 \
      --remove-weights=false \
      $decipher_dir/{lex.word_final.smoothed.fst,ali.word_final.fst} $tgt_lang_dir/LG.fst "$src_syms" ark,t:- "$fsts_syms" \| \
      utils/int2sym.pl -f 2- $tgt_lang_dir/words.txt \> $decode_dir/trans.JOB.txt || exit 1;

  cat $decode_dir/trans.*.txt | sed 's/@@ @@//g' > $decode_dir/output.txt
fi

if [ $stage -le 2 ]; then
  phi=`awk '$1 == "#0" {print $2}' $tgt_lang_dir/words.txt`
  old_lm="fstmap --map_type=invert $tgt_lang_dir/G.fst |"
  new_lm="$rescore_lang_dir/G.fst"

  run.pl JOB=1:$nj $decode_dir/log/decipherment_rescore.JOB.txt \
    fsts-rescore --phi-label=$phi ark:"gunzip -c $decode_dir/fsts.JOB.gz|" "$old_lm" "$new_lm" ark,t:- ark:/dev/null \| \
      utils/int2sym.pl -f 2- $tgt_lang_dir/words.txt \> $decode_dir/trans_rescored.JOB.txt || exit 1;

  cat $decode_dir/trans_rescored.*.txt | sed 's/@@ @@//g' > $decode_dir/output_rescored.txt
fi
