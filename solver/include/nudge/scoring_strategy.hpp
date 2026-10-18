#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace nudge {

// One counter per possible feedback pattern (3^5 = 243): how many candidates
// would produce that pattern for a given guess.
using BucketCounts = std::array<std::uint32_t, 243>;

// How a strategy rates a guess. MinimaxStrategy and EntropyStrategy (T8)
// implement this; the solver builds the bucket counts (plan §5.6).
class ScoringStrategy {
public:
    virtual ~ScoringStrategy();
    // Scores one guess from its bucket counts; total = number of candidates.
    // Higher is always better.
    virtual double score(const BucketCounts& counts, std::size_t total) const = 0;
    virtual std::string_view name() const = 0;  // "minimax" or "entropy"
};

}  // namespace nudge
