#include <gtest/gtest.h>

#include "Mastery.h"

using namespace cq;

namespace {

constexpr std::int64_t kDay = 86400;
constexpr double kPar = 10.0;

// Builds stats by replaying outcomes through the real recording code.
QuestionStats statsFrom(const std::string& outcomes, double time = 5.0, std::int64_t at = 1000) {
    Progress p;
    for (char c : outcomes) {
        const Outcome o = c == 'c' ? Outcome::Correct : c == 'w' ? Outcome::Wrong : Outcome::Skipped;
        p.recordAnswer("q", o, time, at);
    }
    return *p.stats("q");
}

// Plain config: no forgetting, no speed term, no confidence pull, flat recency.
SelectorConfig plain() {
    SelectorConfig c;
    c.forgetHalfLifeDays = 0;
    c.speedWeight = 0;
    c.confidenceAttempts = 0;
    c.recencyDecay = 1.0;
    return c;
}

}  // namespace

TEST(Mastery, UnseenQuestionGetsThePrior) {
    SelectorConfig c;
    EXPECT_DOUBLE_EQ(mastery(nullptr, kPar, 0, c), c.masteryPrior);
    QuestionStats none;
    EXPECT_DOUBLE_EQ(mastery(&none, kPar, 0, c), c.masteryPrior);
}

TEST(Mastery, AccuracyDrivesTheScore) {
    const SelectorConfig c = plain();
    const auto allRight = statsFrom("cccc");
    const auto half = statsFrom("cwcw");
    const auto allWrong = statsFrom("wwww");
    EXPECT_DOUBLE_EQ(mastery(&allRight, kPar, 1000, c), 1.0);
    EXPECT_DOUBLE_EQ(mastery(&half, kPar, 1000, c), 0.5);
    EXPECT_DOUBLE_EQ(mastery(&allWrong, kPar, 1000, c), 0.0);
}

TEST(Mastery, RecentOutcomesCountMore) {
    SelectorConfig c = plain();
    c.recencyDecay = 0.5;
    const auto improving = statsFrom("wwcc");  // got it right lately
    const auto slipping = statsFrom("ccww");   // started missing
    EXPECT_GT(mastery(&improving, kPar, 1000, c), mastery(&slipping, kPar, 1000, c));
}

TEST(Mastery, SkipsCountAsMissesByDefaultAndCanBeSoftened) {
    SelectorConfig c = plain();
    const auto s = statsFrom("cs");
    EXPECT_DOUBLE_EQ(mastery(&s, kPar, 1000, c), 0.5);
    c.skipPenalty = 0.0;
    EXPECT_DOUBLE_EQ(mastery(&s, kPar, 1000, c), 1.0);
}

TEST(Mastery, SlowCorrectAnswersScoreLowerThanFastOnes) {
    SelectorConfig c = plain();
    c.speedWeight = 0.5;
    const auto fast = statsFrom("cccc", 4.0);   // under par
    const auto slow = statsFrom("cccc", 30.0);  // 3x par
    EXPECT_DOUBLE_EQ(mastery(&fast, kPar, 1000, c), 1.0);
    EXPECT_LT(mastery(&slow, kPar, 1000, c), mastery(&fast, kPar, 1000, c));
    EXPECT_GT(mastery(&slow, kPar, 1000, c), 0.5);  // slow but right is still partly known
}

TEST(Mastery, FewAttemptsStayNearThePrior) {
    SelectorConfig c = plain();
    c.confidenceAttempts = 3;
    const auto once = statsFrom("c");
    const auto many = statsFrom("cccccccc");
    const double m1 = mastery(&once, kPar, 1000, c);
    const double m8 = mastery(&many, kPar, 1000, c);
    EXPECT_LT(m1, 0.8);  // one lucky answer is not mastery
    EXPECT_GT(m1, c.masteryPrior);
    EXPECT_GT(m8, m1);
}

TEST(Mastery, ForgettingHalvesEveryHalfLife) {
    SelectorConfig c = plain();
    c.forgetHalfLifeDays = 10;
    const auto s = statsFrom("cccc", 5.0, 1000);
    const double fresh = mastery(&s, kPar, 1000, c);
    EXPECT_NEAR(mastery(&s, kPar, 1000 + 10 * kDay, c), fresh / 2, 1e-9);
    EXPECT_NEAR(mastery(&s, kPar, 1000 + 20 * kDay, c), fresh / 4, 1e-9);
}

TEST(Mastery, HalfLifeZeroMeansNeverForget) {
    SelectorConfig c = plain();
    c.forgetHalfLifeDays = 0;
    const auto s = statsFrom("cccc", 5.0, 1000);
    EXPECT_DOUBLE_EQ(mastery(&s, kPar, 1000 + 3650 * kDay, c), mastery(&s, kPar, 1000, c));
}

TEST(Mastery, ClockGoingBackwardsDoesNotInflateTheScore) {
    SelectorConfig c = plain();
    c.forgetHalfLifeDays = 10;
    const auto s = statsFrom("cccc", 5.0, 1000);
    EXPECT_DOUBLE_EQ(mastery(&s, kPar, 500, c), mastery(&s, kPar, 1000, c));
}

TEST(Mastery, OldSavesWithoutRecentFallBackToTotals) {
    SelectorConfig c = plain();
    QuestionStats legacy;
    legacy.attempts = 4;
    legacy.correct = 3;
    legacy.misses = 1;
    legacy.totalCorrectTime = 15.0;
    legacy.bestTime = 4.0;
    legacy.lastSeen = 1000;
    ASSERT_TRUE(legacy.recent.empty());
    EXPECT_DOUBLE_EQ(mastery(&legacy, kPar, 1000, c), 0.75);
}

TEST(Mastery, StaysWithinZeroAndOne) {
    SelectorConfig c;
    for (const char* o : {"c", "w", "s", "cccccccc", "wwwwwwww", "cwcwcwcw"}) {
        const auto s = statsFrom(o);
        for (std::int64_t t : {std::int64_t{0}, std::int64_t{1000}, 1000 + 100 * kDay}) {
            const double m = mastery(&s, kPar, t, c);
            EXPECT_GE(m, 0.0) << o;
            EXPECT_LE(m, 1.0) << o;
        }
    }
}

TEST(Progress, RecentKeepsOnlyTheNewestOutcomes) {
    Progress p;
    for (int i = 0; i < 12; ++i) p.recordAnswer("q", Outcome::Wrong, 1.0, 1);
    p.recordAnswer("q", Outcome::Correct, 1.0, 2);
    p.recordAnswer("q", Outcome::Skipped, 0.0, 3);
    const std::string r = p.stats("q")->recent;
    EXPECT_EQ(r.size(), Progress::kRecentLength);
    EXPECT_EQ(r.substr(r.size() - 2), "cs");  // oldest first, newest last
}

TEST(Progress, RecentSurvivesTheRoundTrip) {
    Progress p;
    p.recordAnswer("q", Outcome::Correct, 1.0, 1);
    p.recordAnswer("q", Outcome::Wrong, 1.0, 2);
    EXPECT_EQ(Progress::fromJson(p.toJson()).stats("q")->recent, "cw");
}

TEST(Progress, SavesWithoutRecentStillLoad) {
    const Progress p = Progress::fromJson(R"({"version":1,"questions":{"q":{"attempts":2,"correct":2}}})");
    EXPECT_TRUE(p.stats("q")->recent.empty());
}

TEST(Progress, RejectsABadRecentField) {
    EXPECT_THROW(Progress::fromJson(R"({"version":1,"questions":{"q":{"recent":"cxw"}}})"), ProgressError);
    EXPECT_THROW(Progress::fromJson(R"({"version":1,"questions":{"q":{"recent":5}}})"), ProgressError);
}

TEST(Progress, OverlongRecentIsTrimmedOnLoad) {
    const Progress p = Progress::fromJson(
        R"({"version":1,"questions":{"q":{"recent":"wwwwwwwwwwcs"}}})");
    EXPECT_EQ(p.stats("q")->recent.size(), Progress::kRecentLength);
    EXPECT_EQ(p.stats("q")->recent.substr(6), "cs");
}
