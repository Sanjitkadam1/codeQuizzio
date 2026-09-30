#include <gtest/gtest.h>

#include "Scoring.h"
#include "TestUtil.h"

using namespace cq;

TEST(Scoring, ParGrowsWithLengthAndDifficulty) {
    Scoring s;
    EXPECT_LT(s.parSeconds(makeQuestion("a", 1, {"ab"})), s.parSeconds(makeQuestion("b", 1, {"abcdefghij"})));
    EXPECT_LT(s.parSeconds(makeQuestion("a", 1, {"abcd"})), s.parSeconds(makeQuestion("b", 5, {"abcd"})));
}

TEST(Scoring, ParUsesShortestAnswer) {
    Scoring s;
    auto q = makeQuestion("a", 1, {"a much longer spelling", "x"});
    EXPECT_DOUBLE_EQ(s.parSeconds(q), s.parSeconds(makeQuestion("b", 1, {"x"})));
}

TEST(Scoring, SpeedMultiplierShape) {
    Scoring s;
    EXPECT_DOUBLE_EQ(s.speedMultiplier(0.0, 10.0), 1.5);
    EXPECT_DOUBLE_EQ(s.speedMultiplier(10.0, 10.0), 1.0);
    EXPECT_NEAR(s.speedMultiplier(5.0, 10.0), 1.25, 1e-9);
    EXPECT_DOUBLE_EQ(s.speedMultiplier(1000.0, 10.0), 0.25);
    EXPECT_GT(s.speedMultiplier(15.0, 10.0), 0.25);
    EXPECT_LT(s.speedMultiplier(15.0, 10.0), 1.0);
}

TEST(Scoring, SpeedMultiplierIsMonotonic) {
    Scoring s;
    double prev = s.speedMultiplier(0.0, 8.0);
    for (double t = 0.5; t < 40.0; t += 0.5) {
        double cur = s.speedMultiplier(t, 8.0);
        EXPECT_LE(cur, prev) << "t=" << t;
        prev = cur;
    }
}

TEST(Scoring, StreakMultiplierCapped) {
    Scoring s;
    EXPECT_DOUBLE_EQ(s.streakMultiplier(0), 1.0);
    EXPECT_DOUBLE_EQ(s.streakMultiplier(4), 1.2);
    EXPECT_DOUBLE_EQ(s.streakMultiplier(10), 1.5);
    EXPECT_DOUBLE_EQ(s.streakMultiplier(99), 1.5);
}

TEST(Scoring, HarderQuestionsWorthMore) {
    Scoring s;
    auto easy = makeQuestion("e", 1, {"abcd"});
    auto hard = makeQuestion("h", 5, {"abcd"});
    EXPECT_GT(s.score(hard, 3.0, 1).points, s.score(easy, 3.0, 1).points);
}

TEST(Scoring, FasterIsWorthMore) {
    Scoring s;
    auto q = makeQuestion("q", 3, {"abcdef"});
    EXPECT_GT(s.score(q, 2.0, 1).points, s.score(q, 12.0, 1).points);
}

TEST(Scoring, ConcreteExample) {
    Scoring s;
    auto q = makeQuestion("q", 2, {"return 0;"});  // 9 chars, difficulty 2
    auto b = s.score(q, 0.0, 1);
    EXPECT_EQ(b.basePoints, 20);
    EXPECT_NEAR(b.parSeconds, 4.0 + 0.35 * 9 + 1.0, 1e-9);
    EXPECT_EQ(b.points, static_cast<int>(std::lround(20 * 1.5 * 1.05)));
}
