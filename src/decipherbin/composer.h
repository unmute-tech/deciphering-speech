#ifndef DECIPHERMENT_COMPOSER_H_
#define DECIPHERMENT_COMPOSER_H_

#include "threeway_compose.h"

template <class Arc>
struct Composition {

  fst::VectorFst<Arc> fst;
  std::vector<typename Arc::StateId> lex_state;
  std::vector<typename Arc::StateId> ali_state;

};

template <class Arc>
class Composer {

  public:
    virtual Composition<Arc>* Compose(const fst::VectorFst<Arc> &ifst) const = 0;
    virtual ~Composer() { };

};

template <class Arc>
class StandardComposer: public Composer<Arc> {

  public:
    using Fst = typename fst::VectorFst<Arc>;
    using ComposeFst = typename fst::ComposeFst<Arc>;
    using SM = typename fst::SortedMatcher<fst::Fst<Arc>>;
    using ComposeFstOptions = typename fst::ComposeFstImplOptions<SM, SM>;
    using StateTable = typename fst::GenericComposeStateTable<Arc, fst::IntegerFilterState<signed char>>;
    using StateId = typename Arc::StateId;

    StandardComposer(const Fst &lex_fst, const Fst &ali_fst, const Fst &lm_fst) {
      Fst la_fst;
      state_table_la_ = Compose(lex_fst, ali_fst, &la_fst);
      state_table_lag_ = Compose(la_fst, lm_fst, &lag_fst_);
      fst::ArcSort(&lag_fst_, fst::ILabelCompare<Arc>());
    }

    ~StandardComposer() {
      delete state_table_la_;
      delete state_table_lag_;
    }

    Composition<Arc>* Compose(const Fst &ifst) const {
      Composition<Arc> *composition = new Composition<Arc>();
      StateTable *state_table = Compose(ifst, lag_fst_, &(composition->fst));

      composition->lex_state.resize(composition->fst.NumStates());
      composition->ali_state.resize(composition->fst.NumStates());

      for (fst::StateIterator<Fst> siter(composition->fst); !siter.Done(); siter.Next()) {
        StateId state = siter.Value();
        StateId lag_state = state_table->Tuple(state).StateId2();
        StateId la_state = state_table_lag_->Tuple(lag_state).StateId1();

        composition->lex_state[state] = state_table_la_->Tuple(la_state).StateId1();
        composition->ali_state[state] = state_table_la_->Tuple(la_state).StateId2();
      }

      delete state_table;
      return composition;
    }

  private:

    StateTable* Compose(const Fst &fst1, const Fst &fst2, Fst *ofst) const {
      ComposeFstOptions opts;
      opts.gc_limit = 0;
      opts.own_state_table = false;
      opts.state_table = new StateTable(fst1, fst2);
      *ofst = ComposeFst(fst1, fst2, opts);
      return opts.state_table;
    }

    Fst lag_fst_;
    StateTable *state_table_la_, *state_table_lag_;

};


template <class Arc>
class ThreewayComposer: public Composer<Arc> {

  public:
    using Fst = typename fst::VectorFst<Arc>;
    using ComposeFst = typename fst::ComposeFst<fst::StdArc>;
    using SM = typename fst::SortedMatcher<fst::Fst<fst::StdArc>>;
    using ComposeFstOptions = typename fst::ComposeFstImplOptions<SM, SM>;
    using StateTable = typename fst::GenericComposeStateTable<fst::StdArc, fst::IntegerFilterState<signed char>>;
    using ThreewayStateTable = typename fst::ThreeWayComposeStateTable<fst::StdArc>;
    using StateId = typename Arc::StateId;

    ThreewayComposer(
        const Fst &log_lex_fst, const Fst &log_ali_fst, const Fst &log_lm_fst,
        float prune_beam, int steps_threshold
    ): prune_beam_(prune_beam), steps_threshold_(steps_threshold) {
      fst::StdVectorFst lex_fst, ali_fst;
      fst::Cast(log_lex_fst, &lex_fst);
      fst::Cast(log_ali_fst, &ali_fst);
      fst::Cast(log_lm_fst, &lm_fst_);
      state_table_la_ = Compose(lex_fst, ali_fst, &la_fst_);
    }

    ~ThreewayComposer() {
      delete state_table_la_;
    }

    Composition<Arc>* Compose(const Fst &log_ifst) const {
      fst::StdVectorFst ifst;
      fst::Cast(log_ifst, &ifst);
      fst::ThreeWayComposition<fst::StdArc> tc(ifst, la_fst_, lm_fst_, steps_threshold_, prune_beam_, -1);

      Composition<Arc> *composition = new Composition<Arc>();
      fst::Cast(tc.GetFst(), &composition->fst);
      ThreewayStateTable state_table = tc.GetStateTable();

      composition->lex_state.resize(composition->fst.NumStates());
      composition->ali_state.resize(composition->fst.NumStates());

      for (fst::StateIterator<Fst> siter(composition->fst); !siter.Done(); siter.Next()) {
        StateId state = siter.Value();
        StateId la_state = state_table.Tuple(state).StateId2();

        composition->lex_state[state] = state_table_la_->Tuple(la_state).StateId1();
        composition->ali_state[state] = state_table_la_->Tuple(la_state).StateId2();
      }

      return composition;
    }

  private:

    StateTable* Compose(const fst::StdVectorFst &fst1, const fst::StdVectorFst &fst2, fst::StdVectorFst *ofst) const {
      ComposeFstOptions opts;
      opts.gc_limit = 0;
      opts.own_state_table = false;
      opts.state_table = new StateTable(fst1, fst2);
      *ofst = ComposeFst(fst1, fst2, opts);
      return opts.state_table;
    }

    float prune_beam_;
    int steps_threshold_;
    fst::StdVectorFst la_fst_, lm_fst_;
    StateTable *state_table_la_;
};

#endif  // DECIPHERMENT_COMPOSER_H_
