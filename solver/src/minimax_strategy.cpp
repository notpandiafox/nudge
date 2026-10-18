#include "nudge/minimax_strategy.hpp"

#include <algorithm>

namespace nudge {

double MinimaxStrategy::score(const BucketCounts& counts, std::size_t /*total*/) const {
    return -static_cast<double>(*std::max_element(counts.begin(), counts.end()));
}

std::string_view MinimaxStrategy::name() const { return "minimax"; }

}  // namespace nudge
