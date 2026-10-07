#include "SelectorConfig.h"

#include <cmath>
#include <fstream>
#include <sstream>
#include <variant>

#include <nlohmann/json.hpp>

namespace cq {

namespace {

using nlohmann::json;

// One row per setting: its name, which member it is, and the allowed range.
// This table drives parsing, validation, and serialization, so adding a knob
// is: add the member in the header, add a row here.
struct Field {
    const char* name;
    std::variant<int SelectorConfig::*, double SelectorConfig::*, bool SelectorConfig::*> member;
    double min;
    double max;
};

const std::vector<Field>& fields() {
    static const std::vector<Field> table = {
        {"recentWindow", &SelectorConfig::recentWindow, 0, 100},
        {"requeueDelay", &SelectorConfig::requeueDelay, 1, 100},
        {"sigma", &SelectorConfig::sigma, 0.1, 20},
        {"sessionMissWeight", &SelectorConfig::sessionMissWeight, 0, 10},
        {"unseenBoost", &SelectorConfig::unseenBoost, 0, 20},
        {"fastStep", &SelectorConfig::fastStep, 0, 5},
        {"slowStep", &SelectorConfig::slowStep, 0, 5},
        {"wrongStep", &SelectorConfig::wrongStep, -5, 0},
        {"skipStep", &SelectorConfig::skipStep, -5, 0},

        {"useHistory", &SelectorConfig::useHistory, 0, 1},
        {"masteryPrior", &SelectorConfig::masteryPrior, 0, 1},
        {"confidenceAttempts", &SelectorConfig::confidenceAttempts, 0, 100},
        {"speedWeight", &SelectorConfig::speedWeight, 0, 1},
        {"recencyDecay", &SelectorConfig::recencyDecay, 0.05, 1},
        {"skipPenalty", &SelectorConfig::skipPenalty, 0, 1},
        {"forgetHalfLifeDays", &SelectorConfig::forgetHalfLifeDays, 0, 3650},
        {"needFloor", &SelectorConfig::needFloor, 0, 1},
        {"tagBoost", &SelectorConfig::tagBoost, 0, 5},
        {"startMasteryThreshold", &SelectorConfig::startMasteryThreshold, 0, 1},
        {"startMinAttempts", &SelectorConfig::startMinAttempts, 1, 100},
        {"startOffset", &SelectorConfig::startOffset, 0, 5},
    };
    return table;
}

std::string rangeText(const Field& f) {
    std::ostringstream o;
    o << f.min << " to " << f.max;
    return o.str();
}

void apply(SelectorConfig& cfg, const Field& f, const json& value) {
    const std::string name = f.name;
    std::visit(
        [&](auto member) {
            using T = std::decay_t<decltype(cfg.*member)>;
            if constexpr (std::is_same_v<T, bool>) {
                if (!value.is_boolean()) throw ConfigError("'" + name + "' must be true or false");
                cfg.*member = value.get<bool>();
            } else if constexpr (std::is_same_v<T, int>) {
                if (!value.is_number_integer()) throw ConfigError("'" + name + "' must be a whole number");
                const long long v = value.get<long long>();
                if (v < f.min || v > f.max) {
                    throw ConfigError("'" + name + "' must be between " + rangeText(f));
                }
                cfg.*member = static_cast<int>(v);
            } else {
                if (!value.is_number()) throw ConfigError("'" + name + "' must be a number");
                const double v = value.get<double>();
                if (!std::isfinite(v) || v < f.min || v > f.max) {
                    throw ConfigError("'" + name + "' must be between " + rangeText(f));
                }
                cfg.*member = v;
            }
        },
        f.member);
}

}  // namespace

SelectorConfig selectorConfigFromJson(const std::string& text) {
    json doc;
    try {
        doc = json::parse(text);
    } catch (const json::parse_error& e) {
        throw ConfigError(std::string("invalid JSON: ") + e.what());
    }
    if (!doc.is_object()) throw ConfigError("config must be a JSON object");

    SelectorConfig cfg;
    for (const auto& [key, value] : doc.items()) {
        const Field* match = nullptr;
        for (const auto& f : fields()) {
            if (key == f.name) match = &f;
        }
        if (match == nullptr) throw ConfigError("unknown setting '" + key + "'");
        apply(cfg, *match, value);
    }
    return cfg;
}

std::string toJson(const SelectorConfig& cfg) {
    // nlohmann sorts keys; the docs and shipped file follow the table order, so
    // the object is written out by hand.
    std::ostringstream out;
    out << "{\n";
    bool first = true;
    for (const auto& f : fields()) {
        json value;
        std::visit([&](auto member) { value = cfg.*member; }, f.member);
        if (!first) out << ",\n";
        first = false;
        out << "  " << json(std::string(f.name)).dump() << ": " << value.dump();
    }
    out << "\n}\n";
    return out.str();
}

std::vector<std::string> selectorConfigKeys() {
    std::vector<std::string> keys;
    for (const auto& f : fields()) keys.push_back(f.name);
    return keys;
}

LoadedSelectorConfig loadSelectorConfig(const std::filesystem::path& file) {
    LoadedSelectorConfig result;
    std::error_code ec;
    if (!std::filesystem::exists(file, ec)) return result;

    std::ifstream in(file, std::ios::binary);
    if (!in) {
        result.warning = "cannot read " + file.string() + "; using default selector settings";
        return result;
    }
    std::ostringstream buf;
    buf << in.rdbuf();
    try {
        result.config = selectorConfigFromJson(buf.str());
    } catch (const ConfigError& e) {
        result.config = SelectorConfig{};
        result.warning = file.filename().string() + ": " + e.what() + "; using default selector settings";
    }
    return result;
}

}  // namespace cq
