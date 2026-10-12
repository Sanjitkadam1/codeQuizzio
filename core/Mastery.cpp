#include "Mastery.h"

#include <algorithm>
#include <cmath>

namespace cq {

namespace {

constexpr double kSecondsPerDay = 86400.0;

// Share of correct answers, newest outcomes weighted most.
double accuracy(const QuestionStats& s, const SelectorConfig& cfg) {
    double right = 0.0;
    double total = 0.0;
    if (!s.recent.empty()) {
        double weight = 1.0;
        for (auto it = s.recent.rbegin(); it != s.recent.rend(); ++it) {
            switch (*it) {
                case 'c':
                    right += weight;
                    total += weight;
                    break;
                case 'w':
                    total += weight;
                    break;
                default:  // 's'
                    total += weight * cfg.skipPenalty;
                    break;
            }
            weight *= cfg.recencyDecay;
        }
    } else {
        // Saves from before 'recent' existed: fall back to the lifetime totals.
        right = s.correct;
        total = s.correct + s.misses + s.skips * cfg.skipPenalty;
    }
    return total > 0.0 ? right / total : 0.0;
}

}  // namespace

double mastery(const QuestionStats* stats, double parSeconds, std::int64_t now,
               const SelectorConfig& cfg) {
    if (stats == nullptr || stats->attempts <= 0) return cfg.masteryPrior;

    const double acc = accuracy(*stats, cfg);

    double speed = 0.0;
    if (stats->correct > 0 && parSeconds > 0.0) {
        const double avg = stats->averageCorrectTime();
        speed = avg > 0.0 ? std::clamp(parSeconds / avg, 0.0, 1.0) : 1.0;
    }
    double m = acc * ((1.0 - cfg.speedWeight) + cfg.speedWeight * speed);

    // Few attempts -> trust the prior; many -> trust the evidence.
    const double n = stats->attempts;
    const double k = cfg.confidenceAttempts;
    m = (n + k) > 0.0 ? (m * n + cfg.masteryPrior * k) / (n + k) : m;

    if (cfg.forgetHalfLifeDays > 0.0 && stats->lastSeen > 0 && now > stats->lastSeen) {
        const double days = static_cast<double>(now - stats->lastSeen) / kSecondsPerDay;
        m *= std::pow(0.5, days / cfg.forgetHalfLifeDays);
    }
    return std::clamp(m, 0.0, 1.0);
}

}  // namespace cq
