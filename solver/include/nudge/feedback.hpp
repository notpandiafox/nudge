#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace nudge {

enum class Tile : std::uint8_t { Gray, Yellow, Green };
using FeedbackPattern = std::array<Tile, 5>;

// The pattern the real game shows for this guess against this answer. Both
// words may be in any case. Passing anything that isn't a 5-letter word is a
// caller bug: it throws std::invalid_argument (aborts in WebAssembly).
FeedbackPattern computeFeedback(const std::string& guess, const std::string& answer);

}  // namespace nudge
