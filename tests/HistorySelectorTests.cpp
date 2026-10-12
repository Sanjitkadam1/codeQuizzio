#include <gtest/gtest.h>

#include <functional>
#include <map>
#include <string>

#include "AdaptiveSelector.h"
#include "Progress.h"
#include "TestUtil.h"

using namespace cq;

namespace {

constexpr std::int64_t kDay = 86400;
constexpr std::int64_t kStart = 1'700'000'000;

cq::Question tagged(const std::string& id, int difficulty, const std::string& tag) {
    cq::Question q = makeQuestion(id, difficulty);
    q.tags = {tag};
    return q;
}

// `perTag` questions for each tag, all at one difficulty so only history differs.
QuestionBank taggedBank(std::vector<std::string> tags, int perTag, int difficulty = 3) {
    QuestionBank bank;
    for (const auto& tag : tags) {
        for (int i = 0; i < perTag; ++i) {
            bank.add(tagged(tag + std::to_string(i), difficulty, tag));
        }
    }
    return bank;
}

struct Answer {
    Outcome outcome;
    double seconds;
};
using Player = std::function<Answer(const Question&)>;

Answer right() { return {Outcome::Correct, 2.0}; }  // well under par
Answer wrong() { return {Outcome::Wrong, 5.0}; }

std::string tagOf(const Question& q) { return q.tags.front(); }

struct SimResult {
    std::map<std::string, int> servedByTag;  // counted over the measured sessions only
    int measured = 0;
};

// Plays `sessions` sessions of `perSession` questions. Every session gets a
// brand-new selector (so in-session memory resets) but all share `progress`.
// Only the last `measureLast` sessions are counted.
SimResult simulate(const QuestionBank& bank, Progress& progress, const SelectorConfig& cfg,
                   const Player& player, int sessions, int perSession, int measureLast,
                   std::uint32_t seed = 1, std::int64_t* clock = nullptr) {
    std::int64_t localClock = kStart;
    std::int64_t& now = clock ? *clock : localClock;
    SimResult out;
    for (int s = 0; s < sessions; ++s) {
        SelectorContext ctx;
        ctx.progress = &progress;
        ctx.clock = [&now] { return now; };
        AdaptiveSelector sel(seed * 1000 + static_cast<std::uint32_t>(s), cfg, ctx);
        for (int i = 0; i < perSession; ++i) {
            const Question* q = sel.next(bank);
            const Answer a = player(*q);
            progress.recordAnswer(q->id, a.outcome, a.seconds, now);
            sel.record(*q, a.outcome, a.outcome == Outcome::Correct ? 1.4 : 0.0);
            now += 20;
            if (s >= sessions - measureLast) {
                ++out.servedByTag[tagOf(*q)];
                ++out.measured;
            }
        }
        now += kDay / 4;  // sessions are hours apart
    }
    return out;
}

double share(const SimResult& r, const std::string& tag) {
    const auto it = r.servedByTag.find(tag);
    return it == r.servedByTag.end() ? 0.0 : static_cast<double>(it->second) / r.measured;
}

}  // namespace

// ---- The point of 3b: what you did in earlier sessions shapes what you see ----

TEST(HistorySelector, DrillsAWeakTopicAcrossSessions) {
    const auto bank = taggedBank({"basics", "stl", "pointers"}, 8);
    const Player player = [](const Question& q) { return tagOf(q) == "pointers" ? wrong() : right(); };

    SelectorConfig on;
    SelectorConfig off;
    off.useHistory = false;
    Progress withHistory, withoutHistory;
    const auto a = simulate(bank, withHistory, on, player, 40, 12, 20);
    const auto b = simulate(bank, withoutHistory, off, player, 40, 12, 20);

    EXPECT_GT(share(a, "pointers"), 0.45) << "weak topic should dominate";
    EXPECT_GT(share(a, "pointers"), share(b, "pointers") + 0.08)
        << "history should beat in-session memory alone";
}

TEST(HistorySelector, MasteredQuestionsRareButNotGone) {
    const auto bank = taggedBank({"known", "unknown"}, 10);
    const Player player = [](const Question& q) { return tagOf(q) == "known" ? right() : wrong(); };

    Progress progress;
    const auto r = simulate(bank, progress, SelectorConfig{}, player, 40, 12, 20);
    EXPECT_LT(share(r, "known"), 0.40);
    EXPECT_GT(share(r, "known"), 0.015) << "needFloor keeps mastered questions in rotation";
}

TEST(HistorySelector, NeedFloorOfOneDisablesTheDrilling) {
    const auto bank = taggedBank({"known", "unknown"}, 10);
    const Player player = [](const Question& q) { return tagOf(q) == "known" ? right() : wrong(); };

    SelectorConfig flat;
    flat.needFloor = 1.0;
    flat.tagBoost = 0.0;
    flat.sessionMissWeight = 0.0;
    flat.requeueDelay = 100;  // missed questions never come back within a 12-question session
    Progress progress;
    const auto r = simulate(bank, progress, flat, player, 40, 12, 20);
    EXPECT_NEAR(share(r, "known"), 0.5, 0.12);  // no preference left
}

TEST(HistorySelector, ForgettingBringsMasteredQuestionsBack) {
    const auto bank = taggedBank({"old", "fresh"}, 6);
    // Both topics were perfect at kStart; "fresh" was practiced again 1 day ago.
    Progress progress;
    for (const auto& q : bank.all()) {
        for (int i = 0; i < 5; ++i) progress.recordAnswer(q.id, Outcome::Correct, 2.0, kStart);
        if (tagOf(q) == "fresh") progress.recordAnswer(q.id, Outcome::Correct, 2.0, kStart + 59 * kDay);
    }

    SelectorConfig cfg;
    cfg.forgetHalfLifeDays = 14;
    int oldServed = 0, freshServed = 0;
    for (std::uint32_t seed = 0; seed < 300; ++seed) {
        SelectorContext ctx;
        ctx.progress = &progress;
        ctx.clock = [] { return kStart + 60 * kDay; };
        AdaptiveSelector sel(seed, cfg, ctx);
        (tagOf(*sel.next(bank)) == "old" ? oldServed : freshServed)++;
    }
    EXPECT_GT(oldServed, freshServed * 2) << "the rusty topic should be due for review";

    // With forgetting off, they are equally known, so equally likely.
    cfg.forgetHalfLifeDays = 0;
    oldServed = freshServed = 0;
    for (std::uint32_t seed = 0; seed < 300; ++seed) {
        SelectorContext ctx;
        ctx.progress = &progress;
        ctx.clock = [] { return kStart + 60 * kDay; };
        AdaptiveSelector sel(seed, cfg, ctx);
        (tagOf(*sel.next(bank)) == "old" ? oldServed : freshServed)++;
    }
    EXPECT_NEAR(static_cast<double>(oldServed) / 300, 0.5, 0.12);
}

TEST(HistorySelector, SlowAnswersKeepAQuestionInRotation) {
    const auto bank = taggedBank({"quick", "slow"}, 8);
    // Both topics are always answered correctly; "slow" takes 4x par.
    const Player player = [](const Question& q) {
        return tagOf(q) == "quick" ? Answer{Outcome::Correct, 2.0} : Answer{Outcome::Correct, 60.0};
    };
    SelectorConfig cfg;
    cfg.speedWeight = 0.8;
    cfg.tagBoost = 0.0;
    Progress progress;
    const auto r = simulate(bank, progress, cfg, player, 40, 12, 20);
    EXPECT_GT(share(r, "slow"), 0.55);
}

TEST(HistorySelector, WeakTopicBoostLiftsUnseenQuestionsInThatTopic) {
    // x0..x3 and y0..y3 have history; x4..x7 and y4..y7 are brand new.
    QuestionBank bank = taggedBank({"x", "y"}, 8);
    Progress progress;
    for (int i = 0; i < 4; ++i) {
        for (int k = 0; k < 6; ++k) {
            progress.recordAnswer("x" + std::to_string(i), Outcome::Wrong, 5.0, kStart);
            progress.recordAnswer("y" + std::to_string(i), Outcome::Correct, 2.0, kStart);
        }
    }
    auto firstPickRatio = [&](double boost) {
        SelectorConfig cfg;
        cfg.tagBoost = boost;
        int newX = 0, newY = 0;
        for (std::uint32_t seed = 0; seed < 600; ++seed) {
            SelectorContext ctx;
            ctx.progress = &progress;
            ctx.clock = [] { return kStart; };
            AdaptiveSelector sel(seed, cfg, ctx);
            const Question* q = sel.next(bank);
            const int index = std::stoi(q->id.substr(1));
            if (index >= 4) (tagOf(*q) == "x" ? newX : newY)++;
        }
        return static_cast<double>(newX) / std::max(newY, 1);
    };
    EXPECT_GT(firstPickRatio(5.0), 2.0);
    EXPECT_NEAR(firstPickRatio(0.0), 1.0, 0.3);
}

// ---- Starting difficulty ----

namespace {

QuestionBank leveledBank() {
    QuestionBank bank;
    for (int d = 1; d <= 10; ++d)
        for (int i = 0; i < 3; ++i) bank.add(tagged("L" + std::to_string(d) + "_" + std::to_string(i), d, "t"));
    return bank;
}

void master(Progress& p, const QuestionBank& bank, int level, const std::string& outcomes = "cccc") {
    for (const auto& q : bank.all()) {
        if (q.difficulty != level) continue;
        for (char c : outcomes) {
            p.recordAnswer(q.id, c == 'c' ? Outcome::Correct : Outcome::Wrong, 1.0, kStart);
        }
    }
}

double firstTarget(const QuestionBank& bank, const Progress* progress, SelectorConfig cfg = {}) {
    SelectorContext ctx;
    ctx.progress = progress;
    ctx.clock = [] { return kStart; };
    AdaptiveSelector sel(1, cfg, ctx);
    sel.next(bank);
    return sel.targetDifficulty();
}

}  // namespace

TEST(HistorySelector, StartsAtLevelOneWithNoHistory) {
    const auto bank = leveledBank();
    Progress empty;
    EXPECT_DOUBLE_EQ(firstTarget(bank, &empty), 1.0);
    EXPECT_DOUBLE_EQ(firstTarget(bank, nullptr), 1.0);
}

TEST(HistorySelector, StartsJustAboveTheLevelYouKnow) {
    const auto bank = leveledBank();
    Progress p;
    for (int level = 1; level <= 4; ++level) master(p, bank, level);
    EXPECT_DOUBLE_EQ(firstTarget(bank, &p), 4.5);  // comfortable 4 + startOffset 0.5

    SelectorConfig cfg;
    cfg.startOffset = 1.5;
    EXPECT_DOUBLE_EQ(firstTarget(bank, &p, cfg), 5.5);
}

TEST(HistorySelector, StopsAtTheFirstLevelYouStruggleWith) {
    const auto bank = leveledBank();
    Progress p;
    master(p, bank, 1);
    master(p, bank, 2);
    master(p, bank, 3, "wwww");  // failing level 3
    master(p, bank, 4);          // good at 4, but 3 blocks the climb
    EXPECT_DOUBLE_EQ(firstTarget(bank, &p), 2.5);
}

TEST(HistorySelector, ALevelWithNoEvidenceBlocksTheClimb) {
    const auto bank = leveledBank();
    Progress p;
    master(p, bank, 1);
    master(p, bank, 2);
    master(p, bank, 4);  // nothing at level 3
    EXPECT_DOUBLE_EQ(firstTarget(bank, &p), 2.5);
}

TEST(HistorySelector, NeedsEnoughAttemptedQuestionsAtALevel) {
    // Level 1 has two questions, level 2 has two, level 3 has one (never attempted).
    QuestionBank bank;
    bank.add(tagged("a", 1, "t"));
    bank.add(tagged("a2", 1, "t"));
    bank.add(tagged("b", 2, "t"));
    bank.add(tagged("c", 2, "t"));
    bank.add(tagged("d", 3, "t"));
    Progress p;
    for (const char* id : {"a", "a2", "b"}) {  // level 2: only one of its two questions attempted
        for (int i = 0; i < 6; ++i) p.recordAnswer(id, Outcome::Correct, 1.0, kStart);
    }

    SelectorConfig cfg;
    cfg.confidenceAttempts = 0;
    cfg.startMinAttempts = 1;
    EXPECT_DOUBLE_EQ(firstTarget(bank, &p, cfg), 2.5);  // levels 1 and 2 judged; level 3 has no evidence
    cfg.startMinAttempts = 2;
    EXPECT_DOUBLE_EQ(firstTarget(bank, &p, cfg), 1.5);  // level 2 has only one attempted question
}

TEST(HistorySelector, LongAbsenceLowersTheStartingLevel) {
    const auto bank = leveledBank();
    Progress p;
    for (int level = 1; level <= 4; ++level) master(p, bank, level);

    SelectorContext ctx;
    ctx.progress = &p;
    ctx.clock = [] { return kStart + 365 * kDay; };  // a year later
    AdaptiveSelector sel(1, SelectorConfig{}, ctx);
    sel.next(bank);
    EXPECT_LT(sel.targetDifficulty(), 2.0);
}

TEST(HistorySelector, StartingTargetNeverExceedsTheBank) {
    QuestionBank bank;
    for (int i = 0; i < 3; ++i) bank.add(tagged("q" + std::to_string(i), 2, "t"));
    bank.add(tagged("easy", 1, "t"));
    Progress p;
    for (const auto& q : bank.all())
        for (int i = 0; i < 6; ++i) p.recordAnswer(q.id, Outcome::Correct, 1.0, kStart);
    SelectorConfig cfg;
    cfg.startOffset = 5.0;
    EXPECT_LE(firstTarget(bank, &p, cfg), 2.0);
}

// ---- Config switches ----

TEST(HistorySelector, UseHistoryFalseIgnoresProgressEntirely) {
    const auto bank = leveledBank();
    Progress p;
    for (int level = 1; level <= 6; ++level) master(p, bank, level);

    SelectorConfig off;
    off.useHistory = false;
    SelectorContext withProgress;
    withProgress.progress = &p;
    SelectorContext without;

    AdaptiveSelector a(77, off, withProgress);
    AdaptiveSelector b(77, off, without);
    for (int i = 0; i < 60; ++i) {
        const Question* qa = a.next(bank);
        const Question* qb = b.next(bank);
        ASSERT_EQ(qa->id, qb->id) << "diverged at step " << i;
        const Outcome o = i % 3 == 0 ? Outcome::Wrong : Outcome::Correct;
        a.record(*qa, o, 1.2);
        b.record(*qb, o, 1.2);
    }
    EXPECT_DOUBLE_EQ(a.targetDifficulty(), b.targetDifficulty());
}

TEST(HistorySelector, ConfigStepsAreRespected) {
    const auto bank = leveledBank();
    SelectorConfig cfg;
    cfg.fastStep = 1.0;
    cfg.wrongStep = -2.0;
    AdaptiveSelector sel(1, cfg);
    const Question* q = sel.next(bank);
    sel.record(*q, Outcome::Correct, 1.4);
    EXPECT_DOUBLE_EQ(sel.targetDifficulty(), 2.0);
    q = sel.next(bank);
    sel.record(*q, Outcome::Correct, 1.4);
    EXPECT_DOUBLE_EQ(sel.targetDifficulty(), 3.0);
    q = sel.next(bank);
    sel.record(*q, Outcome::Wrong, 0.0);
    EXPECT_DOUBLE_EQ(sel.targetDifficulty(), 1.0);
}

TEST(HistorySelector, ConfigWindowAndRequeueDelayAreRespected) {
    const auto bank = taggedBank({"a"}, 20);
    SelectorConfig cfg;
    cfg.recentWindow = 12;
    AdaptiveSelector sel(5, cfg);
    std::vector<std::string> seen;
    for (int i = 0; i < 100; ++i) {
        const Question* q = sel.next(bank);
        for (std::size_t back = 0; back < 12 && back < seen.size(); ++back) {
            ASSERT_NE(q->id, seen[seen.size() - 1 - back]);
        }
        seen.push_back(q->id);
        sel.record(*q, Outcome::Correct, 1.0);
    }

    SelectorConfig delayed;
    delayed.requeueDelay = 6;
    AdaptiveSelector sel2(5, delayed);
    const Question* missed = sel2.next(bank);
    const std::string id = missed->id;
    sel2.record(*missed, Outcome::Wrong, 0.0);
    int stepsUntilBack = 0;
    for (int i = 1; i <= 10; ++i) {
        const Question* q = sel2.next(bank);
        if (q->id == id) {
            stepsUntilBack = i;
            break;
        }
        sel2.record(*q, Outcome::Correct, 1.0);
    }
    EXPECT_GE(stepsUntilBack, 6);
}
