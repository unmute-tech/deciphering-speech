#include "base/kaldi-common.h"
#include "hmm/transition-model.h"
#include "hmm/tree-accu.h" 
#include "lat/kaldi-lattice.h"
#include "lat/phone-align-lattice.h"
#include "lat/lattice-functions.h"
#include "lat/sausages.h"
#include "util/common-utils.h"


int main(int argc, char *argv[]) {
  try {
    using namespace kaldi;
    using kaldi::int32;

    const char *usage = "";
    ParseOptions po(usage);

    BaseFloat acoustic_scale = 0.1;
    BaseFloat lm_scale = 1.0;
    
    po.Register("acoustic-scale", &acoustic_scale, "Scaling factor for acoustic likelihoods");
    po.Register("lm-scale", &lm_scale, "Scaling factor for graph/lm costs");
    
    MinimumBayesRiskOptions mbr_opts;
    mbr_opts.Register(&po);

    po.Read(argc, argv);

    if (po.NumArgs() != 2) {
      po.PrintUsage();
      exit(1);
    }

    std::string lats_rspecifier = po.GetArg(1),
        fsts_wspecifier = po.GetArg(2);
    
    SequentialCompactLatticeReader clat_reader(lats_rspecifier);
    TableWriter<fst::VectorFstHolder> fst_writer(fsts_wspecifier);

    int32 num_done = 0, num_err = 0;
    std::vector<std::vector<double> > scale = fst::LatticeScale(lm_scale, acoustic_scale);
    for (; !clat_reader.Done(); clat_reader.Next()) {
      std::string key = clat_reader.Key();
      KALDI_LOG << "Processing " << key;
      CompactLattice &clat = clat_reader.Value();
      ScaleLattice(scale, &clat);

      MinimumBayesRisk mbr(clat, mbr_opts);
      const auto &sausages = mbr.GetSausageStats();

      fst::StdVectorFst fst;
      fst.AddState();
      fst.SetStart(0);
      int last_state = 0;
      for (const auto &sausage: sausages) {
        if (sausage.size() == 1 && sausage[0].first == 0) {
          continue;
        }

        fst.AddState();
        last_state++;

        for (const auto &arc: sausage) {
          fst::StdArc::Weight weight(-log(arc.second));
          fst.AddArc(last_state - 1, fst::StdArc(arc.first, arc.first, weight, last_state));
        }
      }
      fst.SetFinal(last_state, 0);
      
      KALDI_LOG << "Processed " << key;
      fst_writer.Write(key, fst); 
      num_done++;
    }

    KALDI_LOG << "Successfully aligned " << num_done << " lattices; "
              << num_err << " had errors.";
    return 0;
  } catch(const std::exception &e) {
    std::cerr << e.what();
    return -1;
  }
}
