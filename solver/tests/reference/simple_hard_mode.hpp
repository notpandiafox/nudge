#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace nudge::reference {

// Hard mode's rule B written the slow, obvious way from the spec's wording, on
// purpose differently from the solver's count table: for each earlier guess,
// every Green letter must be in place, and every lit (Green or Yellow) letter
// is crossed out of a copy of the new guess, so repeated letters need repeated
// copies. history holds (guess, pattern text) pairs. Test-only (plan §8.1).
inline bool simpleHardModeAllows(const std::vector<std::pair<std::string, std::string>>& history,
                                 const std::string& guess) {
    for (const auto& [earlier, pattern] : history) {
        std::string left = guess;
        for (std::size_t i = 0; i < 5; ++i) {
            if (pattern[i] == 'G' && guess[i] != earlier[i]) {
                return false;
            }
            if (pattern[i] == 'G' || pattern[i] == 'Y') {
                const std::size_t found = left.find(earlier[i]);
                if (found == std::string::npos) {
                    return false;
                }
                left[found] = '#';
            }
        }
    }
    return true;
}

}  // namespace nudge::reference
