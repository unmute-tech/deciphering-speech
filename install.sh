#!/bin/bash

set -e

if [ -z $KALDI_ROOT ]; then
  echo "Install with KALDI_ROOT=... bash install.sh"
  exit 1;
fi

if [ ! -d $KALDI_ROOT ]; then
  echo "KALDI_ROOT=$KALDI_ROOT is not a valid directory"
  exit 1;
fi

ls -1 src/decipherbin/* | grep -v Makefile | grep -Pv ".(cc|h)$" | xargs -I {} rm {}
KALDI_ROOT=$KALDI_ROOT make -j 8 -C src/decipherbin

[ ! -L steps ] && ln -s $KALDI_ROOT/egs/librispeech/s5/steps
[ ! -L utils ] && ln -s $KALDI_ROOT/egs/librispeech/s5/utils
[ ! -L conf ] && ln -s $KALDI_ROOT/egs/librispeech/s5/conf

cat $KALDI_ROOT/egs/librispeech/s5/path.sh | \
  sed "/export KALDI_ROOT=/s@.*@export KALDI_ROOT=$KALDI_ROOT@" > path.sh
echo "export PATH=$PWD/src/decipherbin/:\$PATH" >> path.sh
