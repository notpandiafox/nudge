#pragma once

#include <algorithm>
#include <initializer_list>
#include <string_view>
#include <vector>

#include "nudge/word.hpp"

namespace nudge {

// A word the test knows is valid.
inline Word w(std::string_view text) { return *Word::parse(text); }

// Sorted, like every candidate list the solver passes around (pickWinner asserts it).
inline std::vector<Word> words(std::initializer_list<std::string_view> texts) {
    std::vector<Word> result;
    for (const std::string_view text : texts) {
        result.push_back(w(text));
    }
    std::sort(result.begin(), result.end());
    return result;
}

}  // namespace nudge
