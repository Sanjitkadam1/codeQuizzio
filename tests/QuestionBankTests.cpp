#include <gtest/gtest.h>

#include <stdexcept>

#include "AnswerChecker.h"
#include "QuestionBank.h"
#include "TestUtil.h"

using namespace cq;

TEST(QuestionBank, LoadsObjectForm) {
    QuestionBank b;
    b.loadFromString(R"({"questions":[{"id":"a","prompt":"p","answers":["x;"],"difficulty":3,"tags":["t"]}]})");
    ASSERT_EQ(b.size(), 1u);
    const Question* q = b.find("a");
    ASSERT_NE(q, nullptr);
    EXPECT_EQ(q->difficulty, 3);
    EXPECT_EQ(q->tags, std::vector<std::string>{"t"});
}

TEST(QuestionBank, LoadsBareArray) {
    QuestionBank b;
    b.loadFromString(R"([{"id":"a","prompt":"p","answers":["x"],"difficulty":1}])");
    EXPECT_EQ(b.size(), 1u);
}

TEST(QuestionBank, RejectsDuplicateIds) {
    QuestionBank b;
    b.add(makeQuestion("a"));
    EXPECT_THROW(b.add(makeQuestion("a")), std::runtime_error);
}

TEST(QuestionBank, RejectsInvalidQuestions) {
    QuestionBank b;
    EXPECT_THROW(b.add(makeQuestion("")), std::runtime_error);
    EXPECT_THROW(b.add(makeQuestion("a", 0)), std::runtime_error);
    EXPECT_THROW(b.add(makeQuestion("b", 11)), std::runtime_error);
    EXPECT_THROW(b.add(makeQuestion("c", 1, {})), std::runtime_error);
    EXPECT_THROW(b.add(makeQuestion("d", 1, {""})), std::runtime_error);
    auto noPrompt = makeQuestion("e");
    noPrompt.prompt.clear();
    EXPECT_THROW(b.add(noPrompt), std::runtime_error);
}

TEST(QuestionBank, RejectsMalformedJson) {
    QuestionBank b;
    EXPECT_THROW(b.loadFromString("{not json"), std::runtime_error);
    EXPECT_THROW(b.loadFromString(R"({"nope":[]})"), std::runtime_error);
    EXPECT_THROW(b.loadFromString(R"([{"id":"a","prompt":"p","answers":"x","difficulty":1}])"),
                 std::runtime_error);
}

TEST(QuestionBank, MaxDifficulty) {
    auto b = makeBank(4, 2);
    EXPECT_EQ(b.maxDifficulty(), 4);
}

// Guards the shipped question packs: valid JSON, unique ids, and every answer
// actually matches itself under the default checker.
TEST(QuestionBank, ShippedPacksAreValid) {
    QuestionBank b;
    ASSERT_NO_THROW(b.loadFromDirectory(CQ_QUESTIONS_DIR));
    EXPECT_GT(b.size(), 30u);
    for (const auto& q : b.all()) {
        for (const auto& a : q.answers) {
            EXPECT_TRUE(isCorrect(q, a)) << q.id << ": '" << a << "' rejects itself";
        }
    }
}
