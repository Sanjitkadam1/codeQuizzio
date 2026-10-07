#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "Outcome.h"

namespace cq {

// Thrown when saved progress cannot be parsed or fails validation.
class ProgressError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// All-time stats for one question, keyed by question id.
struct QuestionStats {
    int attempts = 0;
    int correct = 0;
    int misses = 0;
    int skips = 0;
    double bestTime = 0.0;          // seconds; 0 means "no correct answer yet"
    double totalCorrectTime = 0.0;  // sum over correct answers
    std::int64_t lastSeen = 0;      // unix seconds

    bool hasBestTime() const { return bestTime > 0.0; }
    double averageCorrectTime() const { return correct > 0 ? totalCorrectTime / correct : 0.0; }
};

// One finished play session.
struct SessionRecord {
    std::int64_t endedAt = 0;  // unix seconds
    int score = 0;
    int correct = 0;
    int wrong = 0;
    int skipped = 0;
    int bestStreak = 0;

    int answered() const { return correct + wrong + skipped; }
    double accuracy() const {
        const int graded = correct + wrong;
        return graded > 0 ? static_cast<double>(correct) / graded : 0.0;
    }
};

struct RecordResult {
    bool newHighScore = false;
    bool newBestStreak = false;
};

// Everything the game remembers between runs. Pure data + (de)serialization:
// no file access here (see ProgressStore) and no clock (callers pass `now`),
// so it is fully deterministic to test.
class Progress {
public:
    static constexpr int kVersion = 1;
    static constexpr std::size_t kMaxHistory = 200;

    void recordAnswer(const std::string& questionId, Outcome outcome, double elapsedSeconds,
                      std::int64_t now);
    RecordResult recordSession(const SessionRecord& record);

    // nullptr if the question has never been attempted.
    const QuestionStats* stats(const std::string& questionId) const;
    const std::unordered_map<std::string, QuestionStats>& allStats() const { return stats_; }
    const std::vector<SessionRecord>& history() const { return history_; }
    int highScore() const { return highScore_; }
    int bestStreak() const { return bestStreak_; }
    int totalSessions() const { return totalSessions_; }

    std::string toJson() const;
    // Throws ProgressError on malformed, invalid, or newer-version data.
    static Progress fromJson(const std::string& text);

private:
    std::unordered_map<std::string, QuestionStats> stats_;
    std::vector<SessionRecord> history_;
    int highScore_ = 0;
    int bestStreak_ = 0;
    int totalSessions_ = 0;
};

}  // namespace cq
