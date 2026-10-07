#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

#include "SelectorConfig.h"

using namespace cq;
namespace fs = std::filesystem;

namespace {

std::string readText(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

void expectSame(const SelectorConfig& a, const SelectorConfig& b) {
    // toJson lists every setting, so comparing it compares every setting.
    EXPECT_EQ(toJson(a), toJson(b));
}

}  // namespace

TEST(SelectorConfig, EmptyObjectGivesDefaults) {
    expectSame(selectorConfigFromJson("{}"), SelectorConfig{});
}

TEST(SelectorConfig, PartialOverrideKeepsOtherDefaults) {
    const SelectorConfig c = selectorConfigFromJson(R"({"sigma": 2.5, "recentWindow": 9, "useHistory": false})");
    EXPECT_DOUBLE_EQ(c.sigma, 2.5);
    EXPECT_EQ(c.recentWindow, 9);
    EXPECT_FALSE(c.useHistory);
    EXPECT_DOUBLE_EQ(c.needFloor, SelectorConfig{}.needFloor);
}

TEST(SelectorConfig, IntegerWrittenForADoubleIsFine) {
    EXPECT_DOUBLE_EQ(selectorConfigFromJson(R"({"sigma": 3})").sigma, 3.0);
}

TEST(SelectorConfig, JsonRoundTrip) {
    SelectorConfig c;
    c.sigma = 2.25;
    c.recentWindow = 7;
    c.useHistory = false;
    c.forgetHalfLifeDays = 0.0;
    expectSame(selectorConfigFromJson(toJson(c)), c);
}

TEST(SelectorConfig, UnknownKeyIsRejectedByName) {
    try {
        selectorConfigFromJson(R"({"sigmaa": 2})");
        FAIL() << "expected ConfigError";
    } catch (const ConfigError& e) {
        EXPECT_NE(std::string(e.what()).find("sigmaa"), std::string::npos);
    }
}

TEST(SelectorConfig, RejectsWrongTypes) {
    EXPECT_THROW(selectorConfigFromJson(R"({"sigma": "wide"})"), ConfigError);
    EXPECT_THROW(selectorConfigFromJson(R"({"recentWindow": 2.5})"), ConfigError);
    EXPECT_THROW(selectorConfigFromJson(R"({"useHistory": 1})"), ConfigError);
    EXPECT_THROW(selectorConfigFromJson(R"({"sigma": null})"), ConfigError);
}

TEST(SelectorConfig, RejectsOutOfRangeValues) {
    EXPECT_THROW(selectorConfigFromJson(R"({"sigma": 0})"), ConfigError);        // would divide by zero
    EXPECT_THROW(selectorConfigFromJson(R"({"sigma": -1})"), ConfigError);
    EXPECT_THROW(selectorConfigFromJson(R"({"needFloor": 1.5})"), ConfigError);
    EXPECT_THROW(selectorConfigFromJson(R"({"recencyDecay": 0})"), ConfigError);
    EXPECT_THROW(selectorConfigFromJson(R"({"wrongStep": 0.5})"), ConfigError);  // must be <= 0
    EXPECT_THROW(selectorConfigFromJson(R"({"requeueDelay": 0})"), ConfigError);
    EXPECT_THROW(selectorConfigFromJson(R"({"startMinAttempts": 0})"), ConfigError);
}

TEST(SelectorConfig, BoundariesAreAllowed) {
    EXPECT_NO_THROW(selectorConfigFromJson(R"({"needFloor": 0, "recencyDecay": 1, "forgetHalfLifeDays": 0})"));
    EXPECT_NO_THROW(selectorConfigFromJson(R"({"needFloor": 1, "wrongStep": 0})"));
}

TEST(SelectorConfig, RejectsMalformedJson) {
    EXPECT_THROW(selectorConfigFromJson(""), ConfigError);
    EXPECT_THROW(selectorConfigFromJson("{sigma: 2}"), ConfigError);
    EXPECT_THROW(selectorConfigFromJson("[1,2]"), ConfigError);
}

TEST(SelectorConfig, ShippedFileMatchesTheDefaults) {
    // config/selector.json is what users edit; it must not drift from the code.
    const auto loaded = loadSelectorConfig(fs::path(CQ_CONFIG_DIR) / "selector.json");
    EXPECT_TRUE(loaded.warning.empty()) << loaded.warning;
    expectSame(loaded.config, SelectorConfig{});
}

TEST(SelectorConfig, ShippedFileListsEverySetting) {
    const std::string text = readText(fs::path(CQ_CONFIG_DIR) / "selector.json");
    for (const auto& key : selectorConfigKeys()) {
        EXPECT_NE(text.find("\"" + key + "\""), std::string::npos) << key << " missing from config/selector.json";
    }
}

TEST(SelectorConfig, TuningDocExplainsEverySetting) {
    const std::string text = readText(fs::path(CQ_DOCS_DIR) / "TUNING.md");
    ASSERT_FALSE(text.empty());
    for (const auto& key : selectorConfigKeys()) {
        EXPECT_NE(text.find("`" + key + "`"), std::string::npos) << key << " is not documented in docs/TUNING.md";
    }
}

class SelectorConfigFileTest : public ::testing::Test {
protected:
    void SetUp() override {
        dir = fs::temp_directory_path() / "cq_selector_cfg_test";
        fs::remove_all(dir);
        fs::create_directories(dir);
    }
    void TearDown() override { fs::remove_all(dir); }
    fs::path dir;
};

TEST_F(SelectorConfigFileTest, MissingFileMeansDefaultsWithoutWarning) {
    const auto r = loadSelectorConfig(dir / "nope.json");
    EXPECT_TRUE(r.warning.empty());
    expectSame(r.config, SelectorConfig{});
}

TEST_F(SelectorConfigFileTest, GoodFileIsApplied) {
    std::ofstream(dir / "c.json") << R"({"sigma": 4})";
    const auto r = loadSelectorConfig(dir / "c.json");
    EXPECT_TRUE(r.warning.empty());
    EXPECT_DOUBLE_EQ(r.config.sigma, 4.0);
}

TEST_F(SelectorConfigFileTest, BadFileFallsBackToDefaultsWithAWarning) {
    std::ofstream(dir / "c.json") << R"({"sigma": 4, "typo": 1})";
    const auto r = loadSelectorConfig(dir / "c.json");
    EXPECT_FALSE(r.warning.empty());
    EXPECT_NE(r.warning.find("typo"), std::string::npos);
    expectSame(r.config, SelectorConfig{});  // all-or-nothing, never half-applied
}
