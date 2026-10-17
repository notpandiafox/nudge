#pragma once

#include <array>
#include <cstdint>
#include <span>

#include "feedback_code.hpp"
#include "nudge/word.hpp"

namespace nudge {

// How many candidates would produce each of the 243 feedback patterns for one
// guess. Filling these is where nearly all of a hint's time goes (plan §5.4).
using BucketCounts = std::array<std::uint32_t, kPatternCount>;

BucketCounts bucketize(const Word& guess, std::span<const Word> candidates);

}  // namespace nudge
