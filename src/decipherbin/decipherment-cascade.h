#include "composer.h"
#include "expectations.h"


template <class Arc>
class DeciphermentCascade {

  public:
    using Fst = typename fst::VectorFst<Arc>;
    using StateId = typename Arc::StateId;
    using Label = typename Arc::Label;
    using Weight = typename Arc::Weight;

    DeciphermentCascade(
        bool train_lex, bool train_ali, Fst *lex_fst, Fst *ali_fst
    ): train_lex_(train_lex), train_ali_(train_ali), lex_fst_(*lex_fst), ali_fst_(*ali_fst) {}

    void ComputeExpectations(const Composer<Arc> &composer, const Fst &ifst, Expectations<Arc> &expectations) const {
      Composition<Arc> *composition = composer.Compose(ifst);

      std::vector<Weight> alphas, betas;
      fst::ShortestDistance(composition->fst, &alphas, /*reverse=*/false);
      fst::ShortestDistance(composition->fst, &betas, /*reverse=*/true);
      if (betas.size() == 0) {
        KALDI_WARN << "Empty composition?";
        return;
      }

      Weight likelihood = betas[composition->fst.Start()];
      if (likelihood == Weight::Zero() || likelihood.Value() != likelihood.Value()) {
        KALDI_WARN << "Empty composition?";
        return;
      }

      expectations.AddLikelihood(likelihood);
      for (fst::StateIterator<Fst> siter(composition->fst); !siter.Done(); siter.Next()) {
        StateId state = siter.Value();
        StateId lex_state = composition->lex_state[state];
        StateId ali_state = composition->ali_state[state];
        Weight alpha = (state < alphas.size()) ? alphas[state] : Weight::Zero();

        for (fst::ArcIterator<Fst> aiter(composition->fst, state); !aiter.Done(); aiter.Next()) {
          auto arc = aiter.Value();
          Weight beta = (arc.nextstate < betas.size()) ? betas[arc.nextstate] : Weight::Zero();
          Weight posterior = fst::Divide(Times(Times(alpha, arc.weight), beta), likelihood);

          if (beta == Weight::Zero()) {
            continue;
          }

          expectations.AddObservation(lex_state, ali_state, arc.ilabel, arc.olabel, posterior);
        }
      }

      delete composition;
    }

    void Maximize(const Expectations<Arc> &expectations) {
      if (train_ali_) {
        for (fst::StateIterator<Fst> siter(ali_fst_); !siter.Done(); siter.Next()) {
          StateId state = siter.Value();
          for (fst::MutableArcIterator<Fst> aiter(&ali_fst_, state); !aiter.Done(); aiter.Next()) {
            auto arc = aiter.Value();
            arc.weight = expectations.MaximizeAli(state, arc.ilabel, arc.olabel);
            aiter.SetValue(arc);
          }
        }
      }

      if (train_lex_) {
        bool mutation = false;
        for (fst::StateIterator<Fst> siter(lex_fst_); !siter.Done(); siter.Next()) {
          StateId state = siter.Value();
          for (fst::MutableArcIterator<Fst> aiter(&lex_fst_, state); !aiter.Done(); aiter.Next()) {
            auto arc = aiter.Value();
            arc.weight = expectations.MaximizeLex(state, arc.ilabel, arc.olabel);
            arc.nextstate = (arc.weight == Weight::Zero()) ? lex_fst_.NumStates() : arc.nextstate;
            mutation |= arc.weight == Weight::Zero();
            aiter.SetValue(arc);
          }
        }

        if (mutation) {
          lex_fst_.AddState();
          fst::Connect(&lex_fst_);
          fst::ArcSort(&lex_fst_, fst::OLabelCompare<Arc>());
        }
      }
    }

    void GetAliFst(Fst *ofst) {
      *ofst = ali_fst_;
    }

    void GetLexFst(Fst *ofst) {
      *ofst = lex_fst_;
    }


  private:
    bool train_lex_, train_ali_;
    Fst lex_fst_, ali_fst_;

};
