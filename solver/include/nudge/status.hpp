#pragma once

#include <cstdint>

namespace nudge {

// Result of a solver call that can fail. Errors are returned, never thrown,
// because exceptions are off by default in Emscripten's WebAssembly builds.
enum class Status : std::uint8_t {
    Ok,
    InvalidWord,
    InvalidPattern,
    ImpossibleState,
    UnknownStrategy,
    GameOver,
};

}  // namespace nudge
