#pragma once

#include <cstdint>

#include "Progress.h"
#include "SelectorConfig.h"

namespace cq {

// How well the player knows one question, from 0 (not at all) to 1 (cold).
// Pure function of the saved stats, so it is easy to test and to reason about:
//
//   accuracy   recency-weighted share of correct answers (newest counts most)
//   speed      how the average correct time compares to par (1 = at/under par)
//   mastery    accuracy x ((1 - speedWeight) + speedWeight x speed)
//   confidence pulled toward masteryPrior while there are few attempts
//   forgetting halves every forgetHalfLifeDays since the question was last seen
//
// `stats` may be null (never attempted), which yields masteryPrior.
double mastery(const QuestionStats* stats, double parSeconds, std::int64_t now,
               const SelectorConfig& config);

}  // namespace cq
