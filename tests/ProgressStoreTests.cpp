#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "ProgressStore.h"

using namespace cq;
namespace fs = std::filesystem;

namespace {

void writeText(const fs::path& p, const std::string& text) {
    fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out << text;
}

std::string readText(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

fs::path withSuffix(fs::path p, const char* s) {
    p += s;
    return p;
}

Progress progressWithScore(int score) {
    Progress p;
    p.recordAnswer("q", Outcome::Correct, 2.0, 10);
    SessionRecord r;
    r.score = score;
    r.correct = 1;
    p.recordSession(r);
    return p;
}

class ProgressStoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        dir = fs::temp_directory_path() /
              ("cq_store_test_" + std::to_string(stamp) + "_" +
               ::testing::UnitTest::GetInstance()->current_test_info()->name());
        file = dir / "progress.json";
        fs::create_directories(dir);
    }
    void TearDown() override {
        std::error_code ec;
        fs::remove_all(dir, ec);
    }
    fs::path dir, file;
};

}  // namespace

TEST_F(ProgressStoreTest, MissingFileMeansFirstRun) {
    LoadResult r = loadProgress(file);
    EXPECT_EQ(r.source, LoadResult::Source::Fresh);
    EXPECT_TRUE(r.warning.empty());
    EXPECT_EQ(r.progress.totalSessions(), 0);
}

TEST_F(ProgressStoreTest, SaveThenLoadRoundTrips) {
    saveProgress(file, progressWithScore(120));
    LoadResult r = loadProgress(file);
    EXPECT_EQ(r.source, LoadResult::Source::Primary);
    EXPECT_TRUE(r.warning.empty());
    EXPECT_EQ(r.progress.highScore(), 120);
    ASSERT_NE(r.progress.stats("q"), nullptr);
}

TEST_F(ProgressStoreTest, SaveCreatesMissingDirectories) {
    const fs::path nested = dir / "a" / "b" / "progress.json";
    saveProgress(nested, progressWithScore(1));
    EXPECT_TRUE(fs::exists(nested));
}

TEST_F(ProgressStoreTest, LeavesNoTempFileBehind) {
    saveProgress(file, progressWithScore(1));
    saveProgress(file, progressWithScore(2));
    EXPECT_FALSE(fs::exists(withSuffix(file, ".tmp")));
}

TEST_F(ProgressStoreTest, FirstSaveHasNoBackupSecondSaveKeepsPreviousVersion) {
    saveProgress(file, progressWithScore(10));
    EXPECT_FALSE(fs::exists(withSuffix(file, ".bak")));

    saveProgress(file, progressWithScore(20));
    ASSERT_TRUE(fs::exists(withSuffix(file, ".bak")));
    EXPECT_EQ(Progress::fromJson(readText(withSuffix(file, ".bak"))).highScore(), 10);
    EXPECT_EQ(Progress::fromJson(readText(file)).highScore(), 20);
}

TEST_F(ProgressStoreTest, CorruptPrimaryRecoversFromBackup) {
    saveProgress(file, progressWithScore(10));
    saveProgress(file, progressWithScore(20));
    writeText(file, "{ this is not json");  // simulate a damaged save

    LoadResult r = loadProgress(file);
    EXPECT_EQ(r.source, LoadResult::Source::Backup);
    EXPECT_FALSE(r.warning.empty());
    EXPECT_EQ(r.progress.highScore(), 10);
    EXPECT_TRUE(fs::exists(withSuffix(file, ".corrupt")));  // kept for inspection
    EXPECT_EQ(readText(withSuffix(file, ".corrupt")), "{ this is not json");
}

TEST_F(ProgressStoreTest, TruncatedPrimaryRecoversFromBackup) {
    saveProgress(file, progressWithScore(10));
    saveProgress(file, progressWithScore(20));
    const std::string full = readText(file);
    writeText(file, full.substr(0, full.size() / 2));  // e.g. disk filled up mid-write

    EXPECT_EQ(loadProgress(file).source, LoadResult::Source::Backup);
}

TEST_F(ProgressStoreTest, MissingPrimaryWithBackupRecovers) {
    saveProgress(file, progressWithScore(10));
    saveProgress(file, progressWithScore(20));
    fs::remove(file);

    LoadResult r = loadProgress(file);
    EXPECT_EQ(r.source, LoadResult::Source::Backup);
    EXPECT_EQ(r.progress.highScore(), 10);
}

TEST_F(ProgressStoreTest, BothCorruptStartsFreshWithWarning) {
    writeText(file, "garbage");
    writeText(withSuffix(file, ".bak"), "also garbage");

    LoadResult r = loadProgress(file);
    EXPECT_EQ(r.source, LoadResult::Source::Fresh);
    EXPECT_FALSE(r.warning.empty());
    EXPECT_EQ(r.progress.totalSessions(), 0);
}

TEST_F(ProgressStoreTest, CorruptPrimaryNoBackupStartsFresh) {
    writeText(file, "garbage");
    LoadResult r = loadProgress(file);
    EXPECT_EQ(r.source, LoadResult::Source::Fresh);
    EXPECT_FALSE(r.warning.empty());
}

TEST_F(ProgressStoreTest, NewerVersionFileIsNotOverwrittenSilently) {
    writeText(file, R"({"version":999})");
    LoadResult r = loadProgress(file);
    EXPECT_EQ(r.source, LoadResult::Source::Fresh);
    EXPECT_NE(r.warning.find("newer"), std::string::npos);
    // The unreadable file is preserved, not deleted.
    EXPECT_EQ(readText(withSuffix(file, ".corrupt")), R"({"version":999})");
}

TEST_F(ProgressStoreTest, SavingAfterRecoveryWritesAGoodPrimary) {
    saveProgress(file, progressWithScore(10));
    saveProgress(file, progressWithScore(20));
    writeText(file, "damaged");

    LoadResult r = loadProgress(file);
    saveProgress(file, r.progress);  // what the app does on the next answer
    EXPECT_EQ(loadProgress(file).source, LoadResult::Source::Primary);
}

TEST_F(ProgressStoreTest, StaleTempFileFromACrashIsIgnoredAndReplaced) {
    saveProgress(file, progressWithScore(10));
    writeText(withSuffix(file, ".tmp"), "half-written junk from a crash");

    EXPECT_EQ(loadProgress(file).progress.highScore(), 10);  // temp never read
    saveProgress(file, progressWithScore(30));
    EXPECT_EQ(loadProgress(file).progress.highScore(), 30);
    EXPECT_FALSE(fs::exists(withSuffix(file, ".tmp")));
}

TEST_F(ProgressStoreTest, SaveFailureLeavesExistingSaveIntact) {
    saveProgress(file, progressWithScore(10));
    // A directory squatting on the temp path makes the write fail.
    fs::create_directory(withSuffix(file, ".tmp"));

    EXPECT_THROW(saveProgress(file, progressWithScore(99)), std::runtime_error);
    EXPECT_EQ(loadProgress(file).progress.highScore(), 10);
}
