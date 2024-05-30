#include "base/kaldi-common.h"
#include "util/common-utils.h"
#include "fstext/fstext-utils.h"
#include "fstext/kaldi-fst-io.h"
#include "util/kaldi-thread.h"
#include "decipherment-cascade.h"

template <class Arc>
class ExpectationTask {
  public:
    ExpectationTask(
        const DeciphermentCascade<Arc> *cascade,
        const Composer<Arc> *composer,
        const std::vector<fst::VectorFst<Arc>> *observations,
        Expectations<Arc> *task_expectations,
        Expectations<Arc> *total_expectations
    ): cascade_(cascade), composer_(composer), observations_(observations), task_expectations_(task_expectations), total_expectations_(total_expectations) { }

  void operator() () {
    for (const auto &observation: *observations_) {
      cascade_->ComputeExpectations(*composer_, observation, *task_expectations_);
    }
  }

  ~ExpectationTask() {
    total_expectations_->Add(*task_expectations_);
    delete task_expectations_;
  }

 private:
   const DeciphermentCascade<Arc> *cascade_;
   const Composer<Arc> *composer_;
   const std::vector<fst::VectorFst<Arc>> *observations_;
   Expectations<Arc> *task_expectations_;
   Expectations<Arc> *total_expectations_;

};




int main(int argc, char *argv[]) {
  try {
    using namespace kaldi;
    using namespace fst;
    typedef kaldi::int32 int32;
    typedef kaldi::uint64 uint64;

    const char *usage =
        "Usage:\n"
        " decipherment-learn <lex-filename> <ali-filename> <lm-filename> <source-rspecifier> <lex-wfilename> <ali-wfilename>\n";

    int num_src_syms = -1;
    int num_tgt_syms = -1;
    bool train_lex = true;
    bool train_ali = true;
    int num_iters = 10;
    int num_threads = 1;
    bool threeway = false;
    float prune_beam = 8;
    int steps_threshold = 5;

    ParseOptions po(usage);
    po.Register("num-source-symbols", &num_src_syms, "Number of source symbols");
    po.Register("num-target-symbols", &num_tgt_syms, "Number of target symbols");
    po.Register("train-lex", &train_lex, "Train lexical model?");
    po.Register("train-ali", &train_ali, "Train alignment model?");
    po.Register("num-iters", &num_iters, "Number of iterations");
    po.Register("num-threads", &num_threads, "Number of threads");
    po.Register("threeway", &threeway, "Use threeway composition?");
    po.Register("prune-beam", &prune_beam, "Prune beam");
    po.Register("steps-threshold", &steps_threshold, "Steps threshold");
    po.Read(argc, argv);

    if (num_src_syms == -1 || num_tgt_syms == -1) {
      KALDI_ERR << "num-source-symbols and num-target-symbols have to be larger than 0";
    }

    if (po.NumArgs() != 6) {
      po.PrintUsage();
      exit(1);
    }

    std::string lex_fst_filename = po.GetArg(1),
        ali_fst_filename = po.GetArg(2),
        lm_fst_rspecifier = po.GetArg(3),
        source_rspecifier = po.GetArg(4),
        lex_fst_wfilename = po.GetArg(5),
        ali_fst_wfilename = po.GetArg(6);

    SequentialTableReader<fst::VectorFstHolder> source_reader(source_rspecifier);
    std::vector<std::vector<fst::VectorFst<fst::LogArc>>> observation_per_job(num_threads);
    int i = 0;
    for (; !source_reader.Done(); source_reader.Next()) {
      const std::string key = source_reader.Key();
      fst::VectorFst<fst::LogArc> observation_fst;
      fst::Cast(source_reader.Value(), &observation_fst);
      fst::ArcSort(&observation_fst, fst::OLabelCompare<fst::LogArc>());
      observation_per_job[i++ % num_threads].push_back(observation_fst);
    }

    fst::StdVectorFst *lex_fst = fst::ReadFstKaldi(lex_fst_filename);
    fst::StdVectorFst *ali_fst = fst::ReadFstKaldi(ali_fst_filename);
    fst::StdVectorFst *lm_fst = fst::ReadFstKaldi(lm_fst_rspecifier);
    fst::Project(lm_fst, fst::PROJECT_INPUT);

    fst::VectorFst<fst::LogArc> log_lex_fst, log_ali_fst, log_lm_fst;
    fst::Cast(*lex_fst, &log_lex_fst);
    fst::Cast(*ali_fst, &log_ali_fst);
    fst::Cast(*lm_fst, &log_lm_fst);

    DeciphermentCascade<fst::LogArc> cascade(train_lex, train_ali, &log_lex_fst, &log_ali_fst);
    for (int iter = 0; iter < num_iters; iter++) {
      kaldi::Timer timer;
      std::cerr << "Iter " << iter;

      Expectations<fst::LogArc> total_expectations(num_src_syms, num_tgt_syms, ali_fst->NumStates(), lex_fst->NumStates());
      if (threeway) {
        total_expectations.Reset(1000);
      }


      Composer<fst::LogArc> *composer;
      if (threeway) {
        composer = new ThreewayComposer<fst::LogArc>(log_lex_fst, log_ali_fst, log_lm_fst, prune_beam, steps_threshold);
      } else {
        composer = new StandardComposer<fst::LogArc>(log_lex_fst, log_ali_fst, log_lm_fst);
      }

      TaskSequencerConfig config;
      config.num_threads = num_threads;
      TaskSequencer<ExpectationTask<fst::LogArc>> sequencer(config);
      for (const auto &observations: observation_per_job) {
        auto task_expectations = new Expectations<fst::LogArc>(num_src_syms, num_tgt_syms, ali_fst->NumStates(), lex_fst->NumStates());
        sequencer.Run(new ExpectationTask<fst::LogArc>(&cascade, composer, &observations, task_expectations, &total_expectations));
      }
      sequencer.Wait();

      std::cerr << " maximizing ";
      cascade.Maximize(total_expectations);
      cascade.GetAliFst(&log_ali_fst);
      cascade.GetLexFst(&log_lex_fst);

      std::cerr << " lex states " << log_lex_fst.NumStates() << " lex arcs " << fst::NumArcs(log_lex_fst);

      std::cerr << " likelihood " << total_expectations.Likelihood() << " done in " << timer.Elapsed() << " seconds" << std::endl;
      delete composer;
    }

    fst::Cast(log_ali_fst, ali_fst);
    ali_fst->Write(ali_fst_wfilename);

    fst::Cast(log_lex_fst, lex_fst);
    lex_fst->Write(lex_fst_wfilename);

    delete lex_fst;
    delete ali_fst;
    delete lm_fst;

    return 0;
  } catch(const std::exception &e) {
    std::cerr << e.what();
    return -1;
  }
}
