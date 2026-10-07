#include <gtest/gtest.h>

#include <limits>

#include "Progress.h"

using namespace cq;

TEST(Progress, RecordsCorrectAnswersWithTimes) {
    Progress p;
    p.recordAnswer("q", Outcome::Correct, 4.0, 100);
    p.recordAnswer("q", Outcome::Correct, 2.0, 200);
    const QuestionStats* s = p.stats("q");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->attempts, 2);
    EXPECT_EQ(s->correct, 2);
    EXPECT_DOUBLE_EQ(s->bestTime, 2.0);
    EXPECT_DOUBLE_EQ(s->averageCorrectTime(), 3.0);
    EXPECT_EQ(s->lastSeen, 200);
}

TEST(Progress, MissesAndSkipsDoNotAffectTimes) {
    Progress p;
    p.recordAnswer("q", Outcome::Wrong, 9.0, 1);
    p.recordAnswer("q", Outcome::Skipped, 0.0, 2);
    const QuestionStats* s = p.stats("q");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->attempts, 2);
    EXPECT_EQ(s->misses, 1);
    EXPECT_EQ(s->skips, 1);
    EXPECT_EQ(s->correct, 0);
    EXPECT_FALSE(s->hasBestTime());
    EXPECT_DOUBLE_EQ(s->averageCorrectTime(), 0.0);
}

TEST(Progress, UnknownQuestionHasNoStats) {
    Progress p;
    EXPECT_EQ(p.stats("nope"), nullptr);
}

TEST(Progress, InstantAnswerStillCountsAsHavingATime) {
    Progress p;
    p.recordAnswer("q", Outcome::Correct, 0.0, 1);
    EXPECT_TRUE(p.stats("q")->hasBestTime());
}

TEST(Progress, SessionRecordsTrackHighScoreAndStreak) {
    Progress p;
    SessionRecord a;
    a.score = 100;
    a.bestStreak = 3;
    a.correct = 4;
    auto r1 = p.recordSession(a);
    EXPECT_TRUE(r1.newHighScore);
    EXPECT_TRUE(r1.newBestStreak);

    SessionRecord b;
    b.score = 50;
    b.bestStreak = 5;
    b.correct = 1;
    auto r2 = p.recordSession(b);
    EXPECT_FALSE(r2.newHighScore);
    EXPECT_TRUE(r2.newBestStreak);

    EXPECT_EQ(p.highScore(), 100);
    EXPECT_EQ(p.bestStreak(), 5);
    EXPECT_EQ(p.totalSessions(), 2);
    EXPECT_EQ(p.history().size(), 2u);
}

TEST(Progress, TieIsNotARecord) {
    Progress p;
    SessionRecord a;
    a.score = 80;
    a.bestStreak = 2;
    p.recordSession(a);
    auto r = p.recordSession(a);
    EXPECT_FALSE(r.newHighScore);
    EXPECT_FALSE(r.newBestStreak);
}

TEST(Progress, HistoryIsCappedKeepingTheNewest) {
    Progress p;
    for (int i = 0; i < static_cast<int>(Progress::kMaxHistory) + 25; ++i) {
        SessionRecord r;
        r.score = i;
        r.endedAt = i;
        p.recordSession(r);
    }
    ASSERT_EQ(p.history().size(), Progress::kMaxHistory);
    EXPECT_EQ(p.history().front().score, 25);
    EXPECT_EQ(p.history().back().score, static_cast<int>(Progress::kMaxHistory) + 24);
    EXPECT_EQ(p.totalSessions(), static_cast<int>(Progress::kMaxHistory) + 25);
}

TEST(Progress, AccuracyIgnoresSkips) {
    SessionRecord r;
    r.correct = 3;
    r.wrong = 1;
    r.skipped = 10;
    EXPECT_DOUBLE_EQ(r.accuracy(), 0.75);
    EXPECT_EQ(r.answered(), 14);
    EXPECT_DOUBLE_EQ(SessionRecord{}.accuracy(), 0.0);
}

TEST(Progress, JsonRoundTrip) {
    Progress p;
    p.recordAnswer("a", Outcome::Correct, 3.25, 1000);
    p.recordAnswer("a", Outcome::Wrong, 1.0, 1001);
    p.recordAnswer("b", Outcome::Skipped, 0.0, 1002);
    SessionRecord r;
    r.endedAt = 1003;
    r.score = 77;
    r.correct = 1;
    r.wrong = 1;
    r.skipped = 1;
    r.bestStreak = 1;
    p.recordSession(r);

    const Progress back = Progress::fromJson(p.toJson());
    EXPECT_EQ(back.highScore(), 77);
    EXPECT_EQ(back.bestStreak(), 1);
    EXPECT_EQ(back.totalSessions(), 1);
    ASSERT_EQ(back.history().size(), 1u);
    EXPECT_EQ(back.history()[0].endedAt, 1003);
    const QuestionStats* a = back.stats("a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->attempts, 2);
    EXPECT_EQ(a->correct, 1);
    EXPECT_EQ(a->misses, 1);
    EXPECT_DOUBLE_EQ(a->bestTime, 3.25);
    EXPECT_EQ(a->lastSeen, 1001);
    EXPECT_EQ(back.stats("b")->skips, 1);
    EXPECT_EQ(back.toJson(), p.toJson());  // stable, deterministic output
}

TEST(Progress, EmptyRoundTrip) {
    const Progress back = Progress::fromJson(Progress{}.toJson());
    EXPECT_EQ(back.totalSessions(), 0);
    EXPECT_TRUE(back.allStats().empty());
}

TEST(Progress, RejectsGarbageAndTruncatedJson) {
    EXPECT_THROW(Progress::fromJson(""), ProgressError);
    EXPECT_THROW(Progress::fromJson("not json"), ProgressError);
    const std::string good = Progress{}.toJson();
    EXPECT_THROW(Progress::fromJson(good.substr(0, good.size() / 2)), ProgressError);
    EXPECT_THROW(Progress::fromJson("[1,2,3]"), ProgressError);
}

TEST(Progress, RejectsMissingOrBadVersion) {
    EXPECT_THROW(Progress::fromJson("{}"), ProgressError);
    EXPECT_THROW(Progress::fromJson(R"({"version":0})"), ProgressError);
    EXPECT_THROW(Progress::fromJson(R"({"version":"one"})"), ProgressError);
}

TEST(Progress, RejectsNewerVersionInsteadOfMisreadingIt) {
    try {
        Progress::fromJson(R"({"version":999})");
        FAIL() << "expected ProgressError";
    } catch (const ProgressError& e) {
        EXPECT_NE(std::string(e.what()).find("newer"), std::string::npos);
    }
}

TEST(Progress, RejectsWrongTypesAndNegativeValues) {
    EXPECT_THROW(Progress::fromJson(R"({"version":1,"highScore":"lots"})"), ProgressError);
    EXPECT_THROW(Progress::fromJson(R"({"version":1,"highScore":-5})"), ProgressError);
    EXPECT_THROW(Progress::fromJson(R"({"version":1,"questions":[]})"), ProgressError);
    EXPECT_THROW(Progress::fromJson(R"({"version":1,"questions":{"q":5}})"), ProgressError);
    EXPECT_THROW(Progress::fromJson(R"({"version":1,"questions":{"q":{"attempts":-1}}})"),
                 ProgressError);
    EXPECT_THROW(Progress::fromJson(R"({"version":1,"questions":{"q":{"bestTime":-2.5}}})"),
                 ProgressError);
    EXPECT_THROW(Progress::fromJson(R"({"version":1,"history":{}})"), ProgressError);
    EXPECT_THROW(Progress::fromJson(R"({"version":1,"history":[3]})"), ProgressError);
}

TEST(Progress, MissingOptionalSectionsDefaultToEmpty) {
    const Progress p = Progress::fromJson(R"({"version":1})");
    EXPECT_EQ(p.highScore(), 0);
    EXPECT_TRUE(p.allStats().empty());
    EXPECT_TRUE(p.history().empty());
}

TEST(Progress, OversizedHistoryIsTrimmedOnLoad) {
    std::string json = R"({"version":1,"history":[)";
    const int n = static_cast<int>(Progress::kMaxHistory) + 10;
    for (int i = 0; i < n; ++i) {
        if (i) json += ",";
        json += R"({"score":)" + std::to_string(i) + "}";
    }
    json += "]}";
    const Progress p = Progress::fromJson(json);
    ASSERT_EQ(p.history().size(), Progress::kMaxHistory);
    EXPECT_EQ(p.history().back().score, n - 1);
}

TEST(Progress, UnicodeAndOddQuestionIdsSurvive) {
    Progress p;
    p.recordAnswer("id with spaces & \"quotes\" / ünïcode", Outcome::Correct, 1.0, 1);
    const Progress back = Progress::fromJson(p.toJson());
    EXPECT_NE(back.stats("id with spaces & \"quotes\" / ünïcode"), nullptr);
}
