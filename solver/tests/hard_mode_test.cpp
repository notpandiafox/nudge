#include "hard_mode.hpp"

#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

#include "nudge/text_format.hpp"
#include "nudge/word_lists.hpp"
#include "reference/simple_feedback.hpp"
#include "reference/simple_hard_mode.hpp"
#include "test_words.hpp"

namespace nudge {
namespace {

using History = std::vector<std::pair<std::string, std::string>>;  // (guess, pattern text)

HardModeConstraints after(const History& history) {
    std::vector<VeteranWord> veterans;
    for (const auto& [guess, pattern] : history) {
        veterans.push_back({guess, *parsePattern(pattern)});
    }
    return hardModeConstraints(veterans);
}

bool allowed(const History& history, std::string_view guess) {
    return isHardModeLegal(w(guess), after(history));
}

TEST(HardMode, NothingIsRequiredBeforeTheFirstGuess) { EXPECT_TRUE(allowed({}, "zzzzz")); }

TEST(HardMode, RepeatedLitLettersNeedRepeatedCopies) {
    // Shared spec: SPEED against ERASE lights up S, E and E (YXYYX), so the
    // next guess needs an S and two E's.
    ASSERT_EQ(reference::simpleFeedback("speed", "erase"), "YXYYX");
    EXPECT_TRUE(allowed({{"speed", "YXYYX"}}, "easel"));
    EXPECT_FALSE(allowed({{"speed", "YXYYX"}}, "senor"));  // only one E
}

TEST(HardMode, AGrayCopyOfALitLetterAddsNoRequirement) {
    // Shared spec: SPEED against ABIDE lights one E and the D (XXYXY); the
    // second E is gray, so one E plus a D is enough.
    ASSERT_EQ(reference::simpleFeedback("speed", "abide"), "XXYXY");
    EXPECT_TRUE(allowed({{"speed", "XXYXY"}}, "ideal"));
    EXPECT_FALSE(allowed({{"speed", "XXYXY"}}, "eerie"));  // no D
}

TEST(HardMode, GreenLettersStayInPlace) {
    EXPECT_TRUE(allowed({{"crane", "GXXXX"}}, "cloth"));
    EXPECT_FALSE(allowed({{"crane", "GXXXX"}}, "local"));  // has a C, in the wrong place
}

TEST(HardMode, YellowLettersMayBeAnywhereEvenTheSamePlace) {
    EXPECT_TRUE(allowed({{"crane", "YXXXX"}}, "cloth"));
    EXPECT_TRUE(allowed({{"crane", "YXXXX"}}, "local"));
    EXPECT_FALSE(allowed({{"crane", "YXXXX"}}, "plush"));  // no C
}

TEST(HardMode, GrayLettersMayBeUsedAgain) { EXPECT_TRUE(allowed({{"crane", "XXXXX"}}, "crane")); }

TEST(HardMode, GreenAndYellowCopiesOfALetterAddUp) {
    // EERIE against EVERY: E green, then a yellow E and a yellow R (GYYXX):
    // E in first place, two E's in all, and an R.
    ASSERT_EQ(reference::simpleFeedback("eerie", "every"), "GYYXX");
    EXPECT_TRUE(allowed({{"eerie", "GYYXX"}}, "ember"));
    EXPECT_FALSE(allowed({{"eerie", "GYYXX"}}, "error"));  // one E
    EXPECT_FALSE(allowed({{"eerie", "GYYXX"}}, "steer"));  // two E's, none in first place
}

TEST(HardMode, CountsAreTheMostFromAnyOneGuessNotTheSum) {
    // Each guess lights one E, so one E is enough.
    const History history{{"elbow", "YXXXX"}, {"there", "XXXXY"}};
    EXPECT_TRUE(allowed(history, "abide"));
    EXPECT_FALSE(allowed(history, "audio"));
}

TEST(HardMode, EveryCandidateIsAllowed) {
    // Plan §5.12: a word that fits all the feedback has every Green in place and
    // every lit copy, so the hard-mode pool can't lose a candidate.
    const WordLists& lists = WordLists::builtin();
    for (const std::string answer : {"every", "erase", "abide", "sissy", "llama"}) {
        History history;
        for (const std::string guess : {"speed", "eerie", "salsa"}) {
            history.emplace_back(guess, reference::simpleFeedback(guess, answer));
        }
        const HardModeConstraints constraints = after(history);
        EXPECT_TRUE(isHardModeLegal(w(answer), constraints)) << answer;
        for (const Word& word : lists.answers()) {
            bool fits = true;
            for (const auto& [guess, pattern] : history) {
                fits = fits && reference::simpleFeedback(guess, word.text()) == pattern;
            }
            if (fits) {
                EXPECT_TRUE(isHardModeLegal(word, constraints)) << word.text() << " for " << answer;
            }
        }
    }
}

TEST(HardMode, AgreesWithTheSimpleCheckerOnEveryGuess) {
    // The fast rule against the reference rule (plan §8.1), for every word in
    // the guess list after several histories with repeated letters.
    const std::vector<History> histories{
        {{"speed", "YXYYX"}},
        {{"speed", "XXYXY"}},
        {{"eerie", "GYYXX"}},
        {{"elbow", "YXXXX"}, {"there", "XXXXY"}},
        {{"salsa", "YXXGY"}, {"assay", "YYGXX"}},
        {{"crane", "XGYXG"}, {"tribe", "XGXXG"}},
    };
    const std::vector<Word>& guesses = WordLists::builtin().guesses();
    for (std::size_t h = 0; h < histories.size(); ++h) {
        const HardModeConstraints constraints = after(histories[h]);
        std::size_t allowedCount = 0;
        for (const Word& word : guesses) {
            const bool fast = isHardModeLegal(word, constraints);
            ASSERT_EQ(fast, reference::simpleHardModeAllows(histories[h], word.text()))
                << word.text() << " after history " << h;
            allowedCount += fast ? 1 : 0;
        }
        // Each history rules out some words and keeps some, so both answers get checked.
        EXPECT_GT(allowedCount, 0u) << "history " << h;
        EXPECT_LT(allowedCount, guesses.size()) << "history " << h;
    }
}

}  // namespace
}  // namespace nudge
