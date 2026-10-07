#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QtQml/qqml.h>

#include <filesystem>
#include <memory>

#include "Progress.h"
#include "QuestionBank.h"
#include "Scoring.h"
#include "Session.h"

// Bridges the pure-C++ core (Session) to QML. Owns the clock: it starts when a
// question appears, stops on submit/skip, and does not advance while paused.
class QuizController : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool paused READ paused NOTIFY pausedChanged)
    Q_PROPERTY(QString loadError READ loadError CONSTANT)
    // Non-empty if saved progress had to be recovered or reset at startup.
    Q_PROPERTY(QString progressWarning READ progressWarning CONSTANT)

    Q_PROPERTY(QString prompt READ prompt NOTIFY questionChanged)
    Q_PROPERTY(int difficulty READ difficulty NOTIFY questionChanged)
    Q_PROPERTY(double par READ par NOTIFY questionChanged)
    Q_PROPERTY(double elapsed READ elapsed NOTIFY elapsedChanged)

    Q_PROPERTY(QString answerHint READ answerHint NOTIFY feedbackChanged)
    Q_PROPERTY(int lastPoints READ lastPoints NOTIFY feedbackChanged)
    Q_PROPERTY(double lastSpeedMultiplier READ lastSpeedMultiplier NOTIFY feedbackChanged)
    Q_PROPERTY(double lastStreakMultiplier READ lastStreakMultiplier NOTIFY feedbackChanged)

    Q_PROPERTY(int score READ score NOTIFY statsChanged)
    Q_PROPERTY(int streak READ streak NOTIFY statsChanged)
    Q_PROPERTY(int bestStreak READ bestStreak NOTIFY statsChanged)
    Q_PROPERTY(int correctCount READ correctCount NOTIFY statsChanged)
    Q_PROPERTY(int wrongCount READ wrongCount NOTIFY statsChanged)
    Q_PROPERTY(int skippedCount READ skippedCount NOTIFY statsChanged)

public:
    enum State {
        Asking,    // question on screen, clock running
        Correct,   // brief +points feedback, then auto-advances
        Missed,    // wrong: player must retype the correct answer to continue
        Skipped,   // +0, answer shown, Enter continues
        Finished,  // session summary
    };
    Q_ENUM(State)

    explicit QuizController(QObject* parent = nullptr);
    ~QuizController() override;

    State state() const { return state_; }
    bool paused() const { return paused_; }
    const QString& loadError() const { return loadError_; }
    const QString& progressWarning() const { return progressWarning_; }

    const QString& prompt() const { return prompt_; }
    int difficulty() const { return difficulty_; }
    double par() const { return par_; }
    double elapsed() const { return elapsed_; }

    const QString& answerHint() const { return answerHint_; }
    int lastPoints() const { return lastPoints_; }
    double lastSpeedMultiplier() const { return lastSpeed_; }
    double lastStreakMultiplier() const { return lastStreak_; }

    int score() const { return session_ ? session_->score() : 0; }
    int streak() const { return session_ ? session_->streak() : 0; }
    int bestStreak() const { return session_ ? session_->bestStreak() : 0; }
    int correctCount() const { return session_ ? session_->correctCount() : 0; }
    int wrongCount() const { return session_ ? session_->wrongCount() : 0; }
    int skippedCount() const { return session_ ? session_->skippedCount() : 0; }

    // Enter key: answers while Asking, retypes while Missed, continues otherwise.
    Q_INVOKABLE void submit(const QString& text);
    Q_INVOKABLE void skip();
    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void endSession();
    Q_INVOKABLE void restart();

signals:
    void stateChanged();
    void pausedChanged();
    void questionChanged();
    void elapsedChanged();
    void feedbackChanged();
    void statsChanged();
    void retypeRejected();

private:
    void loadQuestions();
    void loadProgress();
    void saveProgress();
    void finishSession();  // logs the session in Progress (once) and saves
    void startQuestion();
    void advance();
    void setState(State s);
    double currentElapsed() const;
    void stopClock();
    void updateElapsed();

    // Declared before session_: the session refers to both.
    cq::QuestionBank bank_;
    cq::Progress progress_;
    std::filesystem::path progressPath_;
    std::unique_ptr<cq::Session> session_;
    cq::Scoring scoring_;
    const cq::Question* lastQuestion_ = nullptr;

    State state_ = Finished;
    bool paused_ = false;
    QString loadError_;
    QString progressWarning_;

    QString prompt_;
    int difficulty_ = 1;
    double par_ = 0.0;
    double elapsed_ = 0.0;

    QString answerHint_;
    int lastPoints_ = 0;
    double lastSpeed_ = 0.0;
    double lastStreak_ = 1.0;

    QElapsedTimer running_;
    qint64 accumulatedMs_ = 0;
    bool clockRunning_ = false;
    QTimer tick_;
    QTimer autoAdvance_;
};
