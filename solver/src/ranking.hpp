#pragma once

#include <cstdint>
#include <span>

#include "feedback_code.hpp"
#include "nudge/scoring_strategy.hpp"
#include "nudge/word.hpp"

namespace nudge {

static_assert(std::tuple_size_v<BucketCounts> == kPatternCount);

// How many candidates would produce each of the 243 feedback patterns for one
// guess. Filling these is where nearly all of a hint's time goes (plan §5.4).
BucketCounts bucketize(const Word& guess, std::span<const Word> candidates);

struct ScoredGuess { 
    Word guess;
    double score;
};

// Scores within this of the best count as tied (shared spec, tie-break).
inline constexpr double kTieTolerance = 1e-9;

// The shared spec's tie-break: of the guesses within kTieTolerance of the best
// score, prefer a candidate (rule 1), then the most different letters not yet
// tried (rule 2), then the first alphabetically (rule 3). `scored` must not be
// empty; `candidates` must be sorted.
Word pickWinner(std::span<const ScoredGuess> scored, std::span<const Word> candidates,
                std::uint32_t triedLetters);

// The best guess from `pool` (not empty) for these candidates (sorted, not
// empty): every guess is scored, then pickWinner breaks ties.
Word rankBest(std::span<const Word> pool, std::span<const Word> candidates,
              const ScoringStrategy& strategy, std::uint32_t triedLetters);

}  // namespace nudge
