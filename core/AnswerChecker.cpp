#include "AnswerChecker.h"

#include <cctype>

namespace cq {

namespace {

bool isSpace(char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }

bool isWordChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

std::string trim(const std::string& s) {
    std::size_t b = 0, e = s.size();
    while (b < e && isSpace(s[b])) ++b;
    while (e > b && isSpace(s[e - 1])) --e;
    return s.substr(b, e - b);
}

}  // namespace

std::string normalize(const std::string& text, Strictness strictness) {
    std::string s = trim(text);
    if (strictness == Strictness::Exact) return s;

    // Collapse whitespace runs into a single space.
    std::string collapsed;
    for (char c : s) {
        if (isSpace(c)) {
            if (collapsed.empty() || collapsed.back() != ' ') collapsed.push_back(' ');
        } else {
            collapsed.push_back(c);
        }
    }

    // Keep a space only where it separates two identifier characters.
    std::string out;
    for (std::size_t i = 0; i < collapsed.size(); ++i) {
        if (collapsed[i] == ' ') {
            bool keep = !out.empty() && i + 1 < collapsed.size() &&
                        isWordChar(out.back()) && isWordChar(collapsed[i + 1]);
            if (!keep) continue;
        }
        out.push_back(collapsed[i]);
    }
    return out;
}

bool isCorrect(const Question& q, const std::string& input, Strictness strictness) {
    const std::string given = normalize(input, strictness);
    for (const auto& a : q.answers) {
        if (normalize(a, strictness) == given) return true;
    }
    return false;
}

}  // namespace cq
