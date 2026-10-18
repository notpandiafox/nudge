#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "nudge/word.hpp"

namespace nudge {

// The precomputed first guess of one strategy, by its name() (plan §5.11).
struct FirstGuess {
    std::string_view strategy;
    std::string_view guess;
};

// The answer list and the guess list the solver plays with (plan §5.1, §5.11).
class WordLists {
public:
    // The lists compiled in from data/answers.inc and data/guesses.inc.
    static const WordLists& builtin();

    // For tests: sorts the words and checks them. Nothing if any word isn't 5
    // letters, a list has a duplicate, an answer isn't a guess, there are no
    // answers, or a first guess isn't a guess.
    static std::optional<WordLists> fromWords(const std::vector<std::string_view>& answers,
                                              const std::vector<std::string_view>& guesses,
                                              const std::vector<FirstGuess>& firstGuesses = {});

    // Every accepted guess, sorted alphabetically.
    const std::vector<Word>& guesses() const { return guesses_; }
    // The possible answers, sorted alphabetically. Every answer is also a guess.
    const std::vector<Word>& answers() const { return answers_; }
    // The stored first guess for a strategy name; nothing if none is stored.
    std::optional<Word> firstGuess(std::string_view strategy) const;

private:
    WordLists(std::vector<Word> guesses, std::vector<Word> answers,
              std::vector<std::pair<std::string, Word>> firstGuesses)
        : guesses_(std::move(guesses)),
          answers_(std::move(answers)),
          firstGuesses_(std::move(firstGuesses)) {}

    std::vector<Word> guesses_;
    std::vector<Word> answers_;
    std::vector<std::pair<std::string, Word>> firstGuesses_;  // strategy name -> first guess
};

}  // namespace nudge
