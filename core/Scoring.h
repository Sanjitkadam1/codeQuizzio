#pragma once

#include "Question.h"

namespace cq {

struct ScoreBreakdown {
    int points = 0;
    int basePoints = 0;
    double speedMultiplier = 0.0;
    double streakMultiplier = 1.0;
    double parSeconds = 0.0;
};

// points = basePoints(difficulty) * speedMultiplier(time vs par) * streakMultiplier
// All tuning constants live here so balancing the game is a one-file change.
class Scoring {
public:
    // Time a competent player needs: reading time + typing time + a bit extra
    // for harder questions. Based on the shortest accepted answer.
    double parSeconds(const Question& q) const;

    // 1.5 for an instant answer, 1.0 at par, decaying to a 0.25 floor.
    double speedMultiplier(double elapsedSeconds, double parSeconds) const;

    // +5% per consecutive correct answer, capped at +50%.
    double streakMultiplier(int streak) const;

    // `streakAfter` is the streak length including this correct answer.
    ScoreBreakdown score(const Question& q, double elapsedSeconds, int streakAfter) const;
};

}  // namespace cq
