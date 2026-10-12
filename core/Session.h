#pragma once

#include <memory>
#include <string>

#include "AnswerChecker.h"
#include "Clock.h"
#include "Progress.h"
#include "Question.h"
#include "QuestionBank.h"
#include "Scoring.h"
#include "Selector.h"

namespace cq {

struct AttemptResult {
    Outcome outcome = Outcome::Skipped;
    int points = 0;
    ScoreBreakdown breakdown;  // only meaningful when outcome == Correct
    const Question* question = nullptr;
};

// What finish() reports about the session that just ended.
struct SessionSummary {
    SessionRecord record;
    bool recorded = false;  // false if nothing was answered or no Progress is attached
    bool newHighScore = false;
    bool newBestStreak = false;
};

// One play session. The UI owns the clock: it starts timing when the question
// is shown, excludes paused time, and passes the elapsed seconds in. That keeps
// the core deterministic and trivially testable.
class Session {
public:
    // `bank` must outlive the session.
    Session(const QuestionBank& bank, std::unique_ptr<Selector> selector,
            Scoring scoring = {}, Strictness strictness = Strictness::IgnoreWhitespace);

    // Picks the next question (nullptr if the bank is empty). Any question
    // still pending is abandoned without being recorded.
    const Question* nextQuestion();
    const Question* current() const { return current_; }

    // Both require a current question (std::logic_error otherwise) and clear it.
    AttemptResult submit(const std::string& input, double elapsedSeconds);
    AttemptResult skip();

    // Start recording answers into `progress` (must outlive the session). An
    // empty clock means the system clock.
    void attachProgress(Progress* progress, Clock clock = {});

    // Ends the session: logs it in the attached Progress (if anything was
    // answered). Safe to call more than once; only the first call records.
    SessionSummary finish();

    int score() const { return score_; }
    int streak() const { return streak_; }
    int bestStreak() const { return bestStreak_; }
    int correctCount() const { return correct_; }
    int wrongCount() const { return wrong_; }
    int skippedCount() const { return skipped_; }

private:
    const Question& requireCurrent() const;

    const QuestionBank& bank_;
    std::unique_ptr<Selector> selector_;
    Scoring scoring_;
    Strictness strictness_;
    const Question* current_ = nullptr;
    int score_ = 0, streak_ = 0, bestStreak_ = 0;
    int correct_ = 0, wrong_ = 0, skipped_ = 0;

    Progress* progress_ = nullptr;
    Clock clock_;
    bool finished_ = false;
    SessionSummary summary_;
};

}  // namespace cq
