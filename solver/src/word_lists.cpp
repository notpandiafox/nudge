#include "nudge/word_lists.hpp"

#include <algorithm>
#include <stdexcept>

namespace nudge {
namespace {

// Written by scripts/import_word_lists.py; the files hold one "word", per line.
// NOLINTBEGIN(modernize-avoid-c-arrays): the array size comes from the file.
constexpr std::string_view kAnswers[] = {
#include "answers.inc"
};
constexpr std::string_view kGuesses[] = {
#include "guesses.inc"
};
// NOLINTEND(modernize-avoid-c-arrays)

// The words, parsed and sorted; nothing if one isn't a word or one repeats.
std::optional<std::vector<Word>> sortedUniqueWords(const std::vector<std::string_view>& texts) {
    std::vector<Word> words;
    words.reserve(texts.size());
    for (const std::string_view text : texts) {
        const auto word = Word::parse(text);
        if (!word) {
            return std::nullopt;
        }
        words.push_back(*word);
    }
    std::sort(words.begin(), words.end());
    if (std::adjacent_find(words.begin(), words.end()) != words.end()) {
        return std::nullopt;
    }
    return words;
}

}  // namespace

const WordLists& WordLists::builtin() {
    static const WordLists lists = [] {
        auto built = fromWords({std::begin(kAnswers), std::end(kAnswers)},
                               {std::begin(kGuesses), std::end(kGuesses)});
        if (!built) {
            // GCOVR_EXCL_START: unreachable with the checked data files; a test can't make it run.
            throw std::logic_error(
                "data/*.inc failed the checks; rerun scripts/import_word_lists.py");
            // GCOVR_EXCL_STOP
        }
        return std::move(*built);
    }();
    return lists;
}

std::optional<WordLists> WordLists::fromWords(const std::vector<std::string_view>& answers,
                                              const std::vector<std::string_view>& guesses) {
    auto guessWords = sortedUniqueWords(guesses);
    auto answerWords = sortedUniqueWords(answers);
    // Both lists are sorted with no repeats, so includes() checks every answer is a guess.
    if (!guessWords || !answerWords || answerWords->empty() ||
        !std::includes(guessWords->begin(), guessWords->end(), answerWords->begin(),
                       answerWords->end())) {
        return std::nullopt;
    }
    return WordLists(std::move(*guessWords), std::move(*answerWords));
}

}  // namespace nudge
