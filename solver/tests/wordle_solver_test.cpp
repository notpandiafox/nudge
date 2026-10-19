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

// The pattern as text, the way JavaScript sends it (plan §5.15).
TEST(WordleSolverPatternText, ReadsTheTextLikeParsePattern) {
    WordleSolver solver = tinyGame();
    EXPECT_EQ(solver.addFeedbackPattern("slate", "xxGgG"), Status::Ok);  // either case
    ASSERT_EQ(solver.getVeteranWords().size(), 1u);
    EXPECT_EQ(solver.getVeteranWords()[0].pattern, pattern("XXGGG"));
    EXPECT_EQ(solver.getRemainingCount(), 1u);
}

TEST(WordleSolverPatternText, TextThatIsNoPatternIsRejectedAndChangesNothing) {
    WordleSolver solver = tinyGame();
    for (const std::string_view text : {"", "XXXX", "XXXXXX", "XXXXQ", "GY XG"}) {
        EXPECT_EQ(solver.addFeedbackPattern("slate", text), Status::InvalidPattern) << text;
    }
    EXPECT_TRUE(solver.getVeteranWords().empty());
    EXPECT_EQ(solver.getRemainingCount(), 2u);
}

TEST(WordleSolverPatternText, ChecksInTheSameOrderAsThePatternVersion) {
    WordleSolver solver = tinyGame();
    EXPECT_EQ(solver.addFeedbackPattern("cr4ne", "nonsense"), Status::InvalidWord);  // word first
    ASSERT_EQ(solver.addFeedbackPattern("crane", "GGGGG"), Status::Ok);
    EXPECT_EQ(solver.addFeedbackPattern("cr4ne", "nonsense"), Status::GameOver);  // then nothing
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
    EXPECT_EQ(solver.setStrategy(std::unique_ptr<ScoringStrategy>{}), Status::UnknownStrategy);
    EXPECT_EQ(solver.setStrategy(std::make_unique<FixedStrategy>()), Status::Ok);
    EXPECT_EQ(solver.getVeteranWords().size(), 1u);
    EXPECT_EQ(solver.getRemainingCount(), 1u);
}

// Eight -lls words: b or f, then a, e, i or o. The probe aeoxx splits them into
// four pairs by the vowel (worst group 2, 2 bits); bxaex splits off b from f as
// well, except ?ills from ?olls (groups 1, 1, 2, 1, 1, 2: also worst 2, but 2.5
// bits). Minimax ties and takes aeoxx alphabetically; entropy takes bxaex.
// Every -lls word scores worse for both (worst group 3, 1.81 bits).
WordleSolver vowelGame() {
    static const WordLists lists = *WordLists::fromWords(
        {"balls", "bells", "bills", "bolls", "falls", "fells", "fills", "folls"},
        {"aeoxx", "balls", "bells", "bills", "bolls", "bxaex", "falls", "fells", "fills", "folls"});
    return WordleSolver(std::make_unique<MinimaxStrategy>(), false, lists);
}

TEST(WordleSolver, StrategyIsChosenByItsExactName) {
    WordleSolver solver = vowelGame();
    EXPECT_EQ(solver.setStrategy("knuth"), Status::UnknownStrategy);
    EXPECT_EQ(solver.setStrategy("Minimax "), Status::UnknownStrategy);
    EXPECT_EQ(solver.getBestGuess(), "aeoxx");  // still minimax
    EXPECT_EQ(solver.setStrategy("entropy"), Status::Ok);
    EXPECT_EQ(solver.getBestGuess(), "bxaex");
    EXPECT_EQ(solver.setStrategy("minimax"), Status::Ok);
    EXPECT_EQ(solver.getBestGuess(), "aeoxx");
}

TEST(WordleSolver, SwitchingStrategyMidGameKeepsTheGame) {
    WordleSolver solver = vowelGame();
    // No o anywhere: bolls and folls are out, 6 words remain.
    ASSERT_EQ(solver.addFeedbackPattern("xxxxo", pattern("XXXXX")), Status::Ok);
    ASSERT_EQ(solver.getRemainingCount(), 6u);
    ASSERT_EQ(solver.setStrategy("entropy"), Status::Ok);
    ASSERT_EQ(solver.getVeteranWords().size(), 1u);
    EXPECT_EQ(solver.getVeteranWords()[0].word, "xxxxo");
    EXPECT_EQ(solver.getRemainingCount(), 6u);
    EXPECT_FALSE(solver.isUsingFallback());
    EXPECT_EQ(solver.getBestGuess(), "bxaex");
    // The game goes on from where it was: bxaex's colors for bells leave only bells.
    ASSERT_EQ(solver.addFeedbackPattern("bxaex", pattern("GXXYX")), Status::Ok);
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

TEST(WordleSolverFirstGuess, IsTheStoredOneForTheCurrentStrategy) {
    // Ranking would open with crane (FixedStrategy ties everything; rule 1 picks
    // a candidate), so slate can only come from the stored value.
    static const WordLists lists = *WordLists::fromWords(
        {"crane", "crate"}, {"crane", "crate", "grate", "slate"}, {{"fixed", "slate"}});
    WordleSolver solver(std::make_unique<FixedStrategy>(), false, lists);
    EXPECT_EQ(solver.getBestGuess(), "slate");
    // Only the first guess is stored: after one guess, ranking takes over again.
    ASSERT_EQ(solver.addFeedbackPattern("grate", pattern("XGGGG")), Status::Ok);
    EXPECT_EQ(solver.getBestGuess(), "crate");
}

TEST(WordleSolverFirstGuess, IsComputedWhenNoneIsStoredForTheStrategy) {
    static const WordLists lists = *WordLists::fromWords(
        {"crane", "crate"}, {"crane", "crate", "grate", "slate"}, {{"minimax", "slate"}});
    WordleSolver solver(std::make_unique<FixedStrategy>(), false, lists);
    EXPECT_EQ(solver.getBestGuess(), "crane");
}

TEST(WordleSolverFirstGuess, OnTheBuiltinListsInEitherMode) {
    // Shared spec: the same in hard mode, which has nothing to restrict before
    // the first guess.
    for (const bool hardMode : {false, true}) {
        WordleSolver solver(std::make_unique<MinimaxStrategy>(), hardMode);
        EXPECT_EQ(solver.getBestGuess(), "olate") << "hard mode " << hardMode;
        ASSERT_EQ(solver.setStrategy("entropy"), Status::Ok);
        EXPECT_EQ(solver.getBestGuess(), "tarse") << "hard mode " << hardMode;
    }
}

// The -ills words plus fhkmx, which splits f, h, k and m apart but breaks
// hard mode once ?ills is green.
WordLists illsLists(const std::vector<std::string_view>& answers) {
    return *WordLists::fromWords(answers, {"bills", "fhkmx", "fills", "hills", "kills", "mills"});
}

TEST(WordleSolverHardMode, OnlySuggestsGuessesHardModeAllows) {
    const WordLists lists = illsLists({"bills", "fills", "hills", "kills", "mills"});
    WordleSolver normal(std::make_unique<MinimaxStrategy>(), false, lists);
    WordleSolver hard(std::make_unique<MinimaxStrategy>(), true, lists);
    for (WordleSolver* solver : {&normal, &hard}) {
        ASSERT_EQ(solver->addFeedbackPattern("bills", pattern("XGGGG")), Status::Ok);
    }
    EXPECT_EQ(normal.getBestGuess(), "fhkmx");
    // Hard mode needs ?ills: only the candidates are left, all tied, f first.
    EXPECT_EQ(hard.getBestGuess(), "fills");
}

TEST(WordleSolverHardMode, SwitchingMidGameChangesThePoolAndKeepsTheGame) {
    // Starts in normal mode, so turning hard mode on has to build its pool then.
    WordleSolver solver(std::make_unique<MinimaxStrategy>(), false,
                        illsLists({"bills", "fills", "hills", "kills", "mills"}));
    ASSERT_EQ(solver.addFeedbackPattern("bills", pattern("XGGGG")), Status::Ok);
    EXPECT_EQ(solver.getBestGuess(), "fhkmx");
    solver.setHardMode(true);
    EXPECT_EQ(solver.getBestGuess(), "fills");
    solver.setHardMode(false);  // off again: every guess is back
    EXPECT_EQ(solver.getBestGuess(), "fhkmx");
    EXPECT_EQ(solver.getVeteranWords().size(), 1u);
    EXPECT_EQ(solver.getRemainingCount(), 4u);
}

TEST(WordleSolverHardMode, ResetKeepsHardModeOn) {
    // Shared spec: reset() clears the game but keeps the strategy and the hard-mode setting.
    WordleSolver solver(std::make_unique<MinimaxStrategy>(), true,
                        illsLists({"bills", "fills", "hills", "kills", "mills"}));
    ASSERT_EQ(solver.addFeedbackPattern("bills", pattern("XGGGG")), Status::Ok);
    solver.reset();
    ASSERT_EQ(solver.addFeedbackPattern("bills", pattern("XGGGG")), Status::Ok);
    EXPECT_EQ(solver.getBestGuess(), "fills");  // fhkmx if reset had turned hard mode off
}

TEST(WordleSolverHardMode, WorksTogetherWithFallback) {
    // bills is the only answer, so after XGGGG the candidates come from the guess list.
    WordleSolver solver(std::make_unique<MinimaxStrategy>(), true, illsLists({"bills"}));
    ASSERT_EQ(solver.addFeedbackPattern("bills", pattern("XGGGG")), Status::Ok);
    EXPECT_TRUE(solver.isUsingFallback());
    EXPECT_EQ(solver.getRemainingCount(), 4u);
    EXPECT_EQ(solver.getBestGuess(), "fills");
    solver.setHardMode(false);
    EXPECT_EQ(solver.getBestGuess(), "fhkmx");
}

TEST(WordleSolver, BuiltinListsByDefault) {
    const WordleSolver solver(std::make_unique<FixedStrategy>(), false);
    EXPECT_EQ(solver.getRemainingCount(), 2'309u);
}

}  // namespace
}  // namespace nudge
