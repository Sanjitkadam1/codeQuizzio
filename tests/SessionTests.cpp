#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>

#include "AdaptiveSelector.h"
#include "Session.h"
#include "TestUtil.h"

using namespace cq;

namespace {

// Serves questions in bank order and records feedback for inspection.
class ScriptedSelector : public Selector {
public:
    const Question* next(const QuestionBank& bank) override {
        if (bank.empty()) return nullptr;
        return &bank.all()[index_++ % bank.size()];
    }
    void record(const Question& q, Outcome o, double) override { log.emplace_back(q.id, o); }
    std::vector<std::pair<std::string, Outcome>> log;

private:
    std::size_t index_ = 0;
};

struct Fixture {
    QuestionBank bank;
    ScriptedSelector* selector = nullptr;
    std::unique_ptr<Session> session;

    Fixture() {
        bank.add(makeQuestion("a", 1, {"aaa"}));
        bank.add(makeQuestion("b", 2, {"bbb"}));
        bank.add(makeQuestion("c", 3, {"ccc"}));
        auto sel = std::make_unique<ScriptedSelector>();
        selector = sel.get();
        session = std::make_unique<Session>(bank, std::move(sel));
    }
};

}  // namespace

TEST(Session, CorrectAnswerScoresAndBuildsStreak) {
    Fixture f;
    ASSERT_NE(f.session->nextQuestion(), nullptr);
    auto r1 = f.session->submit("aaa", 1.0);
    EXPECT_EQ(r1.outcome, Outcome::Correct);
    EXPECT_GT(r1.points, 0);
    EXPECT_EQ(f.session->streak(), 1);

    f.session->nextQuestion();
    auto r2 = f.session->submit("bbb", 1.0);
    EXPECT_EQ(f.session->streak(), 2);
    EXPECT_EQ(f.session->score(), r1.points + r2.points);
    EXPECT_EQ(f.session->correctCount(), 2);
}

TEST(Session, WrongAnswerScoresZeroAndResetsStreak) {
    Fixture f;
    f.session->nextQuestion();
    f.session->submit("aaa", 1.0);
    const int scoreBefore = f.session->score();

    f.session->nextQuestion();
    auto r = f.session->submit("nope", 1.0);
    EXPECT_EQ(r.outcome, Outcome::Wrong);
    EXPECT_EQ(r.points, 0);
    EXPECT_EQ(f.session->score(), scoreBefore);
    EXPECT_EQ(f.session->streak(), 0);
    EXPECT_EQ(f.session->bestStreak(), 1);
    EXPECT_EQ(f.session->wrongCount(), 1);
}

TEST(Session, SkipIsZeroPointsAndResetsStreak) {
    Fixture f;
    f.session->nextQuestion();
    f.session->submit("aaa", 1.0);
    f.session->nextQuestion();
    auto r = f.session->skip();
    EXPECT_EQ(r.outcome, Outcome::Skipped);
    EXPECT_EQ(r.points, 0);
    EXPECT_EQ(f.session->streak(), 0);
    EXPECT_EQ(f.session->skippedCount(), 1);
}

TEST(Session, SelectorReceivesFeedback) {
    Fixture f;
    f.session->nextQuestion();
    f.session->submit("aaa", 1.0);
    f.session->nextQuestion();
    f.session->skip();
    ASSERT_EQ(f.selector->log.size(), 2u);
    EXPECT_EQ(f.selector->log[0].second, Outcome::Correct);
    EXPECT_EQ(f.selector->log[1].second, Outcome::Skipped);
}

TEST(Session, SubmitWithoutQuestionThrows) {
    Fixture f;
    EXPECT_THROW(f.session->submit("x", 1.0), std::logic_error);
    EXPECT_THROW(f.session->skip(), std::logic_error);

    f.session->nextQuestion();
    f.session->submit("aaa", 1.0);
    EXPECT_THROW(f.session->submit("aaa", 1.0), std::logic_error);  // already answered
}

TEST(Session, FasterAnswersScoreHigher) {
    Fixture f1, f2;
    f1.session->nextQuestion();
    f2.session->nextQuestion();
    EXPECT_GT(f1.session->submit("aaa", 0.5).points, f2.session->submit("aaa", 20.0).points);
}

TEST(Session, RecordsEveryAnswerIntoProgress) {
    Fixture f;
    Progress progress;
    std::int64_t now = 1000;
    f.session->attachProgress(&progress, [&] { return now++; });

    f.session->nextQuestion();  // a
    f.session->submit("aaa", 2.0);
    f.session->nextQuestion();  // b
    f.session->submit("wrong", 5.0);
    f.session->nextQuestion();  // c
    f.session->skip();

    ASSERT_NE(progress.stats("a"), nullptr);
    EXPECT_EQ(progress.stats("a")->correct, 1);
    EXPECT_DOUBLE_EQ(progress.stats("a")->bestTime, 2.0);
    EXPECT_EQ(progress.stats("a")->lastSeen, 1000);
    EXPECT_EQ(progress.stats("b")->misses, 1);
    EXPECT_EQ(progress.stats("c")->skips, 1);
}

TEST(Session, WithoutProgressNothingIsRecorded) {
    Fixture f;
    f.session->nextQuestion();
    f.session->submit("aaa", 1.0);
    auto summary = f.session->finish();
    EXPECT_FALSE(summary.recorded);
    EXPECT_EQ(summary.record.correct, 1);
}

TEST(Session, FinishLogsTheSessionOnce) {
    Fixture f;
    Progress progress;
    f.session->attachProgress(&progress, [] { return std::int64_t{5000}; });

    f.session->nextQuestion();
    f.session->submit("aaa", 1.0);
    f.session->nextQuestion();
    f.session->submit("bbb", 1.0);
    f.session->nextQuestion();
    f.session->submit("nope", 1.0);

    auto first = f.session->finish();
    EXPECT_TRUE(first.recorded);
    EXPECT_TRUE(first.newHighScore);
    EXPECT_TRUE(first.newBestStreak);
    EXPECT_EQ(first.record.endedAt, 5000);
    EXPECT_EQ(first.record.correct, 2);
    EXPECT_EQ(first.record.wrong, 1);
    EXPECT_EQ(first.record.bestStreak, 2);
    EXPECT_EQ(first.record.score, f.session->score());

    auto second = f.session->finish();  // e.g. ended manually, then app closed
    EXPECT_EQ(second.record.score, first.record.score);
    EXPECT_EQ(progress.totalSessions(), 1);
    EXPECT_EQ(progress.history().size(), 1u);
}

TEST(Session, EmptySessionIsNotRecorded) {
    Fixture f;
    Progress progress;
    f.session->attachProgress(&progress);
    f.session->nextQuestion();  // shown but never answered
    EXPECT_FALSE(f.session->finish().recorded);
    EXPECT_EQ(progress.totalSessions(), 0);
}

TEST(Session, SecondSessionCanSetNoRecord) {
    Progress progress;
    auto bank = makeBank(3, 3);

    Session strong(bank, std::make_unique<AdaptiveSelector>(1));
    strong.attachProgress(&progress);
    for (int i = 0; i < 5; ++i) strong.submit(strong.nextQuestion()->answers.front(), 0.5);
    EXPECT_TRUE(strong.finish().newHighScore);

    Session weak(bank, std::make_unique<AdaptiveSelector>(2));
    weak.attachProgress(&progress);
    weak.nextQuestion();
    weak.skip();
    auto summary = weak.finish();
    EXPECT_TRUE(summary.recorded);
    EXPECT_FALSE(summary.newHighScore);
    EXPECT_EQ(progress.totalSessions(), 2);
}

TEST(Session, WorksEndToEndWithAdaptiveSelector) {
    auto bank = makeBank(5, 6);
    Session s(bank, std::make_unique<AdaptiveSelector>(123));
    for (int i = 0; i < 50; ++i) {
        const Question* q = s.nextQuestion();
        ASSERT_NE(q, nullptr);
        if (i % 7 == 0) s.skip();
        else s.submit(i % 5 == 0 ? "wrong" : q->answers.front(), 2.0);
    }
    EXPECT_EQ(s.correctCount() + s.wrongCount() + s.skippedCount(), 50);
    EXPECT_GT(s.score(), 0);
}
