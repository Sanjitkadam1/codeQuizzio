#pragma once

#include <cstdint>
#include <deque>
#include <random>
#include <string>
#include <unordered_map>

#include "Selector.h"

namespace cq {

// Keeps a moving target difficulty: it climbs when the player answers quickly
// and correctly, and falls on misses/skips. The next question is drawn at
// random, weighted toward questions near the target, toward ones the player
// has missed before, and toward ones not yet seen. Missed questions also come
// back a few questions later (spaced-repetition lite), and nothing repeats
// within a short window.
class AdaptiveSelector : public Selector {
public:
    explicit AdaptiveSelector(std::uint32_t seed = std::random_device{}());

    const Question* next(const QuestionBank& bank) override;
    void record(const Question& q, Outcome outcome, double speedMultiplier) override;

    double targetDifficulty() const { return target_; }

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

    std::unordered_map<std::string, Stats> stats_;
    std::deque<Requeued> requeue_;
    double target_ = 1.0;
    long served_ = 0;
    std::string lastId_;
    std::mt19937 rng_;
};

}  // namespace cq
