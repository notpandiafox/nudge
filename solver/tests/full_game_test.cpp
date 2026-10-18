// Slow tests: every game of the real answer list, as one decision tree (plan §8.2).
#include <gtest/gtest.h>

#include <cstdint>
#include <cstdio>
#include <ostream>
#include <string>

#include "nudge/text_format.hpp"
#include "tree_walk.hpp"

namespace nudge {
namespace {

// The exact tree each strategy played when it was measured (plan, "Done in T7"
// and "Done in T8"). A change to a strategy, the tie-break or the word lists
// changes it, and must update these on purpose.
struct RecordedTree {
    std::string strategy;
    std::string opener;
    std::map<int, int> gamesByLength;
    std::size_t positions;
    std::uint64_t
        digest;  // movesDigest: every move, so a changed guess with the same shape fails too
};

const RecordedTree kRecorded[] = {
    {"minimax", "olate", {{2, 62}, {3, 975}, {4, 1'209}, {5, 63}}, 2'480, 0xf1e2'4eb8'b6f5'5487u},
    {"entropy",
     "tarse",
     {{2, 61}, {3, 1'257}, {4, 936}, {5, 53}, {6, 2}},
     2'470,
     0x17ee'c0cb'3e92'7250u},
};

// Test logs show the strategy name instead of the struct's bytes.
void PrintTo(const RecordedTree& recorded, std::ostream* out) { *out << recorded.strategy; }

MakeStrategy named(const std::string& name) {
    return [name] { return makeStrategy(name); };
}

// Walked once per strategy and shared, since a walk takes seconds.
const GameTree& walked(const std::string& name) {
    static std::map<std::string, GameTree> trees;
    auto found = trees.find(name);
    if (found == trees.end()) {
        found = trees.emplace(name, walkAllGames(WordLists::builtin(), named(name))).first;
    }
    return found->second;
}

// Prints average, worst case and the distribution (strategy specs' acceptance criteria).
void report(const std::string& name, const GameTree& tree) {
    int games = 0;
    int guesses = 0;
    std::printf("%s, normal mode, opening with %s:", name.c_str(), tree.moves.at("").c_str());
    for (const auto& [length, count] : tree.gamesByLength) {
        std::printf(" %d guesses: %d,", length, count);
        games += count;
        guesses += length * count;
    }
    std::printf(" average %.4f, worst %d, unsolved %zu, positions %zu\n",
                static_cast<double>(guesses) / games, tree.gamesByLength.rbegin()->first,
                tree.unsolved.size(), tree.moves.size());
}

class FullGames : public testing::TestWithParam<RecordedTree> {};

TEST_P(FullGames, EveryAnswerIsSolvedWithinSixGuesses) {
    const GameTree& tree = walked(GetParam().strategy);
    report(GetParam().strategy, tree);
    int games = 0;
    for (const auto& [length, count] : tree.gamesByLength) {
        games += count;
        EXPECT_LE(length, 6) << count << " answers needed " << length << " guesses";
    }
    EXPECT_EQ(static_cast<std::size_t>(games), WordLists::builtin().answers().size());
    EXPECT_EQ(tree.countMismatches, 0);
    EXPECT_EQ(tree.replayFailures, 0);
}

TEST_P(FullGames, PlaysTheRecordedTree) {
    const GameTree& tree = walked(GetParam().strategy);
    EXPECT_EQ(tree.moves.at(""), GetParam().opener);
    EXPECT_EQ(tree.gamesByLength, GetParam().gamesByLength);
    EXPECT_EQ(tree.moves.size(), GetParam().positions);
    EXPECT_EQ(movesDigest(tree.moves), GetParam().digest);
}

TEST_P(FullGames, FreshSolversPlayTheSameTree) {
    // Shared spec: the same input always gives the same guess. Every position is
    // computed again by new solvers and compared with the first walk.
    const std::string& name = GetParam().strategy;
    EXPECT_EQ(walkAllGames(WordLists::builtin(), named(name)).moves, walked(name).moves);
}

INSTANTIATE_TEST_SUITE_P(Strategies, FullGames, testing::ValuesIn(kRecorded),
                         [](const auto& test) { return test.param.strategy; });

}  // namespace
}  // namespace nudge
