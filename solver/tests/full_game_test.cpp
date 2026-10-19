// Slow tests: every game of the real answer list, as one decision tree (plan §8.2).
#include <gtest/gtest.h>

#include <cstdint>
#include <cstdio>
#include <ostream>
#include <string>

#include "nudge/text_format.hpp"
#include "ranking.hpp"
#include "test_printers.hpp"
#include "tree_walk.hpp"

namespace nudge {
namespace {

// The exact tree each strategy played in each mode when it was measured (plan,
// "Done in T7", "T8" and "T10"). A change to a strategy, the tie-break, hard
// mode or the word lists changes it, and must update these on purpose.
struct RecordedTree {
    std::string strategy;
    bool hardMode;
    std::string opener;
    std::map<int, int> gamesByLength;
    std::size_t positions;
    std::uint64_t
        digest;  // movesDigest: every move, so a changed guess with the same shape fails too
};

const RecordedTree kRecorded[] = {
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

// "minimax", "entropy_hard", ...: the test names and the keys of the shared walks.
std::string label(const RecordedTree& recorded) {
    return recorded.strategy + (recorded.hardMode ? "_hard" : "");
}

// Test logs show the strategy name instead of the struct's bytes.
void PrintTo(const RecordedTree& recorded, std::ostream* out) { *out << label(recorded); }

MakeStrategy named(const std::string& name) {
    return [name] { return makeStrategy(name); };
}

GameTree walk(const RecordedTree& recorded) {
    return walkAllGames(WordLists::builtin(), named(recorded.strategy), recorded.hardMode);
}

// Walked once per strategy and mode, and shared when the tests run in one
// process (ctest runs each test in its own, so there every test walks).
const GameTree& walked(const RecordedTree& recorded) {
    static std::map<std::string, GameTree> trees;
    auto found = trees.find(label(recorded));
    if (found == trees.end()) {
        found = trees.emplace(label(recorded), walk(recorded)).first;
    }
    return found->second;
}

// Prints average, worst case and the distribution (strategy specs' acceptance criteria).
void report(const RecordedTree& recorded, const GameTree& tree) {
    int games = 0;
    int guesses = 0;
    std::printf("%s, %s mode, opening with %s:", recorded.strategy.c_str(),
                recorded.hardMode ? "hard" : "normal", tree.moves.at("").c_str());
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
    const GameTree& tree = walked(GetParam());
    EXPECT_EQ(tree.moves.at(""), GetParam().opener);
    EXPECT_EQ(tree.gamesByLength, GetParam().gamesByLength);
    EXPECT_EQ(tree.moves.size(), GetParam().positions);
    EXPECT_EQ(movesDigest(tree.moves), GetParam().digest);
}

TEST_P(FullGames, FreshSolversPlayTheSameTree) {
    // Shared spec: the same input always gives the same guess. Every position is
    // computed again by new solvers and compared with the first walk.
    EXPECT_EQ(walk(GetParam()).moves, walked(GetParam()).moves);
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
