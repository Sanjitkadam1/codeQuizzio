#pragma once

#include <chrono>
#include <cstdint>
#include <functional>

namespace cq {

// Seconds since the unix epoch. Injectable so tests control "now".
using Clock = std::function<std::int64_t()>;

inline std::int64_t systemSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

}  // namespace cq
