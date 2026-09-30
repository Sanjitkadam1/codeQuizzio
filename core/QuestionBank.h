#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "Question.h"

namespace cq {

// Holds all loaded questions. Loading functions throw std::runtime_error on
// malformed data or duplicate ids, so bad packs fail loudly instead of
// silently producing unanswerable questions.
class QuestionBank {
public:
    static constexpr int kMinDifficulty = 1;
    static constexpr int kMaxDifficulty = 10;

    void add(Question q);

    // Accepts either {"questions": [...]} or a bare array of questions.
    void loadFromString(const std::string& json);
    void loadFromFile(const std::filesystem::path& file);
    // Loads every *.json file in the directory (sorted by name).
    void loadFromDirectory(const std::filesystem::path& dir);

    const std::vector<Question>& all() const { return questions_; }
    const Question* find(const std::string& id) const;
    std::size_t size() const { return questions_.size(); }
    bool empty() const { return questions_.empty(); }
    int maxDifficulty() const;

private:
    std::vector<Question> questions_;
    std::unordered_map<std::string, std::size_t> index_;
};

}  // namespace cq
