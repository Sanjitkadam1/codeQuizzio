#pragma once

#include <string>
#include <vector>

namespace cq {

struct Question {
    std::string id;
    std::string prompt;
    std::vector<std::string> answers;  // every accepted spelling
    int difficulty = 1;                // 1 (trivial) .. 10 (hard)
    std::vector<std::string> tags;
};

}  // namespace cq
