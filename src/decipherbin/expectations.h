#ifndef DECIPHERMENT_EXPECTATIONS_H_
#define DECIPHERMENT_EXPECTATIONS_H_

#include "table.h"

template <class Arc>
class Expectations {

  public:
    using StateId = typename Arc::StateId;
    using Label = typename Arc::Label;
    using Weight = typename Arc::Weight;
    using Log64Weight = fst::Log64Weight;

    Expectations(
        int num_src_syms, int num_tgt_syms, int num_ali_states, int num_lex_states
    ): num_src_syms_(num_src_syms),
       num_tgt_syms_(num_tgt_syms),
       total_likelihood_(Log64Weight::One()),
       ali_expectations_(num_ali_states, 3, Log64Weight::Zero()),
       ali_expectations_sum_(num_ali_states, Log64Weight::Zero()),
       lex_expectations_(num_lex_states, num_src_syms, num_tgt_syms + 1, Log64Weight::Zero()),
       lex_expectations_sum_(num_lex_states, num_tgt_syms + 1, Log64Weight::Zero()) {}

    void Reset(Log64Weight constant = Log64Weight::Zero()) {
      ali_expectations_.SetToConstant(constant);
      ali_expectations_sum_.SetToConstant(constant.Value() + log(3));
      lex_expectations_.SetToConstant(constant);
      lex_expectations_sum_.SetToConstant(constant.Value() + log(num_src_syms_ - 2));
    }

    void AddLikelihood(Weight likelihood) {
      total_likelihood_ = fst::Times(total_likelihood_, to_log64(likelihood));
    }

    void AddObservation(StateId lex_state, StateId ali_state, Label ilabel, Label olabel, Weight gamma) {
      bool is_epsilon_arc = ilabel == 0 && olabel == 0;
      bool is_silence_arc = olabel == 1;
      if (is_epsilon_arc || is_silence_arc) {
        return;
      }

      bool is_insertion = ilabel == 0;
      if (is_insertion) {
        ali_expectations_(ali_state, INSERTION) = fst::Plus(ali_expectations_(ali_state, INSERTION), to_log64(gamma));
        ali_expectations_sum_(ali_state) = fst::Plus(ali_expectations_sum_(ali_state), to_log64(gamma));
      }

      bool is_deletion = olabel == 0;
      if (is_deletion) {
        ali_expectations_(ali_state, DELETION) = fst::Plus(ali_expectations_(ali_state, DELETION), to_log64(gamma));
        ali_expectations_sum_(ali_state) = fst::Plus(ali_expectations_sum_(ali_state), to_log64(gamma));

        lex_expectations_(lex_state, ilabel, num_tgt_syms_) = fst::Plus(lex_expectations_(lex_state, ilabel, num_tgt_syms_), to_log64(gamma));
        lex_expectations_sum_(lex_state, num_tgt_syms_) = fst::Plus(lex_expectations_sum_(lex_state, num_tgt_syms_), to_log64(gamma));
      }

      bool is_substitution = !is_insertion && !is_deletion;
      if (is_substitution) {
        ali_expectations_(ali_state, MATCH) = fst::Plus(ali_expectations_(ali_state, MATCH), to_log64(gamma));
        ali_expectations_sum_(ali_state) = fst::Plus(ali_expectations_sum_(ali_state), to_log64(gamma));

        lex_expectations_(lex_state, ilabel, olabel) = fst::Plus(lex_expectations_(lex_state, ilabel, olabel), to_log64(gamma));
        lex_expectations_sum_(lex_state, olabel) = fst::Plus(lex_expectations_sum_(lex_state, olabel), to_log64(gamma));
      }
    }

    Weight MaximizeAli(StateId state, Label ilabel, Label olabel) const {
      bool is_epsilon_arc = ilabel == 0 && olabel == 0;
      bool is_silence_arc = olabel == 1;
      if (is_epsilon_arc || is_silence_arc) {
        return Weight::One();
      }

      bool is_insertion = ilabel == 0;
      if (is_insertion) {
        return from_log64(fst::Divide(ali_expectations_(state, INSERTION), ali_expectations_sum_(state)));
      }

      bool is_deletion = ilabel == num_tgt_syms_;
      if (is_deletion) {
        return from_log64(fst::Divide(ali_expectations_(state, DELETION), ali_expectations_sum_(state)));
      }

      return from_log64(fst::Divide(ali_expectations_(state, MATCH), ali_expectations_sum_(state)));
    }

    Weight MaximizeLex(StateId state, Label ilabel, Label olabel) const {
      bool is_silence_arc = ilabel == 1 && olabel == 1;
      if (is_silence_arc) {
        return Weight::One();
      }

      bool is_expectation_zero = lex_expectations_(state, ilabel, olabel) == Log64Weight::Zero();
      if (is_expectation_zero) {
        return Weight::Zero();
      }

      return from_log64(fst::Divide(lex_expectations_(state, ilabel, olabel), lex_expectations_sum_(state, olabel)));
    }

    Weight LexOccupationCount(StateId state, Label ilabel, Label olabel) const {
      return from_log64(lex_expectations_(state, ilabel, olabel));
    }

    void Add(const Expectations &other) {
      auto lambda = [](const Log64Weight &a, const Log64Weight &b) { return fst::Plus(a, b); };
      ali_expectations_.Add(other.ali_expectations_, lambda);
      ali_expectations_sum_.Add(other.ali_expectations_sum_, lambda);
      lex_expectations_.Add(other.lex_expectations_, lambda);
      lex_expectations_sum_.Add(other.lex_expectations_sum_, lambda);
      total_likelihood_ = fst::Times(total_likelihood_, other.total_likelihood_);
    }

    Weight Likelihood() {
      return from_log64(total_likelihood_);
    }

  private:
    const int INSERTION = 0;
    const int DELETION = 1;
    const int MATCH = 2;

    int num_src_syms_, num_tgt_syms_;
    Log64Weight total_likelihood_;
    Table<Log64Weight> ali_expectations_, ali_expectations_sum_, lex_expectations_, lex_expectations_sum_;
    fst::WeightConvert<Weight, Log64Weight> to_log64;
    fst::WeightConvert<Log64Weight, Weight> from_log64;

};


#endif  // DECIPHERMENT_EXPECTATIONS_H_
