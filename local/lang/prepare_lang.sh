#!/bin/bash

# begin configuration section.
stage=0
nj=40
num_units=10000
glossaries="<eps> !SIL <unk> <UNK> #0 <s> </s>"
order=3
# end configuration section.

echo "$0 $@"  # print the command line for logging

. path.sh
. parse_options.sh || exit 1;
set -e

if [ $# != 3 ]; then
   echo "usage: $0 [options] <lang_iso> <script> <work-dir>"
   echo ""
   echo "main options (for others, see top of script file)"
   echo "  --stage <stage>                          # stage"
   echo "  --nj <nj>                                # number of parallel jobs"
   echo "  --num-bpe-units <number-bpe-units>       # number of bpe units"
   echo "  --order <ngram-order>                    # n-gram order"
   exit 1;
fi

lang=$1
script=$2
work_dir=$3

text_dir="$work_dir/text_data"
raw_text=$text_dir/madlad_clean_0000.txt
normalized_text=$text_dir/madlad_clean_0000.normalized.txt
tokenized_text=$text_dir/madlad_clean_0000.normalized.bpe_${num_units}.txt
small_text_ids=$text_dir/madlad_clean_0000.normalized.50k.int
pair_codes="$work_dir/pair_codes"
dict_dir=$work_dir/dict
lang_dir=$work_dir/lang_bpe_${num_units}_p07
rescore_lang_dir=$work_dir/lang_bpe_${num_units}_p09
lm=$work_dir/lms/bpe_${num_units}.p07.arpa.gz
rescore_lm=$work_dir/lms/bpe_${num_units}.p09.arpa.gz

mkdir -p $text_dir $work_dir/lms

if [ $stage -le 0 ]; then
  echo Downloading madlad 400 ${lang}_clean_0000.jsonl.gz
  base_url=https://huggingface.co/datasets/allenai/MADLAD-400/resolve/main/data
  url=$base_url/${lang}/${lang}_clean_0000.jsonl.gz

  curl -L $url | \
    zcat | \
    jq -r '.text' | \
    sed 's/\\n/\n/g;s/\\t/\t/g' > $raw_text
fi

if [ $stage -le 1 ]; then
  echo Normalizing text data
  python3 local/normalize_text.py \
    --nj $nj \
    --script $script \
    --input-file $raw_text \
    --output-file $normalized_text 
fi

if [ $stage -le 2 ]; then
  echo Learning bpe for $lang
  cat $normalized_text | \
    sed 's/<unk>//g' | \
    head -n 100000 | \
    utils/lang/bpe/learn_bpe.py -s $num_units > $pair_codes
fi

if [ $stage -le 3 ]; then
  echo Tokenizing text file for $lang
  cat $normalized_text | \
    local/lang/apply_bpe_parallel.py --nj $nj -c $pair_codes --glossaries $glossaries | \
    sed 's/@@  */@@ @@/g' > $tokenized_text
fi

if [ $stage -le 4 ]; then
  echo Creating dict directory for $lang
  mkdir -p $dict_dir

  cat $tokenized_text | \
    awk '{for(i=1; i <= NF; i++) d[$i] = 1} END {for(w in d) print w}' |
    sort | \
    grep -v "<unk>" > $dict_dir/subwords.txt
  LC_ALL=en_US.utf8 python3 local/lang/bpe_to_pronunciation.py $dict_dir/subwords.{txt,lex}

  (
    echo "!SIL sil"
    echo "<unk> spn"
    cat $dict_dir/subwords.lex
  ) > $dict_dir/lexicon.txt
     
  echo -e "sil\nspn" > $dict_dir/silence_phones.txt
  echo sil > $dict_dir/optional_silence.txt
  awk '{for(i=2; i <= NF; i++) {c[$i] = 1}} END {for(i in c) print i}' $dict_dir/lexicon.txt | \
    sort | grep -v spn | grep -v sil > $dict_dir/nonsilence_phones.txt

  utils/validate_dict_dir.pl $dict_dir || exit 1
fi

if [ $stage -le 5 ]; then
  echo Creating lang directory for $lang
  utils/prepare_lang.sh \
    --position-dependent-phones true \
    --num-extra-phone-disambig-syms 1 \
    $dict_dir "<unk>" $lang_dir/tmp $lang_dir || exit 1;

  local/lang/make_lfst_lr.py $(tail -n1 $lang_dir/phones/disambig.txt) < $lang_dir/tmp/lexiconp_disambig.txt | \
    fstcompile --isymbols=$lang_dir/phones.txt --osymbols=$lang_dir/words.txt --keep_isymbols=false --keep_osymbols=false | \
    fstaddselfloops $lang_dir/phones/wdisambig_phones.int $lang_dir/phones/wdisambig_words.int | fstarcsort --sort_type=olabel > $lang_dir/L_disambig.fst
fi

if [ $stage -le 6 ]; then
  if [ ! -f $lm ]; then
    echo Training LM for $lang
    cat $tokenized_text | \
      sed 's/<unk>//g;s/  */ /g' | \
      grep -Pv "((.)@@ )\1+\2\b" | \
      grep -xv "" | \
      awk 'length($0) <= 1500' | \
      ngram-count -order $order -prune 1e-7 -wbdiscount1 -kndiscount -interpolate -sort -text - -lm $lm || exit 1;
  fi
fi

if [ $stage -le 7 ]; then
  if [ ! -f $lang_dir/G.fst ]; then
    echo Creating G.fst for $lang
    local/lang/arpa2G.sh $lm $lang_dir $lang_dir || exit 1;
  fi
fi

if [ $stage -le 8 ]; then
  if [ ! -f $lang_dir/LG.fst ]; then
    echo Creating LG.fst for $lang

    local/lang/create_pure_phones.sh $lang_dir

    fstprint $lang_dir/L_disambig.fst | \
      awk '{if ($1 == 0 && $2 == 1) print $1, $2, $3, $4; else if ($2 == 2) {if ($1 > 0) print $0;} else print $0} END {print 0, 0}' | \
      fstcompile | \
      fstarcsort --sort_type=olabel | \
      fsttablecompose - $lang_dir/G.fst | \
      fstdeterminizestar --use-log=true | \
      fstrmsymbols --remove-arcs=false $lang_dir/phones/disambig.int | \
      fstrelabel --relabel_ipairs=$lang_dir/phones_to_pure_id_map.int | \
      fstrmepslocal | \
      fstminimizeencoded | \
      fstpushspecial | \
      fstrmsymbols --remove-arcs=true --apply-to-output=true $lang_dir/oov.int | \
      fstarcsort --sort_type=ilabel > $lang_dir/LG.fst
  fi
fi

if [ $stage -le 9 ]; then
  echo Training rescoring LM for $lang
  cat $tokenized_text | \
    sed 's/<unk>//g;s/  */ /g' | \
    grep -Pv "((.)@@ )\1+\2\b" | \
    grep -xv "" | \
    awk 'length($0) <= 1500' | \
    ngram-count -order $order -prune 1e-9 -wbdiscount1 -kndiscount -interpolate -sort -text - -lm $rescore_lm || exit 1;
fi

if [ $stage -le 10 ]; then
  echo Creating rescoring G.fst for $lang
  cp -r $lang_dir $rescore_lang_dir
  local/lang/arpa2G.sh $rescore_lm $rescore_lang_dir $rescore_lang_dir || exit 1;
fi

if [ $stage -le 11 ]; then
  echo Training grapheme LMs for $lang

  head -n 50000 $normalized_text | \
    sed 's/<unk>//g;s/  */ /g;' | \
    awk 'NF > 0 {print "utt", $0}' | \
    LC_ALL=en_GB.UTF-8 python3 local/lang/apply_pronunciation_with_silence.py --output-leading-silence=true --sil-phone=sil | \
    utils/sym2int.pl -f 2- $lang_dir/phones_pure.txt > $small_text_ids

  tgt_syms="ark:$small_text_ids"
  chain-est-phone-lm --ngram-order=2 --no-prune-ngram-order=2 "$tgt_syms" $lang_dir/lm.2gm.fst
  chain-est-phone-lm --ngram-order=3 --no-prune-ngram-order=3 "$tgt_syms" $lang_dir/lm.3gm.fst
  chain-est-phone-lm --ngram-order=4 --no-prune-ngram-order=3 --num-extra-lm-states=1000 "$tgt_syms" $lang_dir/lm.4gm.fst
  chain-est-phone-lm --ngram-order=5 --no-prune-ngram-order=3 --num-extra-lm-states=2000 "$tgt_syms" $lang_dir/lm.5gm.fst
fi
