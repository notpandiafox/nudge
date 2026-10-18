#include "nudge/word_lists.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "test_printers.hpp"
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

TEST(WordListsFirstGuess, IsStoredByStrategyName) {
    const auto lists = WordLists::fromWords({"crane", "crate"}, {"crane", "crate", "slate"},
                                            {{"minimax", "Slate"}});
    ASSERT_TRUE(lists.has_value());
    EXPECT_EQ(lists->firstGuess("minimax"), w("slate"));
    EXPECT_EQ(lists->firstGuess("entropy"), std::nullopt);  // not stored: the solver computes it
}

TEST(WordListsFirstGuess, NoneStoredByDefault) {
    const auto lists = WordLists::fromWords({"crane"}, {"crane"});
    ASSERT_TRUE(lists.has_value());
    EXPECT_EQ(lists->firstGuess("minimax"), std::nullopt);
}

TEST(WordListsFirstGuess, MustBeAGuessInTheList) {
    // slate is a word, but not in this guess list; cr4ne isn't a word at all.
    EXPECT_FALSE(WordLists::fromWords({"crane"}, {"crane"}, {{"minimax", "slate"}}));
    EXPECT_FALSE(WordLists::fromWords({"crane"}, {"crane"}, {{"minimax", "cr4ne"}}));
}

TEST(BuiltinWordLists, StoreTheFirstGuessOfEachStrategy) {
    // The openers the full-game walks measured in T7 and T8 (plan, "Done in T9").
    EXPECT_EQ(WordLists::builtin().firstGuess("minimax"), w("olate"));
    EXPECT_EQ(WordLists::builtin().firstGuess("entropy"), w("tarse"));
}

}  // namespace
}  // namespace nudge
