#include "QuestionBank.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace cq {

namespace {

using nlohmann::json;

Question parseQuestion(const json& j) {
    if (!j.is_object()) {
        throw std::runtime_error("question entry must be a JSON object");
    }
    Question q;
    q.id = j.value("id", std::string{});
    q.prompt = j.value("prompt", std::string{});
    q.difficulty = j.value("difficulty", 0);
    if (j.contains("answers")) {
        if (!j["answers"].is_array()) {
            throw std::runtime_error("'answers' must be an array (question '" + q.id + "')");
        }
        for (const auto& a : j["answers"]) {
            if (!a.is_string()) {
                throw std::runtime_error("'answers' entries must be strings (question '" + q.id + "')");
            }
            q.answers.push_back(a.get<std::string>());
        }
    }
    if (j.contains("tags")) {
        if (!j["tags"].is_array()) {
            throw std::runtime_error("'tags' must be an array (question '" + q.id + "')");
        }
        for (const auto& t : j["tags"]) {
            if (!t.is_string()) {
                throw std::runtime_error("'tags' entries must be strings (question '" + q.id + "')");
            }
            q.tags.push_back(t.get<std::string>());
        }
    }
    return q;
}

}  // namespace

void QuestionBank::add(Question q) {
    if (q.id.empty()) throw std::runtime_error("question is missing an 'id'");
    if (q.prompt.empty()) throw std::runtime_error("question '" + q.id + "' is missing a 'prompt'");
    if (q.answers.empty()) throw std::runtime_error("question '" + q.id + "' has no 'answers'");
    for (const auto& a : q.answers) {
        if (a.empty()) throw std::runtime_error("question '" + q.id + "' has an empty answer");
    }
    if (q.difficulty < kMinDifficulty || q.difficulty > kMaxDifficulty) {
        throw std::runtime_error("question '" + q.id + "' has difficulty outside 1-10");
    }
    if (index_.count(q.id)) throw std::runtime_error("duplicate question id '" + q.id + "'");

    index_[q.id] = questions_.size();
    questions_.push_back(std::move(q));
}

void QuestionBank::loadFromString(const std::string& text) {
    json doc;
    try {
        doc = json::parse(text);
    } catch (const json::parse_error& e) {
        throw std::runtime_error(std::string("invalid JSON: ") + e.what());
    }

    const json* list = &doc;
    if (doc.is_object()) {
        if (!doc.contains("questions")) throw std::runtime_error("JSON object has no 'questions' array");
        list = &doc["questions"];
    }
    if (!list->is_array()) throw std::runtime_error("expected an array of questions");

    for (const auto& entry : *list) add(parseQuestion(entry));
}

void QuestionBank::loadFromFile(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + file.string());
    std::ostringstream buf;
    buf << in.rdbuf();
    try {
        loadFromString(buf.str());
    } catch (const std::runtime_error& e) {
        throw std::runtime_error(file.filename().string() + ": " + e.what());
    }
}

void QuestionBank::loadFromDirectory(const std::filesystem::path& dir) {
    if (!std::filesystem::is_directory(dir)) {
        throw std::runtime_error("not a directory: " + dir.string());
    }
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    for (const auto& f : files) loadFromFile(f);
}

const Question* QuestionBank::find(const std::string& id) const {
    auto it = index_.find(id);
    return it == index_.end() ? nullptr : &questions_[it->second];
}

int QuestionBank::maxDifficulty() const {
    int m = kMinDifficulty;
    for (const auto& q : questions_) m = std::max(m, q.difficulty);
    return m;
}

}  // namespace cq
