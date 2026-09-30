#include <gtest/gtest.h>

#include "AnswerChecker.h"
#include "TestUtil.h"

using namespace cq;

TEST(AnswerChecker, ExactMatch) {
    auto q = makeQuestion("a", 1, {"#include <iostream>"});
    EXPECT_TRUE(isCorrect(q, "#include <iostream>"));
}

TEST(AnswerChecker, IgnoresSpacingAroundPunctuation) {
    auto q = makeQuestion("a", 1, {"int x = 5;"});
    EXPECT_TRUE(isCorrect(q, "int x=5;"));
    EXPECT_TRUE(isCorrect(q, "int   x  =  5 ;"));
    EXPECT_TRUE(isCorrect(q, "  int x = 5;\n"));
}

TEST(AnswerChecker, KeepsSpaceBetweenIdentifiers) {
    auto q = makeQuestion("a", 1, {"int x = 5;"});
    EXPECT_FALSE(isCorrect(q, "intx = 5;"));
    EXPECT_FALSE(isCorrect(q, "int x = 5"));  // missing semicolon
}

TEST(AnswerChecker, TrailingSpaceAroundIncludeAngleBrackets) {
    auto q = makeQuestion("a", 1, {"#include <vector>"});
    EXPECT_TRUE(isCorrect(q, "#include<vector>"));
    EXPECT_TRUE(isCorrect(q, "#include  < vector >"));
}

TEST(AnswerChecker, AcceptsAnyListedAnswer) {
    auto q = makeQuestion("a", 1, {"++i;", "i++;"});
    EXPECT_TRUE(isCorrect(q, "i++;"));
    EXPECT_TRUE(isCorrect(q, "++i;"));
    EXPECT_FALSE(isCorrect(q, "i--;"));
}

TEST(AnswerChecker, IsCaseSensitive) {
    auto q = makeQuestion("a", 1, {"std::string s;"});
    EXPECT_FALSE(isCorrect(q, "std::String s;"));
}

TEST(AnswerChecker, ExactStrictnessKeepsInnerSpacing) {
    auto q = makeQuestion("a", 1, {"int x = 5;"});
    EXPECT_TRUE(isCorrect(q, "int x = 5;\n", Strictness::Exact));
    EXPECT_FALSE(isCorrect(q, "int x=5;", Strictness::Exact));
}

TEST(AnswerChecker, EmptyInputIsWrong) {
    auto q = makeQuestion("a", 1, {"x;"});
    EXPECT_FALSE(isCorrect(q, ""));
    EXPECT_FALSE(isCorrect(q, "   "));
}
