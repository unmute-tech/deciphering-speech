#include "base/kaldi-common.h"
#include "util/common-utils.h"
#include "fstext/fstext-utils.h"
#include "fstext/kaldi-fst-io.h"
#include "threeway_compose.h"


int main(int argc, char *argv[]) {
  try {
    using namespace kaldi;
    using namespace fst;
    typedef kaldi::int32 int32;
    typedef kaldi::uint64 uint64;
    typedef typename StdArc::Weight Weight;
    typedef typename StdArc::StateId StateId;

    const char *usage =
        "Usage:\n"
        " decipherment-apply <lex-filename> <ali-filename> <lm-filename> <source-rspecifier> <target-wspecifier>\n";

    float power = 2.5;
    float prune_beam = 8;
    float output_prune_beam = 4;
    int steps_threshold = 5;
    bool prune_output = true;
    bool remove_weights = true;

    ParseOptions po(usage);
    po.Register("power", &power, "Power p for P(S|T)^p");
    po.Register("prune_beam", &prune_beam, "Prune beam");
    po.Register("output_prune_beam", &output_prune_beam, "Output prune beam");
    po.Register("steps_threshold", &steps_threshold, "Steps threshold");
    po.Register("prune_output", &prune_output, "Prune output");
    po.Register("remove_weights", &remove_weights, "Remove weights");
    po.Read(argc, argv);

    if (po.NumArgs() != 6) {
      po.PrintUsage();
      exit(1);
    }

    std::string lex_fst_filename = po.GetArg(1),
        ali_fst_filename = po.GetArg(2),
        lm_fst_rspecifier = po.GetArg(3),
        source_rspecifier = po.GetArg(4),
        target_wspecifier = po.GetArg(5),
        fst_wspecifier = po.GetArg(6);

    fst::StdVectorFst *lex_fst = fst::ReadFstKaldi(lex_fst_filename);
    fst::StdVectorFst *ali_fst = fst::ReadFstKaldi(ali_fst_filename);
    fst::StdVectorFst *lm_fst = fst::ReadFstKaldi(lm_fst_rspecifier);

    fst::ArcMap(lex_fst, fst::PowerMapper<fst::StdArc>(power));

    fst::StdVectorFst la_fst;
    fst::Compose(*lex_fst, *ali_fst, &la_fst);

    SequentialTableReader<fst::VectorFstHolder> source_reader(source_rspecifier);
    Int32VectorWriter target_writer(target_wspecifier);
    TableWriter<VectorFstHolder> fst_writer(fst_wspecifier);
    for (; !source_reader.Done(); source_reader.Next()) {
      const std::string key = source_reader.Key();
      fst::StdVectorFst observation_fst(source_reader.Value());

      ThreeWayComposition<fst::StdArc> tc(observation_fst, la_fst, *lm_fst, steps_threshold, prune_beam, -1);
      fst::StdVectorFst deciphered_fst = tc.GetFst();

      fst::StdVectorFst shortest_path;
      fst::ShortestPath(deciphered_fst, &shortest_path);

      std::vector<int32> tgt_sequence;
      fst::GetLinearSymbolSequence<fst::StdArc, int32>(shortest_path, NULL, &tgt_sequence, NULL);

      if (tgt_sequence.size() > 0) {
        target_writer.Write(key, tgt_sequence);

        fst::StdVectorFst output_fst;
        if (prune_output) {
          fst::Prune(&deciphered_fst, output_prune_beam);
        }
        fst::Project(&deciphered_fst, fst::PROJECT_OUTPUT);
        if (remove_weights) {
          fst::RemoveWeights(&deciphered_fst);
        }
        fst::RmEpsilon(&deciphered_fst);
        fst::Determinize(deciphered_fst, &output_fst);
        fst::Minimize(&output_fst);

        fst_writer.Write(key, output_fst);
        KALDI_LOG << key << " processed with fst " << output_fst.NumStates() << " states and " << fst::NumArcs(output_fst) << " arcs";
      } else {
        KALDI_LOG << key << " is empty";
      }
    }

    delete lex_fst;
    delete ali_fst;
    delete lm_fst;

    return 0;
  } catch(const std::exception &e) {
    std::cerr << e.what();
    return -1;
  }
}
