#include "nudge/entropy_strategy.hpp"

#include <cmath>

namespace nudge {

double EntropyStrategy::score(const BucketCounts& counts, std::size_t total) const {
    const auto all = static_cast<double>(total);
    double bits = 0;
    for (const std::uint32_t count : counts) {
        if (count > 0) {
            const auto n = static_cast<double>(count);
            bits += n / all * std::log2(all / n);
        }
    }
    return bits;
}

std::string_view EntropyStrategy::name() const { return "entropy"; }

}  // namespace nudge
