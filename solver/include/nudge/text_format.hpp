#pragma once

#include <memory>
#include <optional>
#include <string_view>

#include "nudge/feedback.hpp"
#include "nudge/scoring_strategy.hpp"
#include "nudge/status.hpp"

namespace nudge {

// The text JavaScript receives for each status, e.g. "InvalidWord".
std::string_view statusToText(Status status);

// Pattern text from JavaScript: five of G (green), Y (yellow), X (gray), in
// either case, e.g. "GYXXG". Nothing if the text isn't exactly that.
std::optional<FeedbackPattern> parsePattern(std::string_view text);

// A strategy by its exact name, "minimax" or "entropy". Empty for any other text.
std::unique_ptr<ScoringStrategy> makeStrategy(std::string_view name);

}  // namespace nudge
