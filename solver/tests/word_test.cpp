#include "nudge/word.hpp"

#include <gtest/gtest.h>

#include <type_traits>

namespace nudge {
namespace {

TEST(WordParse, AnyCaseBecomesTheSameLowercaseWord) {
    const auto lower = Word::parse("crane");
    ASSERT_TRUE(lower.has_value());
    EXPECT_EQ(Word::parse("CRANE"), lower);
    EXPECT_EQ(Word::parse("Crane"), lower);
}

TEST(WordText, IsTheWordInLowercase) { EXPECT_EQ(Word::parse("CRane")->text(), "crane"); }

TEST(WordParse, LettersAreNumberedFromA) {
    const auto word = Word::parse("azbyc");
    ASSERT_TRUE(word.has_value());
    EXPECT_EQ(word->letters(), (std::array<std::uint8_t, 5>{0, 25, 1, 24, 2}));
}

// parse() must be the only way in: a Word built from raw letters could hold a
// value above 25 and index past the 26-letter count arrays.
static_assert(!std::is_default_constructible_v<Word>);
static_assert(!std::is_constructible_v<Word, std::array<std::uint8_t, 5>>);

TEST(WordParse, RejectsAnythingThatIsNotFiveLetters) {
    EXPECT_FALSE(Word::parse(""));
    EXPECT_FALSE(Word::parse("cran"));
    EXPECT_FALSE(Word::parse("cranes"));
    EXPECT_FALSE(Word::parse("cr4ne"));
    EXPECT_FALSE(Word::parse("cr ne"));
    EXPECT_FALSE(Word::parse("cr@ne"));        // '@' is just below 'A'
    EXPECT_FALSE(Word::parse("cr[ne"));        // '[' is just above 'Z'
    EXPECT_FALSE(Word::parse("cr`ne"));        // '`' is just below 'a'
    EXPECT_FALSE(Word::parse("cr{ne"));        // '{' is just above 'z'
    EXPECT_FALSE(Word::parse("cr\xC3\xA9n"));  // "créne": non-ASCII bytes
}

}  // namespace
}  // namespace nudge
