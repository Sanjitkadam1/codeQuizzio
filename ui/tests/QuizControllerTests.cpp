#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QThread>

#include "QuizController.h"

namespace {

// Pump the Qt event loop for `ms` (lets the controller's QTimers fire).
void wait(int ms) {
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(5);
    }
}

// The fixture pack has exactly one question: answer "return 0;".
class QuizControllerTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        qputenv("CQ_QUESTIONS_DIR", CQ_TEST_QUESTIONS_DIR);
        static int argc = 1;
        static char arg0[] = "cq_ui_tests";
        static char* argv[] = {arg0, nullptr};
        if (!QCoreApplication::instance()) new QCoreApplication(argc, argv);
    }
};

}  // namespace

TEST_F(QuizControllerTest, StartsAskingWithAQuestion) {
    QuizController c;
    EXPECT_EQ(c.loadError(), "");
    EXPECT_EQ(c.state(), QuizController::Asking);
    EXPECT_EQ(c.prompt(), "Return 0 from a function");
    EXPECT_GT(c.par(), 0.0);
    EXPECT_FALSE(c.paused());
}

TEST_F(QuizControllerTest, TimerRunsFromWhenQuestionAppears) {
    QuizController c;
    wait(300);  // no keystrokes at all
    EXPECT_GT(c.elapsed(), 0.2);
}

TEST_F(QuizControllerTest, CorrectAnswerScoresThenAutoAdvances) {
    QuizController c;
    c.submit("return 0;");
    EXPECT_EQ(c.state(), QuizController::Correct);
    EXPECT_GT(c.lastPoints(), 0);
    EXPECT_EQ(c.score(), c.lastPoints());
    EXPECT_EQ(c.streak(), 1);

    wait(1200);  // flash (900ms) then next question
    EXPECT_EQ(c.state(), QuizController::Asking);
    EXPECT_LT(c.elapsed(), 0.5);  // clock restarted for the new question
}

TEST_F(QuizControllerTest, EnterSkipsTheCorrectFlash) {
    QuizController c;
    c.submit("return 0;");
    c.submit("");  // Enter on the feedback screen
    EXPECT_EQ(c.state(), QuizController::Asking);
}

TEST_F(QuizControllerTest, WrongAnswerRequiresRetypeWithNoPoints) {
    QuizController c;
    c.submit("return 1;");
    EXPECT_EQ(c.state(), QuizController::Missed);
    EXPECT_EQ(c.answerHint(), "return 0;");
    EXPECT_EQ(c.score(), 0);
    EXPECT_EQ(c.wrongCount(), 1);

    QSignalSpy rejected(&c, &QuizController::retypeRejected);
    c.submit("nope");
    EXPECT_EQ(c.state(), QuizController::Missed);
    EXPECT_EQ(rejected.count(), 1);

    c.submit("return 0;");
    EXPECT_EQ(c.state(), QuizController::Asking);
    EXPECT_EQ(c.score(), 0);  // retyping is practice, not points
}

TEST_F(QuizControllerTest, SkipScoresNothingAndShowsAnswer) {
    QuizController c;
    c.skip();
    EXPECT_EQ(c.state(), QuizController::Skipped);
    EXPECT_EQ(c.score(), 0);
    EXPECT_EQ(c.skippedCount(), 1);
    EXPECT_EQ(c.answerHint(), "return 0;");

    c.submit("");  // Enter continues
    EXPECT_EQ(c.state(), QuizController::Asking);
}

TEST_F(QuizControllerTest, PauseFreezesTheClockAndBlocksAnswering) {
    QuizController c;
    wait(300);
    c.togglePause();
    EXPECT_TRUE(c.paused());
    const double frozen = c.elapsed();

    wait(600);
    EXPECT_NEAR(c.elapsed(), frozen, 0.06);  // clock did not move while paused

    c.submit("return 0;");  // ignored while paused
    c.skip();               // ignored while paused
    EXPECT_EQ(c.state(), QuizController::Asking);
    EXPECT_EQ(c.score(), 0);

    c.togglePause();
    EXPECT_FALSE(c.paused());
    c.submit("return 0;");
    EXPECT_EQ(c.state(), QuizController::Correct);
    // ~0.3s played, 0.6s paused: the score must reflect only the played time.
    EXPECT_LT(c.elapsed(), 0.5);
}

TEST_F(QuizControllerTest, EmptySubmitWhileAskingIsIgnored) {
    QuizController c;
    c.submit("   ");
    EXPECT_EQ(c.state(), QuizController::Asking);
    EXPECT_EQ(c.wrongCount(), 0);
}

TEST_F(QuizControllerTest, EndSessionAndRestart) {
    QuizController c;
    c.submit("return 0;");
    c.endSession();
    EXPECT_EQ(c.state(), QuizController::Finished);
    EXPECT_EQ(c.correctCount(), 1);

    c.restart();
    EXPECT_EQ(c.state(), QuizController::Asking);
    EXPECT_EQ(c.score(), 0);
    EXPECT_EQ(c.correctCount(), 0);
}

TEST_F(QuizControllerTest, ClockRestartsOnTheQuestionAfterASkip) {
    QuizController c;
    wait(200);
    c.skip();
    EXPECT_EQ(c.state(), QuizController::Skipped);
    wait(300);  // reading the answer must not leak into the next question's time

    c.submit("");  // Enter on the skipped card
    ASSERT_EQ(c.state(), QuizController::Asking);
    EXPECT_LT(c.elapsed(), 0.1);
    wait(300);
    EXPECT_GT(c.elapsed(), 0.2);  // and it is actually ticking
}

TEST_F(QuizControllerTest, SkippedCardKeepsTheQuestionAndAnswerAvailable) {
    QuizController c;
    const QString prompt = c.prompt();
    c.skip();
    EXPECT_EQ(c.prompt(), prompt);
    EXPECT_EQ(c.answerHint(), "return 0;");
}
