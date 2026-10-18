#include "tree_walk.hpp"

#include <gtest/gtest.h>

#include <algorithm>

#include "nudge/minimax_strategy.hpp"

namespace nudge {
namespace {

const MakeStrategy minimax = [] { return std::make_unique<MinimaxStrategy>(); };

// Answers crane and crate; slate is a guess only.
WordLists twoAnswers() {
    return *WordLists::fromWords({"crane", "crate"}, {"crane", "crate", "slate"});
}

TEST(TreeWalk, PlaysEveryGameOfATinyListByHand) {
    // Both answers split the two perfectly and tie; rule 3 opens with crane.
    // crane is solved in 1; crate gets GGGXG, then is solved in 2.
    const GameTree tree = walkAllGames(twoAnswers(), minimax);
    EXPECT_EQ(tree.gamesByLength, (std::map<int, int>{{1, 1}, {2, 1}}));
    EXPECT_TRUE(tree.unsolved.empty());
    EXPECT_EQ(tree.countMismatches, 0);
    EXPECT_EQ(tree.replayFailures, 0);
    EXPECT_EQ(tree.moves,
              (std::map<std::string, std::string>{{"", "crane"}, {"crane:GGGXG ", "crate"}}));
}

TEST(TreeWalk, DigestsEveryMove) {
    // Expected values from an independent FNV-1a in Python, which matches the
    // published test values (e.g. "foobar" -> 0x85944171f73967e8).
    EXPECT_EQ(movesDigest({}), 0xcbf29ce484222325u);  // nothing hashed: the FNV offset basis
    std::map<std::string, std::string> moves = walkAllGames(twoAnswers(), minimax).moves;
    // Hashes "=crane\ncrane:GGGXG =crate\n": position=guess, one move per line.
    EXPECT_EQ(movesDigest(moves), 0x37ed8b96991ef816u);
    moves["crane:GGGXG "] = "slate";  // one different move, same shape
    EXPECT_NE(movesDigest(moves), 0x37ed8b96991ef816u);
}

TEST(TreeWalk, StopsAtTheDepthLimit) {
    // This strategy prefers the worst guess: zzzzz, which keeps both answers
    // together forever, so the walk must stop at the limit instead of looping.
    struct WorstGuess : ScoringStrategy {
        double score(const BucketCounts& counts, std::size_t /*total*/) const override {
            return static_cast<double>(*std::max_element(counts.begin(), counts.end()));
        }
        std::string_view name() const override { return "worst-guess"; }
    };
    const WordLists lists = *WordLists::fromWords({"crane", "crate"}, {"crane", "crate", "zzzzz"});
    const GameTree tree = walkAllGames(lists, [] { return std::make_unique<WorstGuess>(); });
    EXPECT_TRUE(tree.gamesByLength.empty());
    EXPECT_EQ(tree.unsolved, (std::vector<std::string>{"crane", "crate"}));
    EXPECT_EQ(tree.moves.size(), static_cast<std::size_t>(kDepthLimit));  // one zzzzz per guess
}

// The two detectors must be able to fire, or the slow test's "0 mismatches,
// 0 replay failures" would prove nothing.
TEST(TreeWalk, CountsAPositionWhereTheSolverAndTheReferenceDisagree) {
    // The solver has two candidates, but the walk is told only crane is left.
    GameTree tree;
    std::vector<VeteranWord> veterans;
    walkFrom(twoAnswers(), minimax, veterans, "", {"crane"}, tree);
    EXPECT_EQ(tree.countMismatches, 1);
}

TEST(TreeWalk, CountsAVeteranWordTheSolverRefuses) {
    GameTree tree;
    std::vector<VeteranWord> veterans{{"cr4ne", *parsePattern("XXXXX")}};
    walkFrom(twoAnswers(), minimax, veterans, "", {"crane", "crate"}, tree);
    EXPECT_GT(tree.replayFailures, 0);
}

}  // namespace
}  // namespace nudge
