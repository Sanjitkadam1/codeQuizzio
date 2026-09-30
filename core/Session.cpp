#include "Session.h"

#include <algorithm>
#include <stdexcept>

namespace cq {

Session::Session(const QuestionBank& bank, std::unique_ptr<Selector> selector,
                 Scoring scoring, Strictness strictness)
    : bank_(bank), selector_(std::move(selector)), scoring_(scoring), strictness_(strictness) {}

const Question* Session::nextQuestion() {
    current_ = selector_->next(bank_);
    return current_;
}

const Question& Session::requireCurrent() const {
    if (current_ == nullptr) throw std::logic_error("no current question");
    return *current_;
}

AttemptResult Session::submit(const std::string& input, double elapsedSeconds) {
    const Question& q = requireCurrent();
    AttemptResult r;
    r.question = &q;

    if (isCorrect(q, input, strictness_)) {
        ++streak_;
        bestStreak_ = std::max(bestStreak_, streak_);
        ++correct_;
        r.outcome = Outcome::Correct;
        r.breakdown = scoring_.score(q, elapsedSeconds, streak_);
        r.points = r.breakdown.points;
        score_ += r.points;
        selector_->record(q, Outcome::Correct, r.breakdown.speedMultiplier);
    } else {
        streak_ = 0;
        ++wrong_;
        r.outcome = Outcome::Wrong;
        selector_->record(q, Outcome::Wrong, 0.0);
    }
    current_ = nullptr;
    return r;
}

AttemptResult Session::skip() {
    const Question& q = requireCurrent();
    streak_ = 0;
    ++skipped_;
    selector_->record(q, Outcome::Skipped, 0.0);

    AttemptResult r;
    r.outcome = Outcome::Skipped;
    r.question = &q;
    current_ = nullptr;
    return r;
}

}  // namespace cq
