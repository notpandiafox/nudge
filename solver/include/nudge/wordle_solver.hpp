#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "nudge/feedback.hpp"
#include "nudge/scoring_strategy.hpp"
#include "nudge/status.hpp"
#include "nudge/word.hpp"
#include "nudge/word_lists.hpp"

namespace nudge {

// A guess the player has already entered, with the colors it got.
struct VeteranWord {
    std::string word;  // lowercase
    FeedbackPattern pattern;
};

// Tracks one Wordle game (plan §5.5). It stores only the veteran words and the
// strategy and the mode; everything else is recomputed from them.
class WordleSolver {
public:
    // The lists are copied, so a temporary WordLists is safe to pass.
    WordleSolver(std::unique_ptr<ScoringStrategy> strategy, bool hardMode,
                 WordLists lists = WordLists::builtin());

    Status addFeedbackPattern(const std::string& word, const FeedbackPattern& pattern);
    // The same, with the pattern as text ("GYXXG", either case), the way JavaScript sends it.
    Status addFeedbackPattern(const std::string& word, std::string_view patternText);
    // The suggested next guess, lowercase. Once solved, the winning word.
    std::string getBestGuess() const;

    std::size_t getRemainingCount() const;
    const std::vector<VeteranWord>& getVeteranWords() const;
    bool isUsingFallback() const;
    Status setStrategy(std::unique_ptr<ScoringStrategy> strategy);
    Status setStrategy(std::string_view name);  // "minimax" or "entropy" (makeStrategy)
    // Hard mode suggests only guesses it allows (rule B, plan §5.12). Keeps the game.
    void setHardMode(bool on);
    void reset();

private:
    // What the veteran words imply. Rebuilt by derive() after every change.
    struct View {
        bool solved = false;
        bool fallback = false;
        std::vector<Word> candidates;    // sorted
        std::uint32_t triedLetters = 0;  // Word::mask() of every veteran word, combined
    };
    View derive(std::span<const VeteranWord> veterans) const;
    // Both addFeedbackPattern versions: nothing for a pattern that isn't one.
    Status add(const std::string& word, const std::optional<FeedbackPattern>& pattern);

    WordLists lists_;
    std::unique_ptr<ScoringStrategy> strategy_;
    bool hardMode_;
    std::vector<VeteranWord> veterans_;
    View view_;
};

}  // namespace nudge
