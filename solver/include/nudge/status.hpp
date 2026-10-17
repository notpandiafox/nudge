#pragma once

namespace nudge {

// Result of a solver call that can fail. Errors are returned, never thrown,
// because exceptions are off by default in Emscripten's WebAssembly builds.
enum class Status {
    Ok,
    InvalidWord,
    InvalidPattern,
    ImpossibleState,
    UnknownStrategy,
    GameOver,
};

}  // namespace nudge
