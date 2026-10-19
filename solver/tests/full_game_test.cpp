// Slow tests: every game of the real answer list, as one decision tree (plan §8.2).
#include <gtest/gtest.h>

#include <cstdio>
#include <string>

#include "nudge/text_format.hpp"
#include "ranking.hpp"
#include "recorded_trees.hpp"
#include "test_printers.hpp"
#include "tree_walk.hpp"

namespace nudge {
namespace {

// Walked once per strategy and mode, and shared when the tests run in one
// process (ctest runs each test in its own, so there every test walks).
const GameTree& walked(const RecordedTree& recorded) {
    static std::map<std::string, GameTree> trees;
    auto found = trees.find(label(recorded));
    if (found == trees.end()) {
        found = trees.emplace(label(recorded), walkRecorded(recorded)).first;
    }
    return found->second;
}

// Prints average, worst case and the distribution (strategy specs' acceptance criteria).
void report(const RecordedTree& recorded, const GameTree& tree) {
    std::printf("%s, %s mode, opening with %s:", recorded.strategy.c_str(),
                recorded.hardMode ? "hard" : "normal", tree.moves.at("").c_str());
    for (const auto& [length, count] : tree.gamesByLength) {
        std::printf(" %d guesses: %d,", length, count);
    }
    std::printf(" average %.4f, worst %d, unsolved %zu, positions %zu\n", averageGuesses(tree),
                worstGuesses(tree), tree.unsolved.size(), tree.moves.size());
}

class FullGames : public testing::TestWithParam<RecordedTree> {};

TEST_P(FullGames, EveryAnswerIsSolved) {
    const GameTree& tree = walked(GetParam());
    report(GetParam(), tree);
    int games = 0;
    for (const auto& [length, count] : tree.gamesByLength) {
        games += count;
        // Within 6 is a must-have in normal mode only; in hard mode the spec asks
        // to record games over 6, which PlaysTheRecordedTree pins.
        if (!GetParam().hardMode) {
            EXPECT_LE(length, 6) << count << " answers needed " << length << " guesses";
        }
    }
    EXPECT_EQ(static_cast<std::size_t>(games), WordLists::builtin().answers().size());
    EXPECT_TRUE(tree.unsolved.empty());
    EXPECT_EQ(tree.countMismatches, 0);
    EXPECT_EQ(tree.replayFailures, 0);
    if (GetParam().hardMode) {
        EXPECT_EQ(tree.hardModeViolations, 0);  // every suggestion is allowed
    }
}

TEST_P(FullGames, PlaysTheRecordedTree) {
    EXPECT_EQ(treeDifferences(walked(GetParam()), GetParam()), "");
}

TEST_P(FullGames, FreshSolversPlayTheSameTree) {
    // Shared spec: the same input always gives the same guess. Every position is
    // computed again by new solvers and compared with the first walk.
    EXPECT_EQ(walkRecorded(GetParam()).moves, walked(GetParam()).moves);
}

INSTANTIATE_TEST_SUITE_P(Strategies, FullGames, testing::ValuesIn(kRecorded),
                         [](const auto& test) { return label(test.param); });

TEST(StoredFirstGuess, IsUpToDate) {
    // The walks open with the stored guess, so recompute it the way
    // nudge_precompute does instead of asking a solver. Once per strategy: the
    // first guess is the same in hard mode.
    const WordLists& lists = WordLists::builtin();
    for (const RecordedTree& recorded : kRecorded) {
        if (recorded.hardMode) {
            continue;
        }
        const Word computed =
            rankBest(lists.guesses(), lists.answers(), *makeStrategy(recorded.strategy), 0);
        EXPECT_EQ(computed.text(), recorded.opener);
        EXPECT_EQ(lists.firstGuess(recorded.strategy), computed)
            << "data/first_guesses.inc is out of date: rerun nudge_precompute";
    }
}

}  // namespace
}  // namespace nudge
