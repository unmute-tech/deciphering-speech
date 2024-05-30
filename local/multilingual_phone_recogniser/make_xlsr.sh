#!/usr/bin/env bash

# Copyright 2012-2016  Johns Hopkins University (Author: Daniel Povey)
# Apache 2.0
# To be run from .. (one directory up from here)
# see ../run.sh for example

# Begin configuration section.
nj=4
cmd=run.pl
compress=true
model=xls_r_300m
layer=18
# End configuration section.

echo "$0 $@"  # Print the command line for logging.

if [ -f path.sh ]; then . ./path.sh; fi
. parse_options.sh || exit 1;

if [ $# -ne 1 ]; then
  echo Num params $#
  cat >&2 <<EOF
Usage: $0 [options] <data-dir>
 e.g.: $0 data/train

Options:
  --nj <nj>                            # number of parallel jobs.
  --write-utt2num-frames <true|false>  # If true, write utt2num_frames file.
  --write-utt2dur <true|false>         # If true, write utt2dur file.
EOF
   exit 1;
fi

data=$1
logdir=$data/log
featdir=$data/data

# make $xlsrdir an absolute pathname.
featdir=`perl -e '($dir,$pwd)= @ARGV; if($dir!~m:^/:) { $dir = "$pwd/$dir"; } print $dir; ' $featdir ${PWD}`

# use "name" as part of name of the archive.
name=`basename $data`

mkdir -p $featdir || exit 1;
mkdir -p $logdir || exit 1;

if [ -f $data/feats.scp ]; then
  mkdir -p $data/.backup
  echo "$0: moving $data/feats.scp to $data/.backup"
  mv $data/feats.scp $data/.backup
fi

scp=$data/wav.scp
required="$scp"

for f in $required; do
  if [ ! -f $f ]; then
    echo "$0: no such file $f"
    exit 1;
  fi
done

utils/validate_data_dir.sh --no-text --no-feats $data || exit 1;
write_num_frames_opt="--write-num-frames=ark,t:$logdir/utt2num_frames.JOB"

if [ -f $data/segments ]; then
  echo "$0 [info]: segments file exists: using that."

  split_segments=
  for n in $(seq $nj); do
    split_segments="$split_segments $logdir/segments.$n"
  done

  utils/split_scp.pl $data/segments $split_segments || exit 1;
  rm $logdir/.error 2>/dev/null

  $cmd JOB=1:$nj $logdir/make_feat_${name}.JOB.log \
    extract-segments scp,p:$scp $logdir/segments.JOB ark:- \| \
    python3 local/multilingual_phone_recogniser/make_xlsr.py \
      --model_name=$model \
      --layer=$layer \
      --job=JOB \
      --input_rspecifier=ark:- --output_wspecifier=ark:- \| \
    copy-feats --compress=$compress $write_num_frames_opt ark:- ark,scp:$featdir/raw_feat_$name.JOB.ark,$featdir/raw_feat_$name.JOB.scp || exit 1;
else
  echo "$0: [info]: no segments file exists: assuming wav.scp indexed by utterance."
  split_scps=
  for n in $(seq $nj); do
    split_scps="$split_scps $logdir/wav_${name}.$n.scp"
  done

  utils/split_scp.pl $scp $split_scps || exit 1;


  # add ,p to the input rspecifier so that we can just skip over
  # utterances that have bad wave data.

  $cmd JOB=1:$nj $logdir/make_feat_${name}.JOB.log \
    python3 local/multilingual_phone_recogniser/make_xlsr.py \
      --model_name=$model \
      --layer=$layer \
      --job=JOB \
      --input_rspecifier=scp,p:$logdir/wav_${name}.JOB.scp --output_wspecifier=ark:- \| \
    copy-feats $write_num_frames_opt --compress=$compress ark:- ark,scp:$featdir/raw_feat_$name.JOB.ark,$featdir/raw_feat_$name.JOB.scp || exit 1;
fi

if [ -f $logdir/.error.$name ]; then
  echo "$0: Error producing SSL features for $name:"
  tail $logdir/make_feat_${name}.1.log
  exit 1;
fi

# concatenate the .scp files together.
for n in $(seq $nj); do
  cat $featdir/raw_feat_$name.$n.scp || exit 1
done > $data/feats.scp || exit 1

for n in $(seq $nj); do
  cat $logdir/utt2num_frames.$n || exit 1
done > $data/utt2num_frames || exit 1

awk '{printf("%s %.2f\n", $1, $2 * 0.02)}' $data/utt2num_frames > $data/utt2dur

# Store frame_shift and feat_config along with features.
echo 0.02 > $data/frame_shift
echo $model $layer > $data/conf

rm $logdir/wav_${name}.*.scp  $logdir/segments.* $logdir/utt2num_frames.* $logdir/utt2dur.* 2>/dev/null

nf=$(wc -l < $data/feats.scp)
nu=$(wc -l < $data/utt2spk)
if [ $nf -ne $nu ]; then
  echo "$0: It seems not all of the feature files were successfully procesed" \
       "($nf != $nu); consider using utils/fix_data_dir.sh $data"
fi

if (( nf < nu - nu/20 )); then
  echo "$0: Less than 95% the features were successfully generated."\
       "Probably a serious error."
  exit 1
fi

echo "$0: Succeeded creating SSL features for $name"
