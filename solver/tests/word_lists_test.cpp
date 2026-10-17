#include "nudge/word_lists.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

namespace nudge {
namespace {

Word w(std::string_view text) { return *Word::parse(text); }

// The index of a word in the guess list, or nothing.
std::optional<WordLists::Index> indexOf(const WordLists& lists, std::string_view text) {
    const auto& guesses = lists.guesses();
    const auto it = std::find(guesses.begin(), guesses.end(), w(text));
    if (it == guesses.end()) {
        return std::nullopt;
    }
    return static_cast<WordLists::Index>(it - guesses.begin());
}

TEST(BuiltinWordLists, HaveEveryWordSortedAndOnce) {
    const WordLists& lists = WordLists::builtin();
    ASSERT_EQ(lists.guesses().size(), 14'855u);
    ASSERT_EQ(lists.answers().size(), 2'309u);
    EXPECT_TRUE(std::is_sorted(lists.guesses().begin(), lists.guesses().end()));
    EXPECT_EQ(std::adjacent_find(lists.guesses().begin(), lists.guesses().end()),
              lists.guesses().end());
    EXPECT_EQ(lists.guesses().front(), w("aahed"));
    EXPECT_EQ(lists.guesses().back(), w("zymic"));
}

TEST(BuiltinWordLists, AnswersPointAtTheirWordsInTheGuessList) {
    const WordLists& lists = WordLists::builtin();
    const auto& answers = lists.answers();
    EXPECT_TRUE(std::is_sorted(answers.begin(), answers.end()));
    // "cigar" was the very first Wordle answer; "aalii" is a valid guess but never an answer.
    const auto cigar = indexOf(lists, "cigar");
    const auto aalii = indexOf(lists, "aalii");
    ASSERT_TRUE(cigar && aalii);
    EXPECT_NE(std::find(answers.begin(), answers.end(), *cigar), answers.end());
    EXPECT_EQ(std::find(answers.begin(), answers.end(), *aalii), answers.end());
}

TEST(BuiltinWordLists, IsBuiltOnlyOnce) { EXPECT_EQ(&WordLists::builtin(), &WordLists::builtin()); }

TEST(WordListsFromWords, SortsAndAcceptsAnyCase) {
    const auto lists = WordLists::fromWords({"Slate", "crane"}, {"slate", "CRANE", "abbey"});
    ASSERT_TRUE(lists.has_value());
    EXPECT_EQ(lists->guesses(), (std::vector<Word>{w("abbey"), w("crane"), w("slate")}));
    EXPECT_EQ(lists->answers(), (std::vector<WordLists::Index>{1, 2}));
}

TEST(WordListsFromWords, RejectsListsTheSolverCannotUse) {
    EXPECT_FALSE(WordLists::fromWords({"crane"}, {"crane", "cr4ne"}));  // not a word
    EXPECT_FALSE(WordLists::fromWords({"cranes"}, {"crane"}));          // not a word
    EXPECT_FALSE(WordLists::fromWords({"crane"}, {"crane", "CRANE"}));  // duplicate guess
    EXPECT_FALSE(WordLists::fromWords({"crane", "crane"}, {"crane"}));  // duplicate answer
    EXPECT_FALSE(WordLists::fromWords({"crane", "slate"}, {"crane"}));  // answer not a guess
    EXPECT_FALSE(WordLists::fromWords({}, {"crane"}));                  // no answers at all
}

TEST(WordListsFromWords, RejectsMoreGuessesThanAnIndexCanHold) {
    // Index is 16 bits, so a list of 65,537 words would make indices wrap around.
    std::vector<std::string> texts;
    for (unsigned n = 0; n <= 65'536; ++n) {
        std::string text(5, 'a');
        for (unsigned i = 0, rest = n; i < 5; ++i, rest /= 26) {
            text[i] = static_cast<char>('a' + rest % 26);
        }
        texts.push_back(text);
    }
    const std::vector<std::string_view> guesses(texts.begin(), texts.end());
    EXPECT_FALSE(WordLists::fromWords({"aaaaa"}, guesses));
    const std::vector<std::string_view> justFits(guesses.begin(), guesses.end() - 1);
    EXPECT_TRUE(WordLists::fromWords({"aaaaa"}, justFits));
}

}  // namespace
}  // namespace nudge
