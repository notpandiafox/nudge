#include "worst_cases.hpp"

#include <gtest/gtest.h>

#include "test_words.hpp"

namespace nudge {
namespace {

// Three answers that share "abcd", plus two guesses that share no letter with them.
WordLists tinyLists() {
    return *WordLists::fromWords({"abcde", "abcdf", "abcdg"},
                                 {"abcde", "abcdf", "abcdg", "yyyyy", "zzzzz"});
}

TEST(WorstOpener, IsTheGuessLeavingTheLargestGroupOfAnswers) {
    // yyyyy and zzzzz both leave all 3 answers in the all-gray group; yyyyy comes first.
    const WorstCase worst = worstOpener(tinyLists());
    EXPECT_EQ(worst.opener, w("yyyyy"));
    EXPECT_EQ(worst.pattern, 0);  // all gray
    EXPECT_EQ(worst.candidates, (std::vector<Word>{w("abcde"), w("abcdf"), w("abcdg")}));
}

TEST(WorstFallback, IsThePatternNoAnswerGivesButMostGuessesDo) {
    // After abcde, all gray rules out every answer but fits yyyyy and zzzzz.
    const WorstCase worst = worstFallback(tinyLists());
    EXPECT_EQ(worst.opener, w("abcde"));
    EXPECT_EQ(worst.pattern, 0);
    EXPECT_EQ(worst.candidates, (std::vector<Word>{w("yyyyy"), w("zzzzz")}));
}

}  // namespace
}  // namespace nudge
