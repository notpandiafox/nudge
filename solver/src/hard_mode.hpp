#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>

#include "nudge/word.hpp"
#include "nudge/wordle_solver.hpp"

namespace nudge {

// What hard mode's rule B requires of the next guess (plan §5.12).
struct HardModeConstraints {
    std::array<std::optional<std::uint8_t>, 5> greens;  // the letter a Green fixed at each position
    std::array<std::uint8_t, 26> minCounts{};           // copies of each letter the guess must have
};

// Every veteran word's Green letters stay in place, and each letter needs at
// least as many copies as one veteran word lit up (Green or Yellow): the
// highest count from any one word, never the sum.
HardModeConstraints hardModeConstraints(std::span<const VeteranWord> veterans);

bool isHardModeLegal(const Word& guess, const HardModeConstraints& constraints);

}  // namespace nudge
