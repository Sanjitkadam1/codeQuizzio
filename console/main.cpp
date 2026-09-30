// Text-mode harness for the core library. Lets us play and tune the game
// before the Qt UI exists.
//
//   cq_console [questions_dir]
//   type :skip to skip, :quit to stop.

#include <chrono>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>

#include "AdaptiveSelector.h"
#include "QuestionBank.h"
#include "Session.h"

namespace {

void printSummary(const cq::Session& s) {
    std::printf("\n--- Session over ---\n");
    std::printf("Score: %d   Best streak: %d\n", s.score(), s.bestStreak());
    std::printf("Correct: %d   Wrong: %d   Skipped: %d\n", s.correctCount(), s.wrongCount(),
                s.skippedCount());
}

}  // namespace

int main(int argc, char** argv) {
    const std::string dir = argc > 1 ? argv[1] : CQ_DEFAULT_QUESTIONS_DIR;

    cq::QuestionBank bank;
    try {
        bank.loadFromDirectory(dir);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Failed to load questions from %s: %s\n", dir.c_str(), e.what());
        return 1;
    }
    if (bank.empty()) {
        std::fprintf(stderr, "No questions found in %s\n", dir.c_str());
        return 1;
    }
    std::printf("Loaded %zu questions. Type :skip to skip, :quit to stop.\n", bank.size());

    cq::Session session(bank, std::make_unique<cq::AdaptiveSelector>());

    while (const cq::Question* q = session.nextQuestion()) {
        std::printf("\n[d%d] %s\n> ", q->difficulty, q->prompt.c_str());
        std::fflush(stdout);

        // Timer starts the moment the question is shown.
        const auto start = std::chrono::steady_clock::now();
        std::string line;
        if (!std::getline(std::cin, line) || line == ":quit") break;
        const double elapsed =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

        const cq::AttemptResult r = line == ":skip" ? session.skip() : session.submit(line, elapsed);
        switch (r.outcome) {
            case cq::Outcome::Correct:
                std::printf("Correct! +%d  (%.1fs vs par %.1fs, x%.2f speed, x%.2f streak)  total %d\n",
                            r.points, elapsed, r.breakdown.parSeconds, r.breakdown.speedMultiplier,
                            r.breakdown.streakMultiplier, session.score());
                break;
            case cq::Outcome::Wrong:
                std::printf("Wrong. Answer: %s\n", q->answers.front().c_str());
                break;
            case cq::Outcome::Skipped:
                std::printf("Skipped (+0). Answer: %s\n", q->answers.front().c_str());
                break;
        }
    }
    printSummary(session);
    return 0;
}
