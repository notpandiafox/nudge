#include "nudge/wordle_solver.hpp"

#include <gtest/gtest.h>

#include <memory>

#include "nudge/minimax_strategy.hpp"
#include "nudge/text_format.hpp"
#include "test_printers.hpp"

namespace nudge {
namespace {

// Scores every guess the same: these tests are about the game state, and a
// tie makes the tie-break pick the guess, which keeps the expected words simple.
struct FixedStrategy : ScoringStrategy {
    double score(const BucketCounts& /*counts*/, std::size_t /*total*/) const override { return 0; }
    std::string_view name() const override { return "fixed"; }
};

FeedbackPattern pattern(std::string_view text) { return *parsePattern(text); }

// Answers crane and crate; grate and slate are valid guesses but never answers.
WordleSolver tinyGame() {
    static const WordLists lists =
        *WordLists::fromWords({"crane", "crate"}, {"crane", "crate", "grate", "slate"});
    return WordleSolver(std::make_unique<FixedStrategy>(), false, lists);
}

TEST(WordleSolver, StartsWithEveryAnswerPossible) {
    const WordleSolver solver = tinyGame();
    EXPECT_EQ(solver.getRemainingCount(), 2u);
    EXPECT_TRUE(solver.getVeteranWords().empty());
    EXPECT_FALSE(solver.isUsingFallback());
}

TEST(WordleSolver, FeedbackNarrowsTheCandidates) {
    WordleSolver solver = tinyGame();
    // slate against crane: XXGXG. Only crane fits; crate would have given XXGGG.
    EXPECT_EQ(solver.addFeedbackPattern("slate", pattern("XXGXG")), Status::Ok);
    EXPECT_EQ(solver.getRemainingCount(), 1u);
}

TEST(WordleSolver, StoresGuessesLowercaseWithTheirPattern) {
    WordleSolver solver = tinyGame();
    ASSERT_EQ(solver.addFeedbackPattern("SLATE", pattern("XXGXG")), Status::Ok);
    ASSERT_EQ(solver.getVeteranWords().size(), 1u);
    EXPECT_EQ(solver.getVeteranWords()[0].word, "slate");
    EXPECT_EQ(solver.getVeteranWords()[0].pattern, pattern("XXGXG"));
}

TEST(WordleSolver, AcceptsAFiveLetterWordThatIsInNeitherList) {
    WordleSolver solver = tinyGame();
    // track isn't a listed guess, but the game accepted it. Against crane: XGGYX.
    EXPECT_EQ(solver.addFeedbackPattern("track", pattern("XGGYX")), Status::Ok);
    EXPECT_EQ(solver.getRemainingCount(), 1u);  // crate would have given YGGYX
}

TEST(WordleSolver, RejectsBadWordsAndPatternsAndChangesNothing) {
    WordleSolver solver = tinyGame();
    EXPECT_EQ(solver.addFeedbackPattern("cran", pattern("XXXXX")), Status::InvalidWord);
    EXPECT_EQ(solver.addFeedbackPattern("cranes", pattern("XXXXX")), Status::InvalidWord);
    EXPECT_EQ(solver.addFeedbackPattern("cr4ne", pattern("XXXXX")), Status::InvalidWord);
    FeedbackPattern noColor = pattern("XXXXX");
    noColor[2] = static_cast<Tile>(3);
    EXPECT_EQ(solver.addFeedbackPattern("slate", noColor), Status::InvalidPattern);
    EXPECT_TRUE(solver.getVeteranWords().empty());
    EXPECT_EQ(solver.getRemainingCount(), 2u);
}

TEST(WordleSolver, ImpossibleBoardIsRejectedAndChangesNothing) {
    WordleSolver solver = tinyGame();
    ASSERT_EQ(solver.addFeedbackPattern("slate", pattern("XXGXG")), Status::Ok);
    // No word gives four greens and a yellow: the board was misread.
    EXPECT_EQ(solver.addFeedbackPattern("crane", pattern("GGGGY")), Status::ImpossibleState);
    EXPECT_EQ(solver.getVeteranWords().size(), 1u);
    EXPECT_EQ(solver.getRemainingCount(), 1u);
    EXPECT_FALSE(solver.isUsingFallback());
}

TEST(WordleSolver, FallsBackToTheGuessListWhenNoAnswerFits) {
    WordleSolver solver = tinyGame();
    // The real answer is grate, which isn't in the answer list.
    ASSERT_EQ(solver.addFeedbackPattern("slate", pattern("XXGGG")), Status::Ok);
    // Known weakness (spec, flagged concerns): crate still fits, so no fallback yet.
    EXPECT_EQ(solver.getRemainingCount(), 1u);
    EXPECT_FALSE(solver.isUsingFallback());

    ASSERT_EQ(solver.addFeedbackPattern("crate", pattern("XGGGG")), Status::Ok);
    EXPECT_TRUE(solver.isUsingFallback());
    EXPECT_EQ(solver.getRemainingCount(), 1u);  // grate

    solver.reset();
    EXPECT_FALSE(solver.isUsingFallback());
    EXPECT_TRUE(solver.getVeteranWords().empty());
    EXPECT_EQ(solver.getRemainingCount(), 2u);
}

TEST(WordleSolver, AllGreenMeansSolvedEvenForAnUnknownWord) {
    WordleSolver solver = tinyGame();
    EXPECT_EQ(solver.addFeedbackPattern("zesty", pattern("GGGGG")), Status::Ok);
    EXPECT_EQ(solver.getRemainingCount(), 1u);
    EXPECT_FALSE(solver.isUsingFallback());
}

TEST(WordleSolver, NoMoreGuessesAfterTheGameIsWon) {
    WordleSolver solver = tinyGame();
    ASSERT_EQ(solver.addFeedbackPattern("crane", pattern("GGGGG")), Status::Ok);
    EXPECT_EQ(solver.addFeedbackPattern("crate", pattern("GGGGX")), Status::GameOver);
    EXPECT_EQ(solver.getVeteranWords().size(), 1u);
}

TEST(WordleSolver, SameWordTwice) {
    WordleSolver solver = tinyGame();
    ASSERT_EQ(solver.addFeedbackPattern("slate", pattern("XXGXG")), Status::Ok);
    // Same colors again: the board shows two rows, the candidates don't change.
    EXPECT_EQ(solver.addFeedbackPattern("slate", pattern("XXGXG")), Status::Ok);
    EXPECT_EQ(solver.getVeteranWords().size(), 2u);
    EXPECT_EQ(solver.getRemainingCount(), 1u);
    // Different colors for the same word: no word can give both.
    EXPECT_EQ(solver.addFeedbackPattern("slate", pattern("XXGGG")), Status::ImpossibleState);
    EXPECT_EQ(solver.getVeteranWords().size(), 2u);
}

TEST(WordleSolver, EmptyStrategyIsRejectedAndChangesNothing) {
    WordleSolver solver = tinyGame();
    ASSERT_EQ(solver.addFeedbackPattern("slate", pattern("XXGXG")), Status::Ok);
    EXPECT_EQ(solver.setStrategy(nullptr), Status::UnknownStrategy);
    EXPECT_EQ(solver.setStrategy(std::make_unique<FixedStrategy>()), Status::Ok);
    EXPECT_EQ(solver.getVeteranWords().size(), 1u);
    EXPECT_EQ(solver.getRemainingCount(), 1u);
}

TEST(WordleSolverBestGuess, IsTheWinningWordOnceSolved) {
    WordleSolver solver = tinyGame();
    ASSERT_EQ(solver.addFeedbackPattern("zesty", pattern("GGGGG")), Status::Ok);
    EXPECT_EQ(solver.getBestGuess(), "zesty");
}

TEST(WordleSolverBestGuess, IsTheLastCandidate) {
    WordleSolver solver = tinyGame();
    ASSERT_EQ(solver.addFeedbackPattern("slate", pattern("XXGXG")), Status::Ok);
    EXPECT_EQ(solver.getBestGuess(), "crane");
}

TEST(WordleSolverBestGuess, RanksAnEmptyGame) {
    // FixedStrategy scores every guess the same, so all tie: rule 1 keeps the
    // candidates crane and crate, both have 5 new letters, and rule 3 picks crane.
    EXPECT_EQ(tinyGame().getBestGuess(), "crane");
}

TEST(WordleSolverBestGuess, SameInputGivesTheSameGuessOnFreshSolvers) {
    // Shared spec: the same input, run 10 times, gives the same guess. Each run
    // builds a new solver, so nothing can be remembered from an earlier run.
    static const WordLists lists = *WordLists::fromWords(
        {"bills", "fills", "hills", "kills", "mills"},
        {"aaaaa", "bills", "fhkma", "fhkmx", "fills", "hills", "kills", "mills"});
    for (int run = 0; run < 10; ++run) {
        WordleSolver solver(std::make_unique<MinimaxStrategy>(), false, lists);
        ASSERT_EQ(solver.addFeedbackPattern("aaaaa", pattern("XXXXX")), Status::Ok);
        EXPECT_EQ(solver.getBestGuess(), "fhkmx") << "run " << run;
    }
}

TEST(WordleSolverBestGuess, UsesTheLettersAlreadyTried) {
    // Tried letters only matter between tied non-candidates: every candidate
    // contains exactly the same tried letters. Here the probes fhkma and fhkmx
    // both separate all five -ills words, so they tie and beat every candidate.
    static const WordLists lists = *WordLists::fromWords(
        {"bills", "fills", "hills", "kills", "mills"},
        {"aaaaa", "bills", "fhkma", "fhkmx", "fills", "hills", "kills", "mills"});
    WordleSolver solver(std::make_unique<MinimaxStrategy>(), false, lists);
    ASSERT_EQ(solver.addFeedbackPattern("aaaaa", pattern("XXXXX")), Status::Ok);
    // a is tried now: fhkmx has 5 new letters, fhkma only 4. Ignoring tried
    // letters would let rule 3 pick fhkma.
    EXPECT_EQ(solver.getBestGuess(), "fhkmx");
}

TEST(WordleSolver, BuiltinListsByDefault) {
    const WordleSolver solver(std::make_unique<FixedStrategy>(), false);
    EXPECT_EQ(solver.getRemainingCount(), 2'309u);
}

}  // namespace
}  // namespace nudge
