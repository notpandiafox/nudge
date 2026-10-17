#include "ranking.hpp"

namespace nudge {

BucketCounts bucketize(const Word& guess, std::span<const Word> candidates) {
    BucketCounts counts{};
    for (const Word& candidate : candidates) {
        ++counts[feedbackCode(guess, candidate)];
    }
    return counts;
}

}  // namespace nudge
