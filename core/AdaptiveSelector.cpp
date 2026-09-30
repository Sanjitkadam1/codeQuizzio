#include "AdaptiveSelector.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace cq {

namespace {
constexpr long kRecentWindow = 5;    // no repeats within this many questions
constexpr long kRequeueDelay = 3;    // missed questions return after this many
constexpr double kSigma = 1.5;       // how tightly questions cluster around target
constexpr double kMissWeight = 0.5;  // extra weight per previous miss
constexpr double kUnseenBoost = 1.5;
constexpr double kFastStep = 0.3;
constexpr double kSlowStep = 0.1;
constexpr double kWrongStep = -0.5;
constexpr double kSkipStep = -0.3;
}  // namespace

AdaptiveSelector::AdaptiveSelector(std::uint32_t seed) : rng_(seed) {}

const Question* AdaptiveSelector::next(const QuestionBank& bank) {
    if (bank.empty()) return nullptr;
    ++served_;
    target_ = std::clamp(target_, 1.0, static_cast<double>(bank.maxDifficulty()));

    auto serve = [&](const Question& q) {
        stats_[q.id].lastServed = served_;
        lastId_ = q.id;
        return &q;
    };

    // A missed question that is due comes back first (never twice in a row,
    // unless it is the only question there is).
    for (auto it = requeue_.begin(); it != requeue_.end(); ++it) {
        if (it->dueAt > served_) continue;
        const Question* q = bank.find(it->id);
        if (q == nullptr || (bank.size() > 1 && q->id == lastId_)) continue;
        requeue_.erase(it);
        return serve(*q);
    }

    const long window = std::min<long>(kRecentWindow, static_cast<long>(bank.size()) - 1);
    std::vector<const Question*> candidates;
    std::vector<double> weights;
    for (const auto& q : bank.all()) {
        const auto found = stats_.find(q.id);
        const Stats s = found == stats_.end() ? Stats{} : found->second;
        if (s.lastServed >= 0 && served_ - s.lastServed <= window) continue;

        const double d = q.difficulty - target_;
        double w = std::exp(-(d * d) / (2.0 * kSigma * kSigma));
        w *= 1.0 + kMissWeight * s.misses;
        if (s.attempts == 0) w *= kUnseenBoost;
        candidates.push_back(&q);
        weights.push_back(std::max(w, 1e-9));
    }
    if (candidates.empty()) return serve(bank.all().front());  // unreachable in practice

    std::discrete_distribution<std::size_t> pick(weights.begin(), weights.end());
    return serve(*candidates[pick(rng_)]);
}

void AdaptiveSelector::record(const Question& q, Outcome outcome, double speedMultiplier) {
    Stats& s = stats_[q.id];
    ++s.attempts;

    switch (outcome) {
        case Outcome::Correct:
            target_ += speedMultiplier >= 1.0 ? kFastStep : kSlowStep;
            break;
        case Outcome::Wrong: {
            ++s.misses;
            target_ = std::max(1.0, target_ + kWrongStep);
            const bool queued = std::any_of(requeue_.begin(), requeue_.end(),
                                            [&](const Requeued& r) { return r.id == q.id; });
            if (!queued) requeue_.push_back({q.id, served_ + kRequeueDelay});
            break;
        }
        case Outcome::Skipped:
            target_ = std::max(1.0, target_ + kSkipStep);
            break;
    }
}

}  // namespace cq
