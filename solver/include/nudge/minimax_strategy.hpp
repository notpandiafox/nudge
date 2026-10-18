#pragma once

#include "nudge/scoring_strategy.hpp"

namespace nudge {

// Knuth's minimax rule: a guess is as good as its worst case, so it scores
// minus the size of the largest group of candidates it could leave.
class MinimaxStrategy : public ScoringStrategy {
public:
    double score(const BucketCounts& counts, std::size_t total) const override;
    std::string_view name() const override;
};

}  // namespace nudge
