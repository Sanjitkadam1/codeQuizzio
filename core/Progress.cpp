#include "Progress.h"

#include <algorithm>
#include <cmath>

#include <nlohmann/json.hpp>

namespace cq {

namespace {

using nlohmann::json;

constexpr double kMinRecordedTime = 0.001;  // keeps "bestTime > 0" meaning "has one"

int readCount(const json& j, const char* key) {
    const int v = j.value(key, 0);
    if (v < 0) throw ProgressError(std::string("negative value for '") + key + "'");
    return v;
}

std::int64_t readTimestamp(const json& j, const char* key) {
    const std::int64_t v = j.value(key, std::int64_t{0});
    if (v < 0) throw ProgressError(std::string("negative value for '") + key + "'");
    return v;
}

double readSeconds(const json& j, const char* key) {
    const double v = j.value(key, 0.0);
    if (!std::isfinite(v) || v < 0.0) {
        throw ProgressError(std::string("invalid value for '") + key + "'");
    }
    return v;
}

json toJson(const QuestionStats& s) {
    return {{"attempts", s.attempts},
            {"correct", s.correct},
            {"misses", s.misses},
            {"skips", s.skips},
            {"bestTime", s.bestTime},
            {"totalCorrectTime", s.totalCorrectTime},
            {"lastSeen", s.lastSeen},
            {"recent", s.recent}};
}

// Optional field (older saves don't have it). Keeps only the newest outcomes.
std::string readRecent(const json& j) {
    std::string r = j.value("recent", std::string{});
    for (char c : r) {
        if (c != 'c' && c != 'w' && c != 's') throw ProgressError("invalid character in 'recent'");
    }
    if (r.size() > Progress::kRecentLength) r.erase(0, r.size() - Progress::kRecentLength);
    return r;
}

QuestionStats questionStatsFromJson(const json& j) {
    if (!j.is_object()) throw ProgressError("question stats must be an object");
    QuestionStats s;
    s.attempts = readCount(j, "attempts");
    s.correct = readCount(j, "correct");
    s.misses = readCount(j, "misses");
    s.skips = readCount(j, "skips");
    s.bestTime = readSeconds(j, "bestTime");
    s.totalCorrectTime = readSeconds(j, "totalCorrectTime");
    s.lastSeen = readTimestamp(j, "lastSeen");
    s.recent = readRecent(j);
    return s;
}

json toJson(const SessionRecord& r) {
    return {{"endedAt", r.endedAt},   {"score", r.score},
            {"correct", r.correct},   {"wrong", r.wrong},
            {"skipped", r.skipped},   {"bestStreak", r.bestStreak}};
}

SessionRecord sessionFromJson(const json& j) {
    if (!j.is_object()) throw ProgressError("session record must be an object");
    SessionRecord r;
    r.endedAt = readTimestamp(j, "endedAt");
    r.score = readCount(j, "score");
    r.correct = readCount(j, "correct");
    r.wrong = readCount(j, "wrong");
    r.skipped = readCount(j, "skipped");
    r.bestStreak = readCount(j, "bestStreak");
    return r;
}

}  // namespace

void Progress::recordAnswer(const std::string& id, Outcome outcome, double elapsed,
                            std::int64_t now) {
    QuestionStats& s = stats_[id];
    ++s.attempts;
    s.lastSeen = now;
    char mark = 'c';
    switch (outcome) {
        case Outcome::Correct: {
            ++s.correct;
            const double t = std::max(elapsed, kMinRecordedTime);
            s.totalCorrectTime += t;
            s.bestTime = s.hasBestTime() ? std::min(s.bestTime, t) : t;
            break;
        }
        case Outcome::Wrong:
            ++s.misses;
            mark = 'w';
            break;
        case Outcome::Skipped:
            ++s.skips;
            mark = 's';
            break;
    }
    s.recent.push_back(mark);
    if (s.recent.size() > kRecentLength) s.recent.erase(0, s.recent.size() - kRecentLength);
}

RecordResult Progress::recordSession(const SessionRecord& record) {
    RecordResult result;
    ++totalSessions_;
    if (record.score > highScore_) {
        highScore_ = record.score;
        result.newHighScore = true;
    }
    if (record.bestStreak > bestStreak_) {
        bestStreak_ = record.bestStreak;
        result.newBestStreak = true;
    }
    history_.push_back(record);
    if (history_.size() > kMaxHistory) {
        history_.erase(history_.begin(), history_.end() - static_cast<std::ptrdiff_t>(kMaxHistory));
    }
    return result;
}

const QuestionStats* Progress::stats(const std::string& id) const {
    auto it = stats_.find(id);
    return it == stats_.end() ? nullptr : &it->second;
}

std::string Progress::toJson() const {
    json doc;
    doc["version"] = kVersion;
    doc["highScore"] = highScore_;
    doc["bestStreak"] = bestStreak_;
    doc["totalSessions"] = totalSessions_;

    json questions = json::object();
    for (const auto& [id, s] : stats_) questions[id] = cq::toJson(s);
    doc["questions"] = std::move(questions);

    json history = json::array();
    for (const auto& r : history_) history.push_back(cq::toJson(r));
    doc["history"] = std::move(history);

    return doc.dump(2);
}

Progress Progress::fromJson(const std::string& text) {
    try {
        const json doc = json::parse(text);
        if (!doc.is_object()) throw ProgressError("progress file is not a JSON object");

        const int version = doc.value("version", 0);
        if (version < 1) throw ProgressError("missing or invalid 'version'");
        if (version > kVersion) {
            throw ProgressError("saved by a newer version of CodeQuizzio (format " +
                                std::to_string(version) + ")");
        }

        Progress p;
        p.highScore_ = readCount(doc, "highScore");
        p.bestStreak_ = readCount(doc, "bestStreak");
        p.totalSessions_ = readCount(doc, "totalSessions");

        if (doc.contains("questions")) {
            if (!doc["questions"].is_object()) throw ProgressError("'questions' must be an object");
            for (const auto& [id, value] : doc["questions"].items()) {
                p.stats_[id] = questionStatsFromJson(value);
            }
        }
        if (doc.contains("history")) {
            if (!doc["history"].is_array()) throw ProgressError("'history' must be an array");
            for (const auto& entry : doc["history"]) p.history_.push_back(sessionFromJson(entry));
            if (p.history_.size() > kMaxHistory) {
                p.history_.erase(p.history_.begin(),
                                 p.history_.end() - static_cast<std::ptrdiff_t>(kMaxHistory));
            }
        }
        return p;
    } catch (const ProgressError&) {
        throw;
    } catch (const json::exception& e) {
        throw ProgressError(std::string("malformed progress data: ") + e.what());
    }
}

}  // namespace cq
