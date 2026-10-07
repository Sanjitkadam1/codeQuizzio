#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace cq {

class ConfigError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Every knob of the adaptive selector in one place. The defaults here are the
// defaults of the game; override any subset from a JSON file (see
// config/selector.json and docs/TUNING.md) without recompiling.
struct SelectorConfig {
    // ---- Within a session ----
    int recentWindow = 5;               // no question repeats within this many questions
    int requeueDelay = 3;               // a missed question returns after this many questions
    double sigma = 1.5;                 // how tightly questions cluster around the target difficulty
    double sessionMissWeight = 0.5;     // extra weight per miss in the current session
    double unseenBoost = 1.5;           // weight multiplier for never-seen questions
    double fastStep = 0.3;              // target moves up by this after a fast correct answer
    double slowStep = 0.1;              // ...or this after a slow correct answer
    double wrongStep = -0.5;            // target change after a wrong answer
    double skipStep = -0.3;             // target change after a skip

    // ---- Using saved history (needs a Progress; ignored without one) ----
    bool useHistory = true;             // master switch for everything below
    double masteryPrior = 0.5;          // assumed mastery with no evidence
    double confidenceAttempts = 3.0;    // attempts needed before history outweighs the prior
    double speedWeight = 0.3;           // how much answer speed (vs par) counts toward mastery
    double recencyDecay = 0.8;          // each older outcome counts this much less (1 = equal)
    double skipPenalty = 1.0;           // a skip counts as this fraction of a miss
    double forgetHalfLifeDays = 14.0;   // mastery halves after this many days unseen (0 = never)
    double needFloor = 0.15;            // minimum weight of a fully mastered question (0 = never shown)
    double tagBoost = 0.5;              // extra weight for questions in your weakest topics
    double startMasteryThreshold = 0.7; // mastery needed to count a difficulty level as comfortable
    int startMinAttempts = 2;           // attempted questions needed at a level to judge it
    double startOffset = 0.5;           // start this far above your comfortable level
};

// Parses a (possibly partial) JSON object over the defaults. Unknown keys,
// wrong types, and out-of-range values throw ConfigError naming the problem,
// so a typo never silently does nothing.
SelectorConfig selectorConfigFromJson(const std::string& text);

// Every setting, with its current value, as pretty JSON.
std::string toJson(const SelectorConfig& config);

// Names of all settings (used to keep the docs and the shipped file in sync).
std::vector<std::string> selectorConfigKeys();

struct LoadedSelectorConfig {
    SelectorConfig config;
    std::string warning;  // empty when fine; set when a file existed but was unusable
};

// Missing file -> defaults, no warning. Unusable file -> defaults plus a warning.
LoadedSelectorConfig loadSelectorConfig(const std::filesystem::path& file);

}  // namespace cq
