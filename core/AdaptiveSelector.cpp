#include "AdaptiveSelector.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "Mastery.h"

namespace cq {

AdaptiveSelector::AdaptiveSelector(std::uint32_t seed, SelectorConfig config, SelectorContext context)
    : config_(config), context_(std::move(context)), rng_(seed) {}

std::int64_t AdaptiveSelector::now() const {
    return context_.clock ? context_.clock() : systemSeconds();
}

double AdaptiveSelector::startingTarget(const QuestionBank& bank) const {
    const std::int64_t t = now();
    int comfortable = 0;  // highest level, reached without gaps, that the player has down
    for (int level = QuestionBank::kMinDifficulty; level <= bank.maxDifficulty(); ++level) {
        double sum = 0.0;
        int count = 0;
        for (const auto& q : bank.all()) {
            if (q.difficulty != level) continue;
            const QuestionStats* s = context_.progress->stats(q.id);
            if (s == nullptr || s->attempts <= 0) continue;
            sum += mastery(s, context_.scoring.parSeconds(q), t, config_);
            ++count;
        }
        // No evidence at this level, or not good enough yet: stop climbing.
        if (count < config_.startMinAttempts || sum / count < config_.startMasteryThreshold) break;
        comfortable = level;
    }
    if (comfortable == 0) return 1.0;
    return std::clamp(comfortable + config_.startOffset, 1.0,
                      static_cast<double>(bank.maxDifficulty()));
}

std::unordered_map<std::string, double> AdaptiveSelector::tagNeeds(const QuestionBank& bank) const {
    const std::int64_t t = now();
    std::unordered_map<std::string, std::pair<double, int>> acc;  // tag -> (sum, count)
    for (const auto& q : bank.all()) {
        const QuestionStats* s = context_.progress->stats(q.id);
        if (s == nullptr || s->attempts <= 0) continue;
        const double need = 1.0 - mastery(s, context_.scoring.parSeconds(q), t, config_);
        for (const auto& tag : q.tags) {
            acc[tag].first += need;
            ++acc[tag].second;
        }
    }
    std::unordered_map<std::string, double> out;
    for (const auto& [tag, sc] : acc) out[tag] = sc.first / sc.second;
    return out;
}

const Question* AdaptiveSelector::next(const QuestionBank& bank) {
    if (bank.empty()) return nullptr;

    if (!initialized_) {
        initialized_ = true;
        if (historyOn()) target_ = startingTarget(bank);
    }
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

    const bool history = historyOn();
    const std::int64_t t = history ? now() : 0;
    std::unordered_map<std::string, double> tagNeed;
    if (history && config_.tagBoost > 0.0) tagNeed = tagNeeds(bank);

    const long window = std::min<long>(config_.recentWindow, static_cast<long>(bank.size()) - 1);
    std::vector<const Question*> candidates;
    std::vector<double> weights;
    for (const auto& q : bank.all()) {
        const auto found = stats_.find(q.id);
        const Stats s = found == stats_.end() ? Stats{} : found->second;
        if (s.lastServed >= 0 && served_ - s.lastServed <= window) continue;

        const double d = q.difficulty - target_;
        double w = std::exp(-(d * d) / (2.0 * config_.sigma * config_.sigma));
        w *= 1.0 + config_.sessionMissWeight * s.misses;

        bool unseen = s.attempts == 0;
        if (history) {
            const QuestionStats* hs = context_.progress->stats(q.id);
            unseen = hs == nullptr || hs->attempts <= 0;
            if (!unseen) {
                const double need = 1.0 - mastery(hs, context_.scoring.parSeconds(q), t, config_);
                w *= config_.needFloor + (1.0 - config_.needFloor) * need;
            }
            double tagSum = 0.0;
            int tagCount = 0;
            for (const auto& tag : q.tags) {
                auto it = tagNeed.find(tag);
                if (it == tagNeed.end()) continue;
                tagSum += it->second;
                ++tagCount;
            }
            if (tagCount > 0) w *= 1.0 + config_.tagBoost * (tagSum / tagCount);
        }
        if (unseen) w *= config_.unseenBoost;

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
            target_ += speedMultiplier >= 1.0 ? config_.fastStep : config_.slowStep;
            break;
        case Outcome::Wrong: {
            ++s.misses;
            target_ = std::max(1.0, target_ + config_.wrongStep);
            const bool queued = std::any_of(requeue_.begin(), requeue_.end(),
                                            [&](const Requeued& r) { return r.id == q.id; });
            if (!queued) requeue_.push_back({q.id, served_ + config_.requeueDelay});
            break;
        }
        case Outcome::Skipped:
            target_ = std::max(1.0, target_ + config_.skipStep);
            break;
    }
}

}  // namespace cq
