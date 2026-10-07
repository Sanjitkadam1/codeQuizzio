#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QThread>

#include <filesystem>
#include <fstream>

#include "ProgressStore.h"
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

    // Every test gets its own progress file in a temp dir, so tests never touch
    // the real save and never see each other's data.
    void SetUp() override {
        dir = std::filesystem::temp_directory_path() /
              (std::string("cq_ui_test_") +
               ::testing::UnitTest::GetInstance()->current_test_info()->name());
        std::filesystem::remove_all(dir);
        progressFile = dir / "progress.json";
        qputenv("CQ_PROGRESS_FILE", QByteArray::fromStdString(progressFile.string()));
    }
    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    }

    std::filesystem::path dir, progressFile;
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

TEST_F(QuizControllerTest, EveryAnswerIsSavedImmediately) {
    QuizController c;
    c.submit("return 1;");  // wrong
    {
        const auto loaded = cq::loadProgress(progressFile);  // controller still running
        ASSERT_EQ(loaded.source, cq::LoadResult::Source::Primary);
        ASSERT_NE(loaded.progress.stats("only"), nullptr);
        EXPECT_EQ(loaded.progress.stats("only")->misses, 1);
    }
    c.submit("return 0;");  // retype: practice only, not a new attempt
    c.skip();
    c.submit("");           // Enter continues past the skipped screen
    c.submit("return 0;");  // correct
    const auto loaded = cq::loadProgress(progressFile);
    const cq::QuestionStats* s = loaded.progress.stats("only");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->attempts, 3);
    EXPECT_EQ(s->misses, 1);
    EXPECT_EQ(s->skips, 1);
    EXPECT_EQ(s->correct, 1);
}

TEST_F(QuizControllerTest, SkipIsSaved) {
    QuizController c;
    c.skip();
    const auto loaded = cq::loadProgress(progressFile);
    ASSERT_NE(loaded.progress.stats("only"), nullptr);
    EXPECT_EQ(loaded.progress.stats("only")->skips, 1);
}

TEST_F(QuizControllerTest, EndingASessionLogsItWithRecords) {
    QuizController c;
    c.submit("return 0;");
    c.endSession();
    const auto loaded = cq::loadProgress(progressFile);
    EXPECT_EQ(loaded.progress.totalSessions(), 1);
    ASSERT_EQ(loaded.progress.history().size(), 1u);
    EXPECT_EQ(loaded.progress.history()[0].correct, 1);
    EXPECT_GT(loaded.progress.highScore(), 0);
    EXPECT_EQ(loaded.progress.bestStreak(), 1);
}

TEST_F(QuizControllerTest, EndingTwiceDoesNotLogTwice) {
    QuizController c;
    c.submit("return 0;");
    c.endSession();
    c.endSession();
    c.restart();  // also finishes the old session, which is already finished
    EXPECT_EQ(cq::loadProgress(progressFile).progress.totalSessions(), 1);
}

TEST_F(QuizControllerTest, ClosingMidSessionStillLogsTheSession) {
    {
        QuizController c;
        c.submit("return 0;");
        // window closed without pressing End session
    }
    EXPECT_EQ(cq::loadProgress(progressFile).progress.totalSessions(), 1);
}

TEST_F(QuizControllerTest, UnansweredSessionIsNotLogged) {
    {
        QuizController c;  // question shown, nothing answered
        c.endSession();
    }
    EXPECT_EQ(cq::loadProgress(progressFile).progress.totalSessions(), 0);
}

TEST_F(QuizControllerTest, ProgressCarriesOverToTheNextLaunch) {
    {
        QuizController c;
        c.submit("return 0;");
        c.endSession();
    }
    QuizController again;
    again.submit("return 0;");
    again.endSession();

    const auto loaded = cq::loadProgress(progressFile);
    EXPECT_EQ(loaded.progress.totalSessions(), 2);
    EXPECT_EQ(loaded.progress.stats("only")->correct, 2);
    EXPECT_EQ(loaded.progress.history().size(), 2u);
}

TEST_F(QuizControllerTest, CorruptSaveIsReportedAndGameStillRuns) {
    std::filesystem::create_directories(dir);
    std::ofstream(progressFile) << "{ definitely not json";

    QuizController c;
    EXPECT_FALSE(c.progressWarning().isEmpty());
    EXPECT_EQ(c.state(), QuizController::Asking);
    c.submit("return 0;");
    EXPECT_EQ(c.state(), QuizController::Correct);
    // The next save replaced the bad file with a good one.
    EXPECT_EQ(cq::loadProgress(progressFile).source, cq::LoadResult::Source::Primary);
}

TEST_F(QuizControllerTest, RecoversFromBackupAfterDamage) {
    {
        QuizController c;
        c.submit("return 0;");  // save 1
        c.submit("");           // continue
        c.submit("return 0;");  // save 2 (backup now holds save 1)
    }
    std::ofstream(progressFile, std::ios::trunc) << "garbage";

    QuizController c;
    EXPECT_FALSE(c.progressWarning().isEmpty());
    c.endSession();
    EXPECT_GE(cq::loadProgress(progressFile).progress.stats("only")->attempts, 1);
}

TEST_F(QuizControllerTest, NoWarningOnFirstRun) {
    QuizController c;
    EXPECT_TRUE(c.progressWarning().isEmpty());
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
