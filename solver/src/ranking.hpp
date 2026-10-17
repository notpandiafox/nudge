#pragma once

#include <span>

#include "feedback_code.hpp"
#include "nudge/scoring_strategy.hpp"
#include "nudge/word.hpp"

namespace nudge {

static_assert(std::tuple_size_v<BucketCounts> == kPatternCount);

// How many candidates would produce each of the 243 feedback patterns for one
// guess. Filling these is where nearly all of a hint's time goes (plan §5.4).
BucketCounts bucketize(const Word& guess, std::span<const Word> candidates);

}  // namespace nudge
