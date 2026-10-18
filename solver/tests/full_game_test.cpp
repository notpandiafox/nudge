// Slow tests: every game of the real answer list, as one decision tree (plan §8.2).
#include <gtest/gtest.h>

#include <cstdio>

#include "nudge/minimax_strategy.hpp"
#include "tree_walk.hpp"

namespace nudge {
namespace {

MakeStrategy minimax() {
    return [] { return std::make_unique<MinimaxStrategy>(); };
}

// Walked once and shared, since a walk takes seconds.
const GameTree& minimaxTree() {
    static const GameTree tree = walkAllGames(WordLists::builtin(), minimax());
    return tree;
}

// Prints average, worst case and the distribution (Knuth spec acceptance criteria).
void report(const char* name, const GameTree& tree) {
    int games = 0;
    int guesses = 0;
    std::printf("%s, normal mode, opening with %s:", name, tree.moves.at("").c_str());
    for (const auto& [length, count] : tree.gamesByLength) {
        std::printf(" %d guesses: %d,", length, count);
        games += count;
        guesses += length * count;
    }
    std::printf(" average %.4f, worst %d, unsolved %zu, positions %zu\n",
                static_cast<double>(guesses) / games, tree.gamesByLength.rbegin()->first,
                tree.unsolved.size(), tree.moves.size());
}

TEST(MinimaxFullGames, EveryAnswerIsSolvedWithinSixGuesses) {
    const GameTree& tree = minimaxTree();
    report("minimax", tree);
    int games = 0;
    for (const auto& [length, count] : tree.gamesByLength) {
        games += count;
        EXPECT_LE(length, 6) << count << " answers needed " << length << " guesses";
    }
    EXPECT_EQ(static_cast<std::size_t>(games), WordLists::builtin().answers().size());
    EXPECT_EQ(tree.countMismatches, 0);
    EXPECT_EQ(tree.replayFailures, 0);
}

TEST(MinimaxFullGames, PlaysTheRecordedTree) {
    // The exact tree T7 measured (plan, "Done in T7"). A change to the strategy,
    // the tie-break or the word lists changes it, and must update these on purpose.
    const GameTree& tree = minimaxTree();
    EXPECT_EQ(tree.moves.at(""), "olate");
    EXPECT_EQ(tree.gamesByLength, (std::map<int, int>{{2, 62}, {3, 975}, {4, 1'209}, {5, 63}}));
    EXPECT_EQ(tree.moves.size(), 2'480u);
}

TEST(MinimaxFullGames, FreshSolversPlayTheSameTree) {
    // Shared spec: the same input always gives the same guess. Every position is
    // computed again by new solvers and compared with the first walk.
    EXPECT_EQ(walkAllGames(WordLists::builtin(), minimax()).moves, minimaxTree().moves);
}

}  // namespace
}  // namespace nudge
