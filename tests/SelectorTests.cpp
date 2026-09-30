#include <gtest/gtest.h>

#include <map>
#include <set>

#include "AdaptiveSelector.h"
#include "TestUtil.h"

using namespace cq;

TEST(AdaptiveSelector, EmptyBankReturnsNull) {
    QuestionBank b;
    AdaptiveSelector s(1);
    EXPECT_EQ(s.next(b), nullptr);
}

TEST(AdaptiveSelector, StartsEasy) {
    auto bank = makeBank(10, 5);
    AdaptiveSelector s(42);
    const Question* q = s.next(bank);
    ASSERT_NE(q, nullptr);
    EXPECT_LE(q->difficulty, 4);
}

TEST(AdaptiveSelector, TargetRisesOnFastCorrectAnswers) {
    auto bank = makeBank(10, 5);
    AdaptiveSelector s(1);
    const double before = s.targetDifficulty();
    for (int i = 0; i < 10; ++i) {
        const Question* q = s.next(bank);
        s.record(*q, Outcome::Correct, 1.4);
    }
    EXPECT_GT(s.targetDifficulty(), before + 2.0);
}

TEST(AdaptiveSelector, TargetFallsOnMissesAndSkipsButNotBelowOne) {
    auto bank = makeBank(10, 5);
    AdaptiveSelector s(1);
    for (int i = 0; i < 10; ++i) {
        const Question* q = s.next(bank);
        s.record(*q, Outcome::Correct, 1.4);
    }
    const double high = s.targetDifficulty();
    const Question* q = s.next(bank);
    s.record(*q, Outcome::Wrong, 0.0);
    EXPECT_LT(s.targetDifficulty(), high);

    for (int i = 0; i < 40; ++i) {
        const Question* n = s.next(bank);
        s.record(*n, i % 2 ? Outcome::Skipped : Outcome::Wrong, 0.0);
    }
    EXPECT_DOUBLE_EQ(s.targetDifficulty(), 1.0);
}

TEST(AdaptiveSelector, QuestionsClusterNearTarget) {
    auto bank = makeBank(10, 10);
    AdaptiveSelector s(7);
    // Drive the target up to the ceiling.
    for (int i = 0; i < 40; ++i) s.record(*s.next(bank), Outcome::Correct, 1.4);
    ASSERT_GE(s.targetDifficulty(), 9.9);

    double sum = 0;
    const int n = 30;
    for (int i = 0; i < n; ++i) {
        const Question* q = s.next(bank);
        sum += q->difficulty;
        // Keep the target pinned by answering fast.
        s.record(*q, Outcome::Correct, 1.4);
    }
    EXPECT_GT(sum / n, 7.0);
}

TEST(AdaptiveSelector, NoRepeatsWithinWindow) {
    auto bank = makeBank(3, 4);  // 12 questions
    AdaptiveSelector s(99);
    std::vector<std::string> history;
    for (int i = 0; i < 200; ++i) {
        const Question* q = s.next(bank);
        for (std::size_t back = 0; back < 5 && back < history.size(); ++back) {
            ASSERT_NE(q->id, history[history.size() - 1 - back]) << "repeat at step " << i;
        }
        history.push_back(q->id);
        s.record(*q, Outcome::Correct, 1.0);
    }
}

TEST(AdaptiveSelector, TinyBanksStillWork) {
    QuestionBank one;
    one.add(makeQuestion("only"));
    AdaptiveSelector s(1);
    for (int i = 0; i < 5; ++i) {
        const Question* q = s.next(one);
        ASSERT_NE(q, nullptr);
        s.record(*q, Outcome::Wrong, 0.0);
    }

    QuestionBank two;
    two.add(makeQuestion("a"));
    two.add(makeQuestion("b"));
    AdaptiveSelector s2(1);
    const Question* first = s2.next(two);
    const Question* second = s2.next(two);
    EXPECT_NE(first->id, second->id);
}

TEST(AdaptiveSelector, MissedQuestionComesBackSoon) {
    auto bank = makeBank(5, 10);  // plenty of other questions
    AdaptiveSelector s(5);
    const Question* missed = s.next(bank);
    const std::string id = missed->id;
    s.record(*missed, Outcome::Wrong, 0.0);

    bool seenAgain = false;
    for (int i = 0; i < 4 && !seenAgain; ++i) {
        const Question* q = s.next(bank);
        if (q->id == id) seenAgain = true;
        else s.record(*q, Outcome::Correct, 1.0);
    }
    EXPECT_TRUE(seenAgain);
}

TEST(AdaptiveSelector, MissedQuestionIsNotRequeuedTwice) {
    auto bank = makeBank(2, 10);
    AdaptiveSelector s(5);
    const Question* q = s.next(bank);
    const std::string id = q->id;
    s.record(*q, Outcome::Wrong, 0.0);
    s.record(*q, Outcome::Wrong, 0.0);  // double-record must not double-queue

    int appearances = 0;
    for (int i = 0; i < 4; ++i) {
        const Question* n = s.next(bank);
        if (n->id == id) ++appearances;
        s.record(*n, Outcome::Correct, 1.0);
    }
    EXPECT_EQ(appearances, 1);
}

TEST(AdaptiveSelector, VarietyOverTime) {
    auto bank = makeBank(4, 6);  // 24 questions
    AdaptiveSelector s(3);
    std::set<std::string> seen;
    for (int i = 0; i < 60; ++i) {
        const Question* q = s.next(bank);
        seen.insert(q->id);
        s.record(*q, Outcome::Correct, 1.0);
    }
    EXPECT_GT(seen.size(), 12u);
}
