#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "nudge/word.hpp"

namespace nudge {

// The answer list and the guess list the solver plays with (plan §5.1, §5.11).
class WordLists {
public:
    // A word's position in guesses(). 16 bits, so a guess list holds at most 65,536 words.
    using Index = std::uint16_t;

    // The lists compiled in from data/answers.inc and data/guesses.inc.
    static const WordLists& builtin();

    // For tests: sorts the words and checks them. Nothing if any word isn't 5
    // letters, a list has a duplicate, an answer isn't a guess, there are no
    // answers, or there are too many guesses for an Index.
    static std::optional<WordLists> fromWords(const std::vector<std::string_view>& answers,
                                              const std::vector<std::string_view>& guesses);

    // Every accepted guess, sorted alphabetically.
    const std::vector<Word>& guesses() const { return guesses_; }
    // The possible answers, as sorted positions in guesses().
    const std::vector<Index>& answers() const { return answers_; }

private:
    WordLists(std::vector<Word> guesses, std::vector<Index> answers)
        : guesses_(std::move(guesses)), answers_(std::move(answers)) {}

    std::vector<Word> guesses_;
    std::vector<Index> answers_;
};

}  // namespace nudge
