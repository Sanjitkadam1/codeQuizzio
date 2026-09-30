#pragma once

#include <string>

#include "Question.h"

namespace cq {

enum class Strictness {
    // Only leading/trailing whitespace is ignored.
    Exact,
    // Whitespace runs are collapsed, and whitespace next to punctuation is
    // dropped, so `int x=5;` matches `int x = 5;`. Whitespace between two
    // identifier characters is kept, so `intx` never matches `int x`.
    IgnoreWhitespace,
};

std::string normalize(const std::string& text, Strictness strictness);

bool isCorrect(const Question& q, const std::string& input,
               Strictness strictness = Strictness::IgnoreWhitespace);

}  // namespace cq
