#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <ostream>
#include <string>

#include "nudge/text_format.hpp"
#include "nudge/word_lists.hpp"
#include "tree_walk.hpp"

namespace nudge {

// The exact tree each strategy played in each mode when it was measured (plan,
// "Done in T7", "T8" and "T10"). A change to a strategy, the tie-break, hard
// mode or the word lists changes it, and must update these on purpose. Shared
// by the slow tests and the benchmark, which checks the WebAssembly build
// plays the same trees (§8.4).
struct RecordedTree {
    std::string strategy;
    bool hardMode;
    std::string opener;
    std::map<int, int> gamesByLength;
    std::size_t positions;
    std::uint64_t digest;  // movesDigest: every move, so a changed guess with the same shape fails
};

inline const RecordedTree kRecorded[] = {
    {"minimax",
     false,
     "olate",
     {{2, 62}, {3, 975}, {4, 1'209}, {5, 63}},
     2'480,
     0xf1e2'4eb8'b6f5'5487u},
    {"entropy",
     false,
     "tarse",
     {{2, 61}, {3, 1'257}, {4, 936}, {5, 53}, {6, 2}},
     2'470,
     0x17ee'c0cb'3e92'7250u},
    // Hard mode: 4 and 6 answers need 7 guesses (the spec records these, it doesn't forbid them).
    {"minimax",
     true,
     "olate",
     {{2, 87}, {3, 905}, {4, 1'082}, {5, 206}, {6, 25}, {7, 4}},
     2'392,
     0x6307'0a8c'8e4e'6e99u},
    {"entropy",
     true,
     "tarse",
     {{2, 116}, {3, 1'050}, {4, 973}, {5, 133}, {6, 31}, {7, 6}},
     2'385,
     0x1ab7'531c'4d9a'668cu},
};

// "minimax", "entropy_hard", ...: the slow tests' names.
inline std::string label(const RecordedTree& recorded) {
    return recorded.strategy + (recorded.hardMode ? "_hard" : "");
}

// Test logs show the label instead of the struct's bytes.
inline void PrintTo(const RecordedTree& recorded, std::ostream* out) { *out << label(recorded); }

// Plays every game with the recorded tree's strategy and mode, on the built-in lists.
inline GameTree walkRecorded(const RecordedTree& recorded) {
    const std::string name = recorded.strategy;
    return walkAllGames(
        WordLists::builtin(), [name] { return makeStrategy(name); }, recorded.hardMode);
}

// How a played tree differs from the recorded one; empty when it's the same tree.
// The one definition of "the same", for the slow tests and the benchmark.
inline std::string treeDifferences(const GameTree& tree, const RecordedTree& recorded) {
    std::string differences;
    const auto note = [&](bool same, const std::string& what) {
        if (!same) {
            differences += what + "; ";
        }
    };
    const std::string opener = tree.moves.contains("") ? tree.moves.at("") : "none";
    note(opener == recorded.opener, "opener " + opener + ", recorded " + recorded.opener);
    note(tree.gamesByLength == recorded.gamesByLength, "the number of games per length");
    note(tree.moves.size() == recorded.positions, std::to_string(tree.moves.size()) +
                                                      " positions, recorded " +
                                                      std::to_string(recorded.positions));
    note(movesDigest(tree.moves) == recorded.digest, "the moves (digest)");
    return differences;
}

}  // namespace nudge
