#include "ranking.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cmath>
#include <vector>

namespace nudge {

BucketCounts bucketize(const Word& guess, std::span<const Word> candidates) {
    BucketCounts counts{};
    for (const Word& candidate : candidates) {
        ++counts[feedbackCode(guess, candidate)];
    }
    return counts;
}

Word pickWinner(std::span<const ScoredGuess> scored, std::span<const Word> candidates,
                std::uint32_t triedLetters) {
    assert(!scored.empty());
    assert(std::is_sorted(candidates.begin(), candidates.end()));
    // Tied = within the tolerance of the best, not of each other (plan §5.7),
    // so the result doesn't depend on the order the guesses come in.
    double best = scored.front().score;
    for (const ScoredGuess& s : scored) {
        best = std::max(best, s.score);
    }
    std::vector<Word> tied;
    for (const ScoredGuess& s : scored) {
        if (s.score >= best - kTieTolerance) {
            tied.push_back(s.guess);
        }
    }
    // Rule 1: if any tied guess is still a candidate, only candidates stay.
    const auto isCandidate = [&](const Word& word) {
        return std::binary_search(candidates.begin(), candidates.end(), word);
    };
    if (std::any_of(tied.begin(), tied.end(), isCandidate)) {
        std::erase_if(tied, [&](const Word& word) { return !isCandidate(word); });
    }
    // Rule 2: most different letters not tried yet. Rule 3: first alphabetically.
    const auto newLetters = [&](const Word& word) {
        return std::popcount(word.mask() & ~triedLetters);
    };
    return *std::min_element(tied.begin(), tied.end(), [&](const Word& a, const Word& b) {
        const int newA = newLetters(a);
        const int newB = newLetters(b);
        return newA != newB ? newA > newB : a < b;
    });
}

Word rankBest(std::span<const Word> pool, std::span<const Word> candidates,
              const ScoringStrategy& strategy, std::uint32_t triedLetters) {
    std::vector<ScoredGuess> scored;
    scored.reserve(pool.size());
    for (const Word& guess : pool) {
        const double score = strategy.score(bucketize(guess, candidates), candidates.size());
        // A NaN would drop out of every tie comparison and leave pickWinner nothing.
        assert(!std::isnan(score));
        scored.push_back({guess, score});
    }
    return pickWinner(scored, candidates, triedLetters);
}

}  // namespace nudge
