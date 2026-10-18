#include "nudge/word_lists.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "test_words.hpp"

namespace nudge {
namespace {

bool contains(const std::vector<Word>& words, std::string_view text) {
    return std::binary_search(words.begin(), words.end(), w(text));
}

TEST(BuiltinWordLists, HaveEveryWordSortedAndOnce) {
    const WordLists& lists = WordLists::builtin();
    ASSERT_EQ(lists.guesses().size(), 14'855u);
    ASSERT_EQ(lists.answers().size(), 2'309u);
    for (const auto* words : {&lists.guesses(), &lists.answers()}) {
        EXPECT_TRUE(std::is_sorted(words->begin(), words->end()));
        EXPECT_EQ(std::adjacent_find(words->begin(), words->end()), words->end());
    }
    EXPECT_EQ(lists.guesses().front(), w("aahed"));
    EXPECT_EQ(lists.guesses().back(), w("zymic"));
}

TEST(BuiltinWordLists, EveryAnswerIsAlsoAGuess) {
    const WordLists& lists = WordLists::builtin();
    EXPECT_TRUE(std::includes(lists.guesses().begin(), lists.guesses().end(),
                              lists.answers().begin(), lists.answers().end()));
    // "cigar" was the very first Wordle answer; "aalii" is a valid guess but never an answer.
    EXPECT_TRUE(contains(lists.answers(), "cigar"));
    EXPECT_TRUE(contains(lists.guesses(), "aalii"));
    EXPECT_FALSE(contains(lists.answers(), "aalii"));
}

TEST(BuiltinWordLists, IsBuiltOnlyOnce) { EXPECT_EQ(&WordLists::builtin(), &WordLists::builtin()); }

TEST(WordListsFromWords, SortsAndAcceptsAnyCase) {
    const auto lists = WordLists::fromWords({"Slate", "crane"}, {"slate", "CRANE", "abbey"});
    ASSERT_TRUE(lists.has_value());
    EXPECT_EQ(lists->guesses(), (std::vector<Word>{w("abbey"), w("crane"), w("slate")}));
    EXPECT_EQ(lists->answers(), (std::vector<Word>{w("crane"), w("slate")}));
}

TEST(WordListsFromWords, RejectsListsTheSolverCannotUse) {
    EXPECT_FALSE(WordLists::fromWords({"crane"}, {"crane", "cr4ne"}));  // not a word
    EXPECT_FALSE(WordLists::fromWords({"cranes"}, {"crane"}));          // not a word
    EXPECT_FALSE(WordLists::fromWords({"crane"}, {"crane", "CRANE"}));  // duplicate guess
    EXPECT_FALSE(WordLists::fromWords({"crane", "crane"}, {"crane"}));  // duplicate answer
    EXPECT_FALSE(WordLists::fromWords({"crane", "slate"}, {"crane"}));  // answer not a guess
    EXPECT_FALSE(WordLists::fromWords({}, {"crane"}));                  // no answers at all
}

}  // namespace
}  // namespace nudge
