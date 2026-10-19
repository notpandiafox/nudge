#include "hard_mode.hpp"

#include <algorithm>

namespace nudge {

HardModeConstraints hardModeConstraints(std::span<const VeteranWord> veterans) {
    HardModeConstraints constraints;
    for (const VeteranWord& veteran : veterans) {
        // Only addFeedbackPattern adds veteran words, after checking them, so parse() can't fail.
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        const Word guess = *Word::parse(veteran.word);
        std::array<std::uint8_t, 26> lit{};
        for (std::size_t i = 0; i < 5; ++i) {
            const std::uint8_t letter = guess.letters()[i];
            if (veteran.pattern[i] == Tile::Green) {
                constraints.greens[i] = letter;
            }
            if (veteran.pattern[i] != Tile::Gray) {
                ++lit[letter];
            }
        }
        for (std::size_t letter = 0; letter < 26; ++letter) {
            constraints.minCounts[letter] = std::max(constraints.minCounts[letter], lit[letter]);
        }
    }
    return constraints;
}

bool isHardModeLegal(const Word& guess, const HardModeConstraints& constraints) {
    std::array<std::uint8_t, 26> copies{};
    for (std::size_t i = 0; i < 5; ++i) {
        const std::uint8_t letter = guess.letters()[i];
        // A Green fixed another letter here.
        if (constraints.greens[i].value_or(letter) != letter) {
            return false;
        }
        ++copies[letter];
    }
    for (std::size_t letter = 0; letter < 26; ++letter) {
        if (copies[letter] < constraints.minCounts[letter]) {
            return false;
        }
    }
    return true;
}

}  // namespace nudge
