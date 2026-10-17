#include "nudge/feedback.hpp"

#include <gtest/gtest.h>

#include <random>
#include <set>
#include <string>

#include "feedback_code.hpp"
#include "nudge/text_format.hpp"
#include "reference/simple_feedback.hpp"
#include "tile_printer.hpp"
#include "word.hpp"

namespace nudge {
namespace {

constexpr Tile G = Tile::Green;
constexpr Tile Y = Tile::Yellow;
constexpr Tile X = Tile::Gray;

// The shared spec's four hand-checked cases, in upper and lower case.
TEST(ComputeFeedback, MatchesTheSpecCases) {
    EXPECT_EQ(computeFeedback("SPEED", "ERASE"), (FeedbackPattern{Y, X, Y, Y, X}));
    EXPECT_EQ(computeFeedback("SPEED", "ABIDE"), (FeedbackPattern{X, X, Y, X, Y}));
    EXPECT_EQ(computeFeedback("LEVEL", "HELLO"), (FeedbackPattern{Y, G, X, X, Y}));
    EXPECT_EQ(computeFeedback("ALLEY", "HELLO"), (FeedbackPattern{X, Y, G, Y, X}));
    EXPECT_EQ(computeFeedback("speed", "erase"), (FeedbackPattern{Y, X, Y, Y, X}));
    EXPECT_EQ(computeFeedback("Level", "hello"), (FeedbackPattern{Y, G, X, X, Y}));
}

TEST(ComputeFeedback, RepeatedLetters) {
    // Repeated in the guess only: the second copy finds no unused E.
    EXPECT_EQ(computeFeedback("geese", "crane"), (FeedbackPattern{X, X, X, X, G}));
    // Repeated in the answer only: one copy in the guess gets one color.
    EXPECT_EQ(computeFeedback("crane", "geese"), (FeedbackPattern{X, X, X, X, G}));
    // Repeated in both: the Green E uses up one copy first, so only one of the
    // other two E's in the guess can be Yellow, and the leftmost one gets it.
    EXPECT_EQ(computeFeedback("eerie", "there"), (FeedbackPattern{Y, X, Y, X, G}));
    EXPECT_EQ(computeFeedback("aabbb", "bbbaa"), (FeedbackPattern{Y, Y, G, Y, Y}));
}

TEST(ComputeFeedback, InvalidWordIsACallerBugNotASilentResult) {
    EXPECT_THROW(computeFeedback("abc", "crane"), std::invalid_argument);
    EXPECT_THROW(computeFeedback("crane", "cr4ne"), std::invalid_argument);
}

TEST(EncodePattern, GivesEachOfThe243PatternsItsOwnCode) {
    EXPECT_EQ(encode({X, X, X, X, X}), 0);
    EXPECT_EQ(encode({G, G, G, G, G}), 242);
    EXPECT_EQ(encode({Y, X, X, X, X}), 1);  // position i is worth 3^i
    EXPECT_EQ(encode({X, Y, X, X, X}), 3);
    EXPECT_EQ(encode({X, X, X, X, G}), 162);

    std::set<int> codes;
    const Tile tiles[] = {X, Y, G};
    for (const Tile a : tiles)
        for (const Tile b : tiles)
            for (const Tile c : tiles)
                for (const Tile d : tiles)
                    for (const Tile e : tiles) codes.insert(encode({a, b, c, d, e}));
    EXPECT_EQ(codes.size(), kPatternCount);
    EXPECT_EQ(*codes.rbegin(), kPatternCount - 1);
}

std::string wordFrom(unsigned index, const std::string& alphabet) {
    std::string word(5, ' ');
    for (char& c : word) {
        c = alphabet[index % alphabet.size()];
        index /= static_cast<unsigned>(alphabet.size());
    }
    return word;
}

void expectSameAsReference(const std::string& guess, const std::string& answer) {
    const auto expected = parsePattern(reference::simpleFeedback(guess, answer));
    ASSERT_TRUE(expected.has_value());
    EXPECT_EQ(computeFeedback(guess, answer), *expected) << guess << " vs " << answer;
}

// Every pair of 5-letter words over {a, b, c}: 243 x 243 = 59,049 pairs.
// Nearly every pair repeats letters, including triples and more.
TEST(ComputeFeedback, MatchesReferenceOnEveryThreeLetterAlphabetPair) {
    for (unsigned g = 0; g < 243; ++g) {
        for (unsigned a = 0; a < 243; ++a) {
            expectSameAsReference(wordFrom(g, "abc"), wordFrom(a, "abc"));
        }
    }
}

TEST(ComputeFeedback, MatchesReferenceOnRandomPairs) {
    std::mt19937 rng(20261017);  // fixed seed: the same pairs every run
    std::uniform_int_distribution<unsigned> any(0, 26 * 26 * 26 * 26 * 26 - 1);
    const std::string alphabet = "abcdefghijklmnopqrstuvwxyz";
    for (int i = 0; i < 100'000; ++i) {
        expectSameAsReference(wordFrom(any(rng), alphabet), wordFrom(any(rng), alphabet));
    }
}

TEST(FeedbackCode, MatchesEncodedPublicFeedback) {
    const Word guess = *Word::parse("level");
    const Word answer = *Word::parse("hello");
    EXPECT_EQ(feedbackCode(guess, answer), encode(computeFeedback("level", "hello")));
}

}  // namespace
}  // namespace nudge
