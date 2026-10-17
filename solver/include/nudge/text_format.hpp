#pragma once

#include <string_view>

#include "nudge/status.hpp"

namespace nudge {

// The text JavaScript receives for each status, e.g. "InvalidWord".
std::string_view statusToText(Status status);

}  // namespace nudge
