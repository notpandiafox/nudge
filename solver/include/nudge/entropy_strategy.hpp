#pragma once

#include "nudge/scoring_strategy.hpp"

namespace nudge {

// Expected information: how many bits, on average, the guess's colors reveal,
// with every candidate equally likely (entropy spec).
class EntropyStrategy : public ScoringStrategy {
public:
    double score(const BucketCounts& counts, std::size_t total) const override;
    std::string_view name() const override;
};

}  // namespace nudge
