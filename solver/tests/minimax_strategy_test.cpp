#include "nudge/minimax_strategy.hpp"

#include <gtest/gtest.h>

#include <vector>

#include "ranking.hpp"
#include "test_words.hpp"

namespace nudge {
namespace {

// Knuth spec: on a small hand-built set, the worst-case group for several
// guesses matches values worked out by hand.
TEST(MinimaxStrategy, ScoresMinusTheLargestGroup) {
    const std::vector<Word> candidates{w("stare"), w("stale"), w("stake"), w("crane")};
    const MinimaxStrategy minimax;
    const auto scoreOf = [&](std::string_view guess) {
        return minimax.score(bucketize(w(guess), candidates), candidates.size());
    };
    EXPECT_EQ(scoreOf("stare"), -2);  // stale and stake both give GGGXG
    EXPECT_EQ(scoreOf("zzzzz"), -4);  // all four give XXXXX
    EXPECT_EQ(scoreOf("rlkcx"), -1);  // YXXXX, XYXXX, XXYXX, YXXYX: all different
}

TEST(MinimaxStrategy, IsNamedMinimax) { EXPECT_EQ(MinimaxStrategy{}.name(), "minimax"); }

}  // namespace
}  // namespace nudge
