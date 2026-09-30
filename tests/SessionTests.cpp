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
