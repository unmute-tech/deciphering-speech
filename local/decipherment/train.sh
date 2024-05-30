#!/bin/bash

. path.sh

set -e

# Begin configuration section.
stage=0
num_threads=32
num_restarts=50
keep_top=20
prune_beam=12
steps_threshold=10
# End configuration section.

echo "$0 $@"  # Print the command line for logging
. parse_options.sh || exit 1;

if [ $# != 3 ]; then
   echo "Usage: $0 [options] <src_lang_dir> <tgt_lang_dir> <decipher_dir>"
   echo ""
   echo "main options (for others, see top of script file)"
   echo "  --stage <stage>                          # stage"
   echo "  --num-threads <num-threads>              # number of threads"
   echo "  --num-restarts <num-restarts>            # number of random restarts"
   exit 1;
fi

src_lang_dir=$1
tgt_lang_dir=$2
dir=$3

src_syms="ark:$dir/input.ark"
num_src_syms=`grep -v "^#" $src_lang_dir/phones.txt | wc -l`
num_tgt_syms=`cat $tgt_lang_dir/phones_pure.txt | wc -l`

if [ $stage -le 0 ]; then
  mkdir -p $dir/random_restarts

  for i in `seq $num_restarts`; do
    if [ $i == 1 ]; then
      random_weights=false
    else
      random_weights=true
    fi

    LC_ALL=en_US.utf8 python local/decipherment/create_lexical_model.py \
      --num_src_symbols=$num_src_syms \
      --num_tgt_symbols=$num_tgt_syms \
      --random_weights=$random_weights \
      --seed $i | \
      fstcompile | fstarcsort --sort_type=olabel > $dir/random_restarts/lex.init.${i}.fst
  done

  LC_ALL=en_US.utf8 python local/decipherment/create_alignment_model.py \
    --num_symbols=$num_tgt_syms \
    --max_insertions=2 \
    --prob_insertion=0.1 \
    --max_deletions=2 \
    --prob_deletion=0.1 | \
    fstcompile | fstarcsort --sort_type=ilabel > $dir/random_restarts/ali.init.fst
fi

if [ $stage -le 1 ]; then
  mkdir -p $dir/random_restarts/logs

  for i in `seq $num_restarts`; do
    time decipherment-learn \
      --num-source-symbols=$num_src_syms \
      --num-target-symbols=$num_tgt_syms \
      --train-lex=true \
      --train-ali=false \
      --num-iters=20 \
      --num-threads=$num_threads \
      $dir/random_restarts/{lex.init.${i}.fst,ali.init.fst} \
      $tgt_lang_dir/lm.2gm.fst "$src_syms" \
      $dir/random_restarts/{lex.stage1.${i}.fst,ali.stage1.${i}.fst} 2>&1 | \
      tee $dir/random_restarts/logs/train_2gm.${i}.log 
  done

  grep -Po "(?<=likelihood )\d+" $dir/random_restarts/logs/*.log | \
    sed 's/.*\.\([0-9]*\)\.log:/\1 /g' | \
    sort -k 2,2n | \
    head -n 1 | \
    awk '{print $1}' > $dir/random_restarts/best_init
fi

if [ $stage -le 2 ]; then
  best_init=`cat $dir/random_restarts/best_init`
  fstprint $dir/random_restarts/lex.stage1.${best_init}.fst | \
    LC_ALL=en_US.utf8 python local/decipherment/prune_lexical_model.py \
      --num_src_symbols=$num_src_syms \
      --num_tgt_symbols=$num_tgt_syms \
      --keep_top_tgt_symbols=$keep_top | \
    fstcompile > $dir/lex.stage1.pruned.fst

  cp $dir/random_restarts/ali.stage1.${best_init}.fst $dir/ali.stage1.fst

  time decipherment-learn \
    --num-source-symbols=$num_src_syms \
    --num-target-symbols=$num_tgt_syms \
    --train-lex=true \
    --train-ali=true \
    --num-iters=20 \
    --num-threads=$num_threads \
    $dir/{lex.stage1.pruned.fst,ali.stage1.fst} $tgt_lang_dir/lm.3gm.fst \
    "$src_syms" $dir/{lex.stage2.fst,ali.stage2.fst} || exit 1;
fi

if [ $stage -le 3 ]; then
  time decipherment-learn \
    --num-source-symbols=$num_src_syms \
    --num-target-symbols=$num_tgt_syms \
    --train-lex=true \
    --train-ali=true \
    --num-iters=20 \
    --num-threads=$num_threads \
    $dir/{lex.stage2.fst,ali.stage2.fst} $tgt_lang_dir/lm.4gm.fst \
    "$src_syms" $dir/{lex.stage3.fst,ali.stage3.fst} || exit 1;
fi

if [ $stage -le 4 ]; then
  time decipherment-learn \
    --num-source-symbols=$num_src_syms \
    --num-target-symbols=$num_tgt_syms \
    --train-lex=true \
    --train-ali=true \
    --num-iters=20 \
    --num-threads=$num_threads \
    $dir/{lex.stage3.fst,ali.stage3.fst} $tgt_lang_dir/lm.5gm.fst \
    "$src_syms" $dir/{lex.final.fst,ali.final.fst} || exit 1;
fi

if [ $stage -le 5 ]; then
  fstprint $dir/lex.final.fst | \
    LC_ALL=en_US.utf8 python local/decipherment/smooth_lexical_model.py \
      --num_src_symbols=$num_src_syms \
      --num_tgt_symbols=$num_tgt_syms \
      --alpha=0.9 | \
    fstcompile > $dir/lex.final.smoothed.fst

  time decipherment-learn \
    --num-source-symbols=$num_src_syms \
    --num-target-symbols=$num_tgt_syms \
    --train-lex=true \
    --train-ali=true \
    --threeway=true \
    --prune-beam=$prune_beam \
    --steps-threshold=$steps_threshold \
    --num-iters=20 \
    --num-threads=$num_threads \
    $dir/{lex.final.smoothed.fst,ali.final.fst} $tgt_lang_dir/LG.fst \
    "$src_syms" $dir/{lex.word_final.fst,ali.word_final.fst} || exit 1;

  fstprint $dir/lex.word_final.fst | \
    LC_ALL=en_US.utf8 python local/decipherment/smooth_lexical_model.py \
      --num_src_symbols=$num_src_syms \
      --num_tgt_symbols=$num_tgt_syms \
      --alpha=0.9 | \
    fstcompile > $dir/lex.word_final.smoothed.fst
fi
