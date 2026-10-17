#pragma once

#include <ostream>

#include "nudge/feedback.hpp"

namespace nudge {

// Lets GoogleTest print tiles as G/Y/X in failure messages, instead of raw bytes.
// '?' for a value that is no color, which tests create on purpose.
inline void PrintTo(Tile tile, std::ostream* out) {
    const auto i = static_cast<int>(tile);
    *out << (i < 3 ? "XYG"[i] : '?');
}

}  // namespace nudge
