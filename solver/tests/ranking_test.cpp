#include "ranking.hpp"

#include <gtest/gtest.h>

#include <numeric>
#include <vector>

#include "nudge/minimax_strategy.hpp"
#include "test_printers.hpp"
#include "test_words.hpp"

namespace nudge {
namespace {

std::uint32_t lettersOf(std::string_view text) { return w(text).mask(); }

TEST(Bucketize, CountsHowManyCandidatesGiveEachPattern) {
    const std::vector<Word> candidates{w("stare"), w("stale"), w("stake"), w("crane")};
    const BucketCounts counts = bucketize(w("stare"), candidates);

    EXPECT_EQ(counts[242], 1u);  // stare: GGGGG
    EXPECT_EQ(counts[188], 2u);  // stale and stake: GGGXG = 2 + 2*3 + 2*9 + 0*27 + 2*81
    EXPECT_EQ(counts[207], 1u);  // crane: XXGYG = 0 + 0 + 2*9 + 1*27 + 2*81
    EXPECT_EQ(std::accumulate(counts.begin(), counts.end(), 0u), candidates.size());
}

TEST(Bucketize, NoCandidatesMeansEveryBucketIsEmpty) {
    const BucketCounts counts = bucketize(w("stare"), {});
    EXPECT_EQ(std::accumulate(counts.begin(), counts.end(), 0u), 0u);
}

// --- pickWinner: the shared spec's tie-break, given scores directly ---

TEST(PickWinner, HighestScoreWinsWhenNothingTies) {
    const std::vector<ScoredGuess> scored{{w("crane"), 1}, {w("slate"), 3}, {w("chart"), 2}};
    EXPECT_EQ(pickWinner(scored, {}, 0), w("slate"));
}

TEST(PickWinner, Rule1PrefersACandidate) {
    const std::vector<ScoredGuess> scored{{w("crane"), 2}, {w("slate"), 2}};
    EXPECT_EQ(pickWinner(scored, words({"slate"}), 0), w("slate"));  // crane would win rule 3
}

TEST(PickWinner, Rule1ComesBeforeRule2) {
    // crane has 5 new letters, abbey only 4, but abbey is a candidate.
    const std::vector<ScoredGuess> scored{{w("abbey"), 2}, {w("crane"), 2}};
    EXPECT_EQ(pickWinner(scored, words({"abbey"}), 0), w("abbey"));
}

TEST(PickWinner, Rule1KeepsEveryoneWhenNoTiedGuessIsACandidate) {
    const std::vector<ScoredGuess> scored{{w("slate"), 2}, {w("crane"), 2}};
    EXPECT_EQ(pickWinner(scored, words({"abbey"}), 0), w("crane"));  // down to rule 3
}

TEST(PickWinner, Rule2PrefersMoreLettersNotYetTried) {
    // abbey has 4 different letters, crane 5; nothing tried yet.
    const std::vector<ScoredGuess> scored{{w("abbey"), 2}, {w("crane"), 2}};
    EXPECT_EQ(pickWinner(scored, {}, 0), w("crane"));
    // After trying c, r, a, n, e: crane has no new letters, abbey has b and y.
    EXPECT_EQ(pickWinner(scored, {}, lettersOf("crane")), w("abbey"));
}

TEST(PickWinner, Rule3TakesTheFirstAlphabetically) {
    const std::vector<ScoredGuess> scored{{w("slate"), 2}, {w("crane"), 2}};
    EXPECT_EQ(pickWinner(scored, {}, 0), w("crane"));
}

TEST(PickWinner, ScoresWithinTheToleranceAreTied) {
    const std::vector<ScoredGuess> tied{{w("crane"), 1.0}, {w("slate"), 1.0 + 5e-10}};
    EXPECT_EQ(pickWinner(tied, {}, 0), w("crane"));  // tied, so rule 3
    const std::vector<ScoredGuess> apart{{w("crane"), 1.0}, {w("slate"), 1.0 + 2e-9}};
    EXPECT_EQ(pickWinner(apart, {}, 0), w("slate"));  // not tied: the higher score
}

TEST(PickWinner, TiedMeansWithinToleranceOfTheBestNotOfEachOther) {
    // chart is 0.8e-9 below the best (tied); brick is 1.6e-9 below (not tied),
    // even though brick is within 1e-9 of chart. All have 5 new letters, so
    // rule 3 decides: chart if only chart joins, brick if brick wrongly joins.
    const std::vector<ScoredGuess> scored{
        {w("slate"), 1.0}, {w("chart"), 1.0 - 0.8e-9}, {w("brick"), 1.0 - 1.6e-9}};
    EXPECT_EQ(pickWinner(scored, {}, 0), w("chart"));
}

// --- rankBest: every guess scored, then the tie-break ---

TEST(RankBest, PicksANonCandidateWhenItSplitsBetter) {
    // Guessing any of the five -ills words leaves the other four together.
    // fhkmx tests f, h, k and m at once and separates all five.
    const std::vector<Word> pool = words({"bills", "fhkmx", "fills", "hills", "kills", "mills"});
    const std::vector<Word> candidates = words({"bills", "fills", "hills", "kills", "mills"});
    EXPECT_EQ(rankBest(pool, candidates, MinimaxStrategy{}, 0), w("fhkmx"));
}

TEST(RankBest, OneCandidateLeftIsTheAnswer) {
    const std::vector<Word> pool = words({"crane", "slate"});
    EXPECT_EQ(rankBest(pool, words({"slate"}), MinimaxStrategy{}, 0), w("slate"));
}

}  // namespace
}  // namespace nudge
