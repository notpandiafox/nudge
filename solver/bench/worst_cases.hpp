#pragma once

#include <vector>

#include "feedback_code.hpp"
#include "nudge/word_lists.hpp"
#include "ranking.hpp"

namespace nudge {

// A first guess, the colors it got, and the candidates left afterwards.
struct WorstCase {
    Word opener;
    PatternCode pattern;
    std::vector<Word> candidates;
};

inline std::vector<Word> wordsGiving(const Word& opener, PatternCode pattern,
                                     const std::vector<Word>& words) {
    std::vector<Word> result;
    for (const Word& word : words) {
        if (feedbackCode(opener, word) == pattern) {
            result.push_back(word);
        }
    }
    return result;
}

// The slowest possible second turn: the first guess and colors that leave the
// most answers still possible (plan §5.9). Ties go to the earliest guess.
inline WorstCase worstOpener(const WordLists& lists) {
    const std::vector<Word>& answers = lists.answers();
    WorstCase worst{lists.guesses().front(), 0, {}};
    std::uint32_t largest = 0;
    for (const Word& opener : lists.guesses()) {
        const BucketCounts counts = bucketize(opener, answers);
        for (std::size_t p = 0; p < kPatternCount; ++p) {
            if (counts[p] > largest) {
                largest = counts[p];
                worst.opener = opener;
                worst.pattern = static_cast<PatternCode>(p);
            }
        }
    }
    worst.candidates = wordsGiving(worst.opener, worst.pattern, answers);
    return worst;
}

// The slowest early fallback: the first guess and colors that rule out every
// answer but leave the most guess-list words. Ties go to the earliest guess.
inline WorstCase worstFallback(const WordLists& lists) {
    const std::vector<Word>& answers = lists.answers();
    WorstCase worst{lists.guesses().front(), 0, {}};
    std::uint32_t largest = 0;
    for (const Word& opener : lists.guesses()) {
        const BucketCounts answerCounts = bucketize(opener, answers);
        const BucketCounts guessCounts = bucketize(opener, lists.guesses());
        for (std::size_t p = 0; p < kPatternCount; ++p) {
            if (answerCounts[p] == 0 && guessCounts[p] > largest) {
                largest = guessCounts[p];
                worst.opener = opener;
                worst.pattern = static_cast<PatternCode>(p);
            }
        }
    }
    worst.candidates = wordsGiving(worst.opener, worst.pattern, lists.guesses());
    return worst;
}

}  // namespace nudge
