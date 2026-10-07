#pragma once

#include "Outcome.h"
#include "Question.h"
#include "QuestionBank.h"

namespace cq {

// Decides which question comes next. Kept as an interface so the algorithm can
// be swapped (or unit-tested with a scripted fake) without touching Session.
class Selector {
public:
    virtual ~Selector() = default;

    // Returns nullptr only when the bank is empty.
    virtual const Question* next(const QuestionBank& bank) = 0;

    // Feedback after each question. speedMultiplier is 0 for non-correct outcomes.
    virtual void record(const Question& q, Outcome outcome, double speedMultiplier) = 0;
};

}  // namespace cq
