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
        if (progress_) progress_->recordAnswer(q.id, Outcome::Correct, elapsedSeconds, clock_());
    } else {
        streak_ = 0;
        ++wrong_;
        r.outcome = Outcome::Wrong;
        selector_->record(q, Outcome::Wrong, 0.0);
        if (progress_) progress_->recordAnswer(q.id, Outcome::Wrong, elapsedSeconds, clock_());
    }
    current_ = nullptr;
    return r;
}

AttemptResult Session::skip() {
    const Question& q = requireCurrent();
    streak_ = 0;
    ++skipped_;
    selector_->record(q, Outcome::Skipped, 0.0);
    if (progress_) progress_->recordAnswer(q.id, Outcome::Skipped, 0.0, clock_());

    AttemptResult r;
    r.outcome = Outcome::Skipped;
    r.question = &q;
    current_ = nullptr;
    return r;
}

void Session::attachProgress(Progress* progress, Clock clock) {
    progress_ = progress;
    clock_ = clock ? std::move(clock) : Clock(systemSeconds);
}

SessionSummary Session::finish() {
    if (finished_) return summary_;
    finished_ = true;

    summary_.record.endedAt = clock_ ? clock_() : systemSeconds();
    summary_.record.score = score_;
    summary_.record.correct = correct_;
    summary_.record.wrong = wrong_;
    summary_.record.skipped = skipped_;
    summary_.record.bestStreak = bestStreak_;

    if (progress_ && summary_.record.answered() > 0) {
        const RecordResult r = progress_->recordSession(summary_.record);
        summary_.recorded = true;
        summary_.newHighScore = r.newHighScore;
        summary_.newBestStreak = r.newBestStreak;
    }
    return summary_;
}

}  // namespace cq
