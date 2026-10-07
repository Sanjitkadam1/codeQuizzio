#include "QuizController.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

#include <exception>
#include <random>

#include "AdaptiveSelector.h"
#include "AnswerChecker.h"
#include "ProgressStore.h"

namespace {
constexpr int kTickMs = 50;
constexpr int kCorrectFlashMs = 900;
}  // namespace

QuizController::QuizController(QObject* parent) : QObject(parent) {
    tick_.setInterval(kTickMs);
    connect(&tick_, &QTimer::timeout, this, &QuizController::updateElapsed);
    autoAdvance_.setSingleShot(true);
    autoAdvance_.setInterval(kCorrectFlashMs);
    connect(&autoAdvance_, &QTimer::timeout, this, &QuizController::advance);

    loadQuestions();
    loadProgress();
    loadSelectorConfig();
    restart();
}

QuizController::~QuizController() {
    // Closing the window mid-session still counts the session.
    finishSession();
}

void QuizController::loadProgress() {
    // CQ_PROGRESS_FILE overrides the location (used by tests and for debugging).
    QString path = qEnvironmentVariable("CQ_PROGRESS_FILE");
    if (path.isEmpty()) {
        path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/progress.json";
    }
    progressPath_ = std::filesystem::path(path.toStdU16String());
    qInfo().noquote() << "Progress file:" << path;

    cq::LoadResult loaded = cq::loadProgress(progressPath_);
    progress_ = std::move(loaded.progress);
    if (!loaded.warning.empty()) {
        progressWarning_ = QString::fromStdString(loaded.warning);
        qWarning().noquote() << "Progress:" << progressWarning_;
    }
}

void QuizController::loadSelectorConfig() {
    // CQ_SELECTOR_CONFIG overrides the file; otherwise config/selector.json next
    // to the app, then the one in the source tree. No file at all means defaults.
    const QString override = qEnvironmentVariable("CQ_SELECTOR_CONFIG");
    QStringList candidates;
    if (!override.isEmpty()) {
        if (!QFile::exists(override)) {
            configWarning_ = "CQ_SELECTOR_CONFIG points to a missing file: " + override +
                             "; using default selector settings";
            qWarning().noquote() << "Selector:" << configWarning_;
            return;
        }
        candidates << override;
    }
    candidates << QCoreApplication::applicationDirPath() + "/config/selector.json"
               << QStringLiteral(CQ_DEFAULT_CONFIG_DIR) + "/selector.json";

    for (const QString& path : candidates) {
        if (!QFile::exists(path)) continue;
        cq::LoadedSelectorConfig loaded = cq::loadSelectorConfig(std::filesystem::path(path.toStdU16String()));
        selectorConfig_ = loaded.config;
        if (!loaded.warning.empty()) {
            configWarning_ = QString::fromStdString(loaded.warning);
            qWarning().noquote() << "Selector:" << configWarning_;
        } else {
            qInfo().noquote() << "Selector settings:" << path;
        }
        return;
    }
}

void QuizController::saveProgress() {
    try {
        cq::saveProgress(progressPath_, progress_);
    } catch (const std::exception& e) {
        // Never interrupt the game over a failed save; the next answer retries.
        qWarning().noquote() << "Could not save progress:" << e.what();
    }
}

void QuizController::finishSession() {
    if (!session_) return;
    // Answers were already saved one by one; only a newly logged session needs a save.
    if (session_->finish().recorded) saveProgress();
}
void QuizController::loadQuestions() {
    QStringList candidates = {
        QCoreApplication::applicationDirPath() + "/questions",
        QStringLiteral(CQ_DEFAULT_QUESTIONS_DIR),
    };
    // CQ_QUESTIONS_DIR lets you point the app at your own packs (and tests at fixtures).
    const QString override = qEnvironmentVariable("CQ_QUESTIONS_DIR");
    if (!override.isEmpty()) candidates.prepend(override);
    for (const QString& dir : candidates) {
        if (!QDir(dir).exists()) continue;
        try {
            bank_.loadFromDirectory(dir.toStdString());
            if (bank_.empty()) loadError_ = "No questions found in " + dir;
        } catch (const std::exception& e) {
            loadError_ = QString("Could not load questions: ") + e.what();
        }
        return;
    }
    loadError_ = "Questions folder not found (looked in: " + candidates.join(", ") + ")";
}

void QuizController::restart() {
    autoAdvance_.stop();
    tick_.stop();
    paused_ = false;
    emit pausedChanged();

    finishSession();  // no-op if the previous session was already finished
    if (bank_.empty()) {
        session_.reset();
        setState(Finished);
        emit statsChanged();
        return;
    }
    cq::SelectorContext context;
    context.progress = &progress_;  // the selector reads everything saved so far
    context.scoring = scoring_;
    session_ = std::make_unique<cq::Session>(
        bank_, std::make_unique<cq::AdaptiveSelector>(std::random_device{}(), selectorConfig_, context),
        scoring_);
    session_->attachProgress(&progress_);
    emit statsChanged();
    startQuestion();
}

void QuizController::startQuestion() {
    const cq::Question* q = session_->nextQuestion();
    prompt_ = QString::fromStdString(q->prompt);
    difficulty_ = q->difficulty;
    par_ = scoring_.parSeconds(*q);
    emit questionChanged();

    paused_ = false;
    emit pausedChanged();
    accumulatedMs_ = 0;
    elapsed_ = 0.0;
    emit elapsedChanged();

    // The clock starts the moment the question is shown.
    running_.start();
    clockRunning_ = true;
    tick_.start();
    setState(Asking);
}

double QuizController::currentElapsed() const {
    qint64 ms = accumulatedMs_;
    if (clockRunning_) ms += running_.elapsed();
    return static_cast<double>(ms) / 1000.0;
}

void QuizController::stopClock() {
    if (clockRunning_) accumulatedMs_ += running_.elapsed();
    clockRunning_ = false;
    tick_.stop();
}

void QuizController::updateElapsed() {
    if (state_ != Asking || !clockRunning_) return;
    elapsed_ = currentElapsed();
    emit elapsedChanged();
}

void QuizController::setState(State s) {
    if (state_ == s) return;
    state_ = s;
    emit stateChanged();
}

void QuizController::submit(const QString& text) {
    switch (state_) {
        case Asking: {
            if (paused_ || text.trimmed().isEmpty()) return;
            const double t = currentElapsed();
            stopClock();
            elapsed_ = t;
            emit elapsedChanged();

            const cq::AttemptResult r = session_->submit(text.toStdString(), t);
            saveProgress();
            lastQuestion_ = r.question;
            if (r.outcome == cq::Outcome::Correct) {
                lastPoints_ = r.points;
                lastSpeed_ = r.breakdown.speedMultiplier;
                lastStreak_ = r.breakdown.streakMultiplier;
                answerHint_.clear();
                emit feedbackChanged();
                emit statsChanged();
                setState(Correct);
                autoAdvance_.start();
            } else {
                lastPoints_ = 0;
                answerHint_ = QString::fromStdString(r.question->answers.front());
                emit feedbackChanged();
                emit statsChanged();
                setState(Missed);
            }
            break;
        }
        case Missed:
            // Retyping the right answer earns nothing but builds the muscle memory.
            if (cq::isCorrect(*lastQuestion_, text.toStdString())) {
                advance();
            } else {
                emit retypeRejected();
            }
            break;
        case Correct:
        case Skipped:
            advance();
            break;
        case Finished:
            break;
    }
}

void QuizController::skip() {
    if (state_ != Asking || paused_) return;
    stopClock();
    const cq::AttemptResult r = session_->skip();
    saveProgress();
    lastQuestion_ = r.question;
    lastPoints_ = 0;
    answerHint_ = QString::fromStdString(r.question->answers.front());
    emit feedbackChanged();
    emit statsChanged();
    setState(Skipped);
}

void QuizController::togglePause() {
    if (state_ != Asking) return;
    if (paused_) {
        running_.restart();
        clockRunning_ = true;
        paused_ = false;
    } else {
        accumulatedMs_ += running_.elapsed();
        clockRunning_ = false;
        paused_ = true;
    }
    emit pausedChanged();
}

void QuizController::advance() {
    autoAdvance_.stop();
    if (state_ == Correct || state_ == Missed || state_ == Skipped) startQuestion();
}

void QuizController::endSession() {
    autoAdvance_.stop();
    stopClock();
    finishSession();
    paused_ = false;
    emit pausedChanged();
    setState(Finished);
    emit statsChanged();
}
