#pragma once

#include <string>
#include <vector>

#include "Question.h"
#include "QuestionBank.h"

inline cq::Question makeQuestion(const std::string& id, int difficulty = 1,
                                 std::vector<std::string> answers = {"x"}) {
    cq::Question q;
    q.id = id;
    q.prompt = "prompt " + id;
    q.answers = std::move(answers);
    q.difficulty = difficulty;
    return q;
}

// Bank with `perLevel` questions at each difficulty 1..levels.
inline cq::QuestionBank makeBank(int levels, int perLevel) {
    cq::QuestionBank bank;
    for (int d = 1; d <= levels; ++d) {
        for (int i = 0; i < perLevel; ++i) {
            bank.add(makeQuestion("q" + std::to_string(d) + "_" + std::to_string(i), d));
        }
    }
    return bank;
}
