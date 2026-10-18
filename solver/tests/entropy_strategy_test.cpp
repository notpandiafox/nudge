#include "nudge/entropy_strategy.hpp"

#include <gtest/gtest.h>

#include <vector>

#include "ranking.hpp"
#include "test_printers.hpp"
#include "test_words.hpp"

namespace nudge {
namespace {

// Entropy spec: 4 candidates in 4 buckets of 1 is exactly 2 bits; all 4 in one bucket is 0.
TEST(EntropyStrategy, ScoresTheSpecsTwoBitAndZeroBitCases) {
    const EntropyStrategy entropy;
    BucketCounts split{};
    split[0] = split[1] = split[2] = split[3] = 1;
    EXPECT_EQ(entropy.score(split, 4), 2.0);
    BucketCounts together{};
    together[0] = 4;
    EXPECT_EQ(entropy.score(together, 4), 0.0);
}

TEST(EntropyStrategy, MatchesHandWorkedScoresOnRealGuesses) {
    const std::vector<Word> candidates = words({"crane", "stake", "stale", "stare"});
    const EntropyStrategy entropy;
    const auto scoreOf = [&](std::string_view guess) {
        return entropy.score(bucketize(w(guess), candidates), candidates.size());
    };
    // stare: groups of 1, 2, 1 (stale and stake both give GGGXG), so
    // 1/4·log2(4) + 2/4·log2(2) + 1/4·log2(4) = 0.5 + 0.5 + 0.5.
    EXPECT_DOUBLE_EQ(scoreOf("stare"), 1.5);
    EXPECT_DOUBLE_EQ(scoreOf("zzzzz"), 0.0);  // all four give XXXXX
    EXPECT_DOUBLE_EQ(scoreOf("rlkcx"), 2.0);  // all four different
}

TEST(EntropyStrategy, IsNamedEntropy) { EXPECT_EQ(EntropyStrategy{}.name(), "entropy"); }

TEST(EntropyStrategy, BestGuessCanBeANonCandidate) {
    // Guessing any of the five -ills words leaves the other four together.
    // fhkmx tests f, h, k and m at once and separates all five.
    const std::vector<Word> pool = words({"bills", "fhkmx", "fills", "hills", "kills", "mills"});
    const std::vector<Word> candidates = words({"bills", "fills", "hills", "kills", "mills"});
    EXPECT_EQ(rankBest(pool, candidates, EntropyStrategy{}, 0), w("fhkmx"));
}

TEST(EntropyStrategy, RoundingDoesNotBreakATie) {
    // Both probes split the six into groups of 3, 1, 1, 1, so their entropy is
    // equal. But the groups fall in a different pattern order (the 3 first for
    // fhmxy, last for xfhmb), and adding the same numbers in a different order
    // can round differently in the last digit. Within the tolerance they tie,
    // and the tie-break picks fhmxy alphabetically (both are non-candidates
    // with 5 new letters).
    const std::vector<Word> candidates =
        words({"balls", "bells", "bills", "fills", "hills", "mills"});
    const std::vector<Word> pool = words({"fhmxy", "xfhmb"});
    const EntropyStrategy entropy;
    const double first = entropy.score(bucketize(w("fhmxy"), candidates), candidates.size());
    const double second = entropy.score(bucketize(w("xfhmb"), candidates), candidates.size());
    // The last-digit gap is what this test is about: if a platform ever adds
    // these up equal, or the other way round, the tie no longer tests the tolerance.
    EXPECT_LT(first, second);
    EXPECT_NEAR(first, second, kTieTolerance);
    EXPECT_EQ(rankBest(pool, candidates, entropy, 0), w("fhmxy"));
}

}  // namespace
}  // namespace nudge
