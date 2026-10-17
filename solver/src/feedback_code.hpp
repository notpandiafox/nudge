#pragma once

#include <cstddef>
#include <cstdint>

#include "nudge/feedback.hpp"
#include "nudge/word.hpp"

namespace nudge {

// A feedback pattern packed into one number 0-242: Gray = 0, Yellow = 1,
// Green = 2, and position i is worth 3^i. Grouping candidates by pattern is
// then counting into an array of 243 slots (plan §5.2).
using PatternCode = std::uint8_t;
inline constexpr std::size_t kPatternCount = 243;

PatternCode encode(const FeedbackPattern& pattern);

// The fast feedback calculation used by the scoring loop.
PatternCode feedbackCode(const Word& guess, const Word& answer);

}  // namespace nudge
