#include "Scoring.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace cq {

namespace {
constexpr double kReadSeconds = 4.0;
constexpr double kSecondsPerChar = 0.35;
constexpr double kSecondsPerDifficulty = 0.5;
constexpr double kMaxSpeedBonus = 0.5;
constexpr double kSpeedFloor = 0.25;
constexpr int kBasePointsPerDifficulty = 10;
constexpr double kStreakStep = 0.05;
constexpr int kStreakCap = 10;
}  // namespace

double Scoring::parSeconds(const Question& q) const {
    std::size_t shortest = q.answers.empty() ? 0 : q.answers.front().size();
    for (const auto& a : q.answers) shortest = std::min(shortest, a.size());
    return kReadSeconds + kSecondsPerChar * static_cast<double>(shortest) +
           kSecondsPerDifficulty * q.difficulty;
}

double Scoring::speedMultiplier(double elapsed, double par) const {
    if (par <= 0.0) return 1.0;
    elapsed = std::max(elapsed, 0.0);
    if (elapsed <= par) return 1.0 + kMaxSpeedBonus * (1.0 - elapsed / par);
    // Linear fall from 1.0 at par to the floor at 3x par.
    double t = (elapsed - par) / (2.0 * par);
    return std::max(kSpeedFloor, 1.0 - (1.0 - kSpeedFloor) * t);
}

double Scoring::streakMultiplier(int streak) const {
    return 1.0 + kStreakStep * std::clamp(streak, 0, kStreakCap);
}

ScoreBreakdown Scoring::score(const Question& q, double elapsed, int streakAfter) const {
    ScoreBreakdown b;
    b.basePoints = kBasePointsPerDifficulty * q.difficulty;
    b.parSeconds = parSeconds(q);
    b.speedMultiplier = speedMultiplier(elapsed, b.parSeconds);
    b.streakMultiplier = streakMultiplier(streakAfter);
    b.points = static_cast<int>(std::lround(b.basePoints * b.speedMultiplier * b.streakMultiplier));
    return b;
}

}  // namespace cq
