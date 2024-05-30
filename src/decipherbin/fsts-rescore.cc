// latbin/fsts-compose.cc

// Copyright 2019 Joachim Fainberg  Edinburgh University
//
// Based on lattice-compose.cc, with copyright:
// Copyright 2009-2011  Microsoft Corporation;  Saarland University

// See ../../COPYING for clarification regarding multiple authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED
// WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
// MERCHANTABLITY OR NON-INFRINGEMENT.
// See the Apache 2 License for the specific language governing permissions and
// limitations under the License.


#include "base/kaldi-common.h"
#include "util/common-utils.h"
#include "fstext/fstext-lib.h"
#include "fstext/table-matcher.h"
#include "fstext/fstext-utils.h"
#include "fstext/kaldi-fst-io.h"

int main(int argc, char *argv[]) {
  try {
    using namespace kaldi;
    using namespace fst;
    typedef kaldi::int32 int32;
    typedef kaldi::int64 int64;
    using fst::SymbolTable;
    using fst::VectorFst;
    using fst::StdArc;

    const char *usage =
        "Like fstcompose, but composes archives of fsts (stored in scp/ark with uttids).\n"
        "\n"
        "Usage: fsts-compose [options] fst-rspecifier1 "
        "fst-rxfilename2 fst-wspecifier\n"
        " e.g.: fsts-compose ark:1.fsts G.fst ark:composed.fsts\n";

    ParseOptions po(usage);

    int32 phi_label = fst::kNoLabel; // == -1
    float output_prune_beam = 4;
    bool prune_output = true;
    bool remove_weights = true;
    po.Register("phi-label", &phi_label, "If >0, the label on backoff arcs of the LM");
    po.Register("output_prune_beam", &output_prune_beam, "Output prune beam");
    po.Register("prune_output", &prune_output, "Prune output");
    po.Register("remove_weights", &remove_weights, "Remove weights");
    po.Read(argc, argv);

    if (po.NumArgs() != 5) {
      po.PrintUsage();
      exit(1);
    }

    KALDI_ASSERT(phi_label > 0);

    std::string fst_rspecifier = po.GetArg(1),
        old_lm_rxfilename = po.GetArg(2),
        new_lm_rxfilename = po.GetArg(3),
        target_wspecifier = po.GetArg(4),
        fst_wspecifier = po.GetArg(5);
    int32 n_done = 0, n_fail = 0;

    SequentialTableReader<VectorFstHolder> fst_reader(fst_rspecifier);
    Int32VectorWriter target_writer(target_wspecifier);
    TableWriter<VectorFstHolder> fst_writer(fst_wspecifier);
    
    VectorFst<StdArc> *old_lm_fst = fst::ReadFstKaldi(old_lm_rxfilename);
    VectorFst<StdArc> *new_lm_fst = fst::ReadFstKaldi(new_lm_rxfilename);

    if (old_lm_fst->Properties(fst::kILabelSorted, true) == 0) {
      fst::ILabelCompare<StdArc> ilabel_comp;
      ArcSort(old_lm_fst, ilabel_comp);
    }

    if (new_lm_fst->Properties(fst::kILabelSorted, true) == 0) {
      fst::ILabelCompare<StdArc> ilabel_comp;
      ArcSort(new_lm_fst, ilabel_comp);
    }

    PropagateFinal(phi_label, old_lm_fst);
    PropagateFinal(phi_label, new_lm_fst);

    for (; !fst_reader.Done(); fst_reader.Next()) {
      std::string key = fst_reader.Key();
      VectorFst<StdArc> fst = fst_reader.Value();
      fst::OLabelCompare<StdArc> olabel_comp;
      ArcSort(&fst, olabel_comp);

      fst::StdVectorFst composed_fst, rescored_fst;
      PhiCompose(fst, *old_lm_fst, phi_label, &composed_fst);
      PhiCompose(composed_fst, *new_lm_fst, phi_label, &rescored_fst);

      fst::StdVectorFst shortest_path;
      fst::ShortestPath(rescored_fst, &shortest_path);

      std::vector<int32> tgt_sequence;
      fst::GetLinearSymbolSequence<fst::StdArc, int32>(shortest_path, NULL, &tgt_sequence, NULL);

      if (tgt_sequence.size() > 0) {
        fst::StdVectorFst output_fst;
        if (prune_output) {
          fst::Prune(&rescored_fst, output_prune_beam);
        }
        fst::Project(&rescored_fst, fst::PROJECT_OUTPUT);
        if (remove_weights) {
          fst::RemoveWeights(&rescored_fst);
        }
        fst::RmEpsilon(&rescored_fst);
        fst::Determinize(rescored_fst, &output_fst);
        fst::Minimize(&output_fst);

        fst_writer.Write(key, output_fst);
        target_writer.Write(key, tgt_sequence);
        KALDI_LOG << key << " rescored with fst " << output_fst.NumStates() << " states and " << fst::NumArcs(output_fst) << " arcs";
        n_done++;
      } else {
        target_writer.Write(key, tgt_sequence);
        KALDI_LOG << key << " is empty";
        n_fail++;
      }
    }

    KALDI_LOG << "Done " << n_done << " fsts; failed for "
              << n_fail;
    return (n_done != 0 ? 0 : 1);
  } catch(const std::exception &e) {
    std::cerr << e.what();
    return -1;
  }
}
