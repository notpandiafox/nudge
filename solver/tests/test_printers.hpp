#pragma once

#include <ostream>

#include "nudge/feedback.hpp"
#include "nudge/text_format.hpp"

namespace nudge {

// Let GoogleTest print tiles as G/Y/X and statuses by name in failure
// messages, instead of raw bytes.

// '?' for a value that is no color, which tests create on purpose.
inline void PrintTo(Tile tile, std::ostream* out) {
    const auto i = static_cast<int>(tile);
    *out << (i < 3 ? "XYG"[i] : '?');
}

inline void PrintTo(Status status, std::ostream* out) { *out << statusToText(status); }

}  // namespace nudge
