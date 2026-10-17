#pragma once

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "nudge/word.hpp"

namespace nudge {

// The answer list and the guess list the solver plays with (plan §5.1, §5.11).
class WordLists {
public:
    // The lists compiled in from data/answers.inc and data/guesses.inc.
    static const WordLists& builtin();

    // For tests: sorts the words and checks them. Nothing if any word isn't 5
    // letters, a list has a duplicate, an answer isn't a guess, or there are no
    // answers.
    static std::optional<WordLists> fromWords(const std::vector<std::string_view>& answers,
                                              const std::vector<std::string_view>& guesses);

    // Every accepted guess, sorted alphabetically.
    const std::vector<Word>& guesses() const { return guesses_; }
    // The possible answers, sorted alphabetically. Every answer is also a guess.
    const std::vector<Word>& answers() const { return answers_; }

private:
    WordLists(std::vector<Word> guesses, std::vector<Word> answers)
        : guesses_(std::move(guesses)), answers_(std::move(answers)) {}

    std::vector<Word> guesses_;
    std::vector<Word> answers_;
};

}  // namespace nudge
