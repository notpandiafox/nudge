#include "ranking.hpp"

#include <gtest/gtest.h>

#include <numeric>
#include <vector>

namespace nudge {
namespace {

Word w(std::string_view text) { return *Word::parse(text); }

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

}  // namespace
}  // namespace nudge
