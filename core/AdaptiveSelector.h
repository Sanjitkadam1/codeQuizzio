#pragma once

#include <cstdint>
#include <deque>
#include <random>
#include <string>
#include <unordered_map>

#include "Clock.h"
#include "Progress.h"
#include "Scoring.h"
#include "Selector.h"
#include "SelectorConfig.h"

namespace cq {

// Optional collaborators. With no Progress the selector only reacts to the
// current session; with one it also uses everything the player has done before.
struct SelectorContext {
    const Progress* progress = nullptr;  // must outlive the selector
    Scoring scoring;                     // supplies par times for the speed part of mastery
    Clock clock;                         // empty = system clock
};

// Picks the next question by weighted random draw. A question's weight is the
// product of independent factors, each controlled by SelectorConfig:
//
//   difficulty  bell curve around a moving target difficulty. The target
//               climbs after fast correct answers and falls after misses/skips.
//   need        (history) weak or rusty questions weigh more; mastered ones
//               weigh needFloor, so they still come back occasionally
//   tag         (history) questions in your weakest topics get a boost
//   session     questions missed earlier this session get a boost
//   unseen      never-seen questions get a boost
//
// Missed questions also return a few turns later, and nothing repeats within a
// short window. The whole thing sits behind the Selector interface, so a
// different algorithm can replace it without touching Session or the UI.
class AdaptiveSelector : public Selector {
public:
    explicit AdaptiveSelector(std::uint32_t seed = std::random_device{}(),
                              SelectorConfig config = {}, SelectorContext context = {});

    const Question* next(const QuestionBank& bank) override;
    void record(const Question& q, Outcome outcome, double speedMultiplier) override;

    double targetDifficulty() const { return target_; }
    const SelectorConfig& config() const { return config_; }

private:
    struct Stats {
        int attempts = 0;
        int misses = 0;
        long lastServed = -1;
    };
    struct Requeued {
        std::string id;
        long dueAt;
    };

    bool historyOn() const { return config_.useHistory && context_.progress != nullptr; }
    std::int64_t now() const;
    double startingTarget(const QuestionBank& bank) const;
    // Average "need" (1 - mastery) of the attempted questions in each tag.
    std::unordered_map<std::string, double> tagNeeds(const QuestionBank& bank) const;

    SelectorConfig config_;
    SelectorContext context_;
    bool initialized_ = false;

    std::unordered_map<std::string, Stats> stats_;
    std::deque<Requeued> requeue_;
    double target_ = 1.0;
    long served_ = 0;
    std::string lastId_;
    std::mt19937 rng_;
};

}  // namespace cq
