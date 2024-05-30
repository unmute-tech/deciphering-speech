# make "all" the target.
all:

# Disable linking math libs because  not needed here.  Just for compilation speed.
MATHLIB = NONE

EXTRA_CXXFLAGS = -Wno-unused-variable -Wno-sign-compare -I${KALDI_ROOT}/src

include ${KALDI_ROOT}/src/kaldi.mk

BINFILES = decipherment-learn decipherment-apply lattices-to-phone-fsts \
           transcripts-to-fsts fsts-rescore

OBJFILES =

TESTFILES =

LIBFILE =

ADDLIBS = ${KALDI_ROOT}/src/fstext/kaldi-fstext.a \
          ${KALDI_ROOT}/src/lat/kaldi-lat.a \
          ${KALDI_ROOT}/src/hmm/kaldi-hmm.a \
          ${KALDI_ROOT}/src/gmm/kaldi-gmm.a \
          ${KALDI_ROOT}/src/tree/kaldi-tree.a \
          ${KALDI_ROOT}/src/util/kaldi-util.a \
          ${KALDI_ROOT}/src/matrix/kaldi-matrix.a \
          ${KALDI_ROOT}/src/base/kaldi-base.a

include ${KALDI_ROOT}/src/makefiles/default_rules.mk
