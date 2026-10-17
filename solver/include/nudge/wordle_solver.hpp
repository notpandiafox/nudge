#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <string>
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
// strategy; everything else is recomputed from them. (Hard mode arrives in T10.)
class WordleSolver {
public:
    // The lists are copied, so a temporary WordLists is safe to pass.
    WordleSolver(std::unique_ptr<ScoringStrategy> strategy, bool hardMode,
                 WordLists lists = WordLists::builtin());

    Status addFeedbackPattern(const std::string& word, const FeedbackPattern& pattern);

    std::size_t getRemainingCount() const;
    const std::vector<VeteranWord>& getVeteranWords() const;
    bool isUsingFallback() const;
    Status setStrategy(std::unique_ptr<ScoringStrategy> strategy);
    void reset();

private:
    // What the veteran words imply. Rebuilt by derive() after every change.
    struct View {
        bool solved = false;
        bool fallback = false;
        std::vector<Word> candidates;
    };
    View derive(std::span<const VeteranWord> veterans) const;

    WordLists lists_;
    std::unique_ptr<ScoringStrategy> strategy_;
    std::vector<VeteranWord> veterans_;
    View view_;
};

}  // namespace nudge
