#pragma once

#include <string>

namespace nudge::reference {

// The feedback rule written the slow, obvious way, on purpose differently from
// the solver's letter-counting version: matched answer letters are crossed out
// as they are used. Returns pattern text, e.g. "GYXXG". Test-only (plan §8.1):
// never optimize this or share it with production code.
inline std::string simpleFeedback(const std::string& guess, std::string answer) {
    std::string result = "XXXXX";
    for (std::size_t i = 0; i < 5; ++i) {
        if (guess[i] == answer[i]) {
            result[i] = 'G';
            answer[i] = '#';
        }
    }
    for (std::size_t i = 0; i < 5; ++i) {
        if (result[i] == 'G') {
            continue;
        }
        const std::size_t found = answer.find(guess[i]);
        if (found != std::string::npos) {
            result[i] = 'Y';
            answer[found] = '#';
        }
    }
    return result;
}

}  // namespace nudge::reference
