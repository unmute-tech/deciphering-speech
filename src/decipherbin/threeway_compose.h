#ifndef DECIPHERMENT_THREEWAY_COMPOSE_
#define DECIPHERMENT_THREEWAY_COMPOSE_

#include "fstext/fstext-utils.h"
#include "table.h"


namespace fst {

template <typename S>
class ThreeWayComposeStateTuple{
  public:
    using StateId = S;

    ThreeWayComposeStateTuple()
        : state1_(kNoStateId), state2_(kNoStateId), state3_(kNoStateId) {}

    ThreeWayComposeStateTuple(StateId s1, StateId s2, StateId s3)
        : state1_(s1), state2_(s2), state3_(s3) {}

    StateId StateId1() const { return state1_; }

    StateId StateId2() const { return state2_; }

    StateId StateId3() const { return state3_; }

    friend bool operator==(const ThreeWayComposeStateTuple &x,
                           const ThreeWayComposeStateTuple &y) {
      return (x.state1_ == y.state1_) && (x.state2_ == y.state2_) && (x.state3_ == y.state3_);
    }

    size_t Hash() const {
      size_t h1 = static_cast<size_t>(state1_) & 16777215; // 2^24 - 1
      size_t h2 = static_cast<size_t>(state2_) & 255; // 2^8 - 1
      size_t h3 = static_cast<size_t>(state3_) & 4294967295; // 2^32 - 1

      return (((h1 << 8) | h2) << 32) | h3;
    }

  private:
    StateId state1_, state2_, state3_;
};

template <typename Arc,
          typename StateTuple = ThreeWayComposeStateTuple<typename Arc::StateId>,
          typename StateTable = CompactHashStateTable<StateTuple, ComposeHash<StateTuple>>>
class ThreeWayComposeStateTable : public StateTable {
  public:
    using StateId = typename Arc::StateId;

    ThreeWayComposeStateTable(const Fst<Arc> &fst1, const Fst<Arc> &fst2, const Fst<Arc> &fst3) {}

    ThreeWayComposeStateTable(const Fst<Arc> &fst1, const Fst<Arc> &fst2, const Fst<Arc> &fst3, size_t table_size)
        : StateTable(table_size) {}

   constexpr bool Error() const { return false; }

  private:
    ThreeWayComposeStateTable &operator=(const ThreeWayComposeStateTable &table) = delete;
};

template <typename Arc>
struct BeamSearchStateEquivClass {
  public:
    explicit BeamSearchStateEquivClass(const ThreeWayComposeStateTable<Arc> &state_table)
      : state_table_(state_table) {}

    int operator()(int s) const {
      return state_table_.Tuple(s).StateId1();
    }

  private:
    const ThreeWayComposeStateTable<Arc> &state_table_;
};

template <typename Arc>
class DenseMatcher {
  using StateId = typename Arc::StateId;
  using Label = typename Arc::Label;

  public:
    DenseMatcher(const VectorFst<Arc> &fst, const Arc &default_arc)
      : table_(fst.NumStates(), HighestNumberedInputSymbol(fst) + 1, HighestNumberedOutputSymbol(fst) + 1, default_arc) {
      for (StateIterator<Fst<Arc>> siter(fst); !siter.Done(); siter.Next()) {
        const StateId &s = siter.Value();
        for (ArcIterator<Fst<Arc>> aiter(fst, s); !aiter.Done(); aiter.Next()) {
          const Arc &arc = aiter.Value();
          table_(s, arc.ilabel, arc.olabel) = arc;
        }
      }
    }

    const Arc &GetArc(StateId state, Label ilabel, Label olabel) const {
      return table_(state, ilabel, olabel);
    }

  private:
    Table<Arc> table_;
};

template<class Arc>
class ThreeWayComposition {
  using StateId = typename Arc::StateId;
  using Weight = typename Arc::Weight;
  typedef NaturalPruneQueue<PruneNaturalShortestFirstQueue<StateId, Weight>, Weight, BeamSearchStateEquivClass<Arc>> Queue;
  typedef ThreeWayComposeStateTuple<StateId> StateTuple;

  public:

    ThreeWayComposition(const VectorFst<Arc> &fst1, const VectorFst<Arc> &fst2, const VectorFst<Arc> &fst3, int steps_threshold, float prune_beam, int max_paths)
        : fst1_(fst1), fst2_(fst2), fst3_(fst3),
          dm2_(fst2, Arc(kNoLabel, kNoLabel, Weight::Zero(), -1)),
          state_table_(fst1_, fst2_, fst3_),
          equivalence_class_(state_table_),
          queue_(distance_, new PruneNaturalShortestFirstQueue<StateId, Weight>(distance_, steps_threshold), equivalence_class_, prune_beam),
          max_paths_(max_paths), num_paths_(0), best_final_distance_(Weight::Zero()) {
      Compose();
    }

    const VectorFst<Arc> &GetFst() const {
      return ofst_;
    }

    const ThreeWayComposeStateTable<Arc> &GetStateTable() const {
      return state_table_;
    }

  private:

    void Compose() {
      assert(fst1_.Properties(kOLabelSorted, true) == kOLabelSorted);
      assert(fst3_.Properties(kILabelSorted, true) == kILabelSorted);

      bool fst1_has_output_epsilons = fst1_.Properties(fst::kOEpsilons, true) != 0;
      bool fst3_has_input_epsilons = fst3_.Properties(fst::kIEpsilons, true) != 0;

      ProcessStart();
      while (!queue_.Empty()) {
        StateId state = queue_.Head();
        queue_.Dequeue();

        const StateTuple tuple = state_table_.Tuple(state);
        if (max_paths_ == 1 && less_(best_final_distance_, distance_[state])) {
          break;
        }

        // TODO: check whether this can be optimized in any other way? allocating fewer arcs?
        if (fst1_has_output_epsilons) {
          HandleOutputEpsilonsInFst1(state, tuple);
        }

        if (fst3_has_input_epsilons) {
          HandleInputEpsilonsInFst3(state, tuple);
        }

        HandleOutputEpsilonsInFst2(state, tuple);
        HandleInputEpsilonsInFst2(state, tuple);
        HandleInputOutputEpsilonsInFst2(state, tuple);
        HandleNonEpsilonArcs(state, tuple);
      }
    }

    void ProcessStart() {
      ofst_.DeleteStates();
      ofst_.AddState();
      ofst_.SetStart(0);
      distance_.push_back(Weight::One());
      queue_.Enqueue(state_table_.FindState({fst1_.Start(), fst2_.Start(), fst3_.Start()}));
    }

    void HandleOutputEpsilonsInFst1(StateId state, StateTuple tuple) {
      for (ArcIterator<Fst<Arc>> aiter1(fst1_, tuple.StateId1()); !aiter1.Done(); aiter1.Next()) {
        const Arc &arc1 = aiter1.Value();
        if (arc1.olabel > 0) {
          break;
        }
        const Arc arc2(0, 0, Arc::Weight::One(), tuple.StateId2());
        const Arc arc3(0, 0, Arc::Weight::One(), tuple.StateId3());

        AddArc(state, arc1, arc2, arc3);
      }
    }

    void HandleOutputEpsilonsInFst2(StateId state, StateTuple tuple) {
      for (ArcIterator<Fst<Arc>> aiter1(fst1_, tuple.StateId1()); !aiter1.Done(); aiter1.Next()) {
        const Arc &arc1 = aiter1.Value();
        const Arc arc3(0, 0, Arc::Weight::One(), tuple.StateId3());
        if (arc1.olabel == 0) {
          continue;
        }

        const Arc &arc2 = dm2_.GetArc(tuple.StateId2(), arc1.olabel, arc3.ilabel);
        AddArc(state, arc1, arc2, arc3);
      }
    }

    void HandleInputEpsilonsInFst2(StateId state, StateTuple tuple) {
      for (ArcIterator<Fst<Arc>> aiter3(fst3_, tuple.StateId3()); !aiter3.Done(); aiter3.Next()) {
        const Arc arc1(0, 0, Arc::Weight::One(), tuple.StateId1());
        const Arc &arc3 = aiter3.Value();
        if (arc3.ilabel == 0) {
          continue;
        }

        const Arc &arc2 = dm2_.GetArc(tuple.StateId2(), arc1.olabel, arc3.ilabel);
        AddArc(state, arc1, arc2, arc3);
      }
    }

    void HandleInputOutputEpsilonsInFst2(StateId state, StateTuple tuple) {
      const Arc arc1(0, 0, Arc::Weight::One(), tuple.StateId1());
      const Arc arc3(0, 0, Arc::Weight::One(), tuple.StateId3());
      const Arc &arc2 = dm2_.GetArc(tuple.StateId2(), 0, 0);
      AddArc(state, arc1, arc2, arc3);
    }

    void HandleInputEpsilonsInFst3(StateId state, StateTuple tuple) {
      for (ArcIterator<Fst<Arc>> aiter3(fst3_, tuple.StateId3()); !aiter3.Done(); aiter3.Next()) {
        const Arc &arc3 = aiter3.Value();
        if (arc3.ilabel > 0) {
          break;
        }
        const Arc arc1(0, 0, Arc::Weight::One(), tuple.StateId1());
        const Arc arc2(0, 0, Arc::Weight::One(), tuple.StateId2());

        AddArc(state, arc1, arc2, arc3);
      }
    }

    void HandleNonEpsilonArcs(StateId state, StateTuple tuple) {
      for (ArcIterator<Fst<Arc>> aiter1(fst1_, tuple.StateId1()); !aiter1.Done(); aiter1.Next()) {
        const Arc &arc1 = aiter1.Value();
        if (arc1.olabel == 0) {
          continue;
        }

        for (ArcIterator<Fst<Arc>> aiter3(fst3_, tuple.StateId3()); !aiter3.Done(); aiter3.Next()) {
          const Arc &arc3 = aiter3.Value();
          if (arc3.ilabel == 0) {
            continue;
          }

          const Arc &arc2 = dm2_.GetArc(tuple.StateId2(), arc1.olabel, arc3.ilabel);
          AddArc(state, arc1, arc2, arc3);
        }
      }
    }

    void AddArc(StateId state, const Arc &arc1, const Arc &arc2, const Arc &arc3) {
      if (arc2.ilabel == kNoLabel && arc2.olabel == kNoLabel) {
        return;
      }

      StateId nextstate = state_table_.FindState({arc1.nextstate, arc2.nextstate, arc3.nextstate});
      Weight weight = Times(arc1.weight, Times(arc2.weight, arc3.weight));
      Weight new_distance = Times(distance_[state], weight);
      Weight final_weight = Times(fst1_.Final(arc1.nextstate), Times(fst2_.Final(arc2.nextstate), fst3_.Final(arc3.nextstate)));

      if (nextstate == ofst_.NumStates()) {
        distance_.push_back(new_distance);
        queue_.Enqueue(nextstate);
        ofst_.AddState();
      } else if (less_(new_distance, distance_[nextstate])) {
        distance_[nextstate] = new_distance;
        queue_.Update(nextstate);
      }

      if (final_weight != Weight::Zero()) {
        ofst_.SetFinal(nextstate, final_weight);
        if (less_(Times(new_distance, final_weight), best_final_distance_)) {
          best_final_distance_ = Times(new_distance, final_weight);
        }
      }

      ofst_.AddArc(state, Arc(arc1.ilabel, arc3.olabel, weight, nextstate));
    }

    VectorFst<Arc> fst1_, fst2_, fst3_;
    DenseMatcher<Arc> dm2_;
    VectorFst<Arc> ofst_;

    std::vector<Weight> distance_;
    ThreeWayComposeStateTable<Arc> state_table_;
    BeamSearchStateEquivClass<Arc> equivalence_class_;
    Queue queue_;

    int max_paths_, num_paths_;
    Weight best_final_distance_;
    NaturalLess<Weight> less_;

};

}

#endif  // DECIPHERMENT_THREEWAY_COMPOSE_
