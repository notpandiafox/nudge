#include <array>
#include <stdexcept>

#include "feedback_code.hpp"

namespace nudge {
namespace {

constexpr std::array<PatternCode, 5> kPlace{1, 3, 9, 27, 81};  // what position i is worth: 3^i

FeedbackPattern decode(PatternCode code) {
    FeedbackPattern tiles{};
    for (Tile& tile : tiles) {
        tile = static_cast<Tile>(code % 3);
        code = static_cast<PatternCode>(code / 3);
    }
    return tiles;
}

}  // namespace

PatternCode encode(const FeedbackPattern& pattern) {
    PatternCode code = 0;
    for (std::size_t i = 5; i-- > 0;) {
        code = static_cast<PatternCode>(code * 3 + static_cast<PatternCode>(pattern[i]));
    }
    return code;
}

// The shared spec's rule: Greens first, then Yellows left to right, each using
// up one copy of that letter from the answer's non-Green positions. It builds
// the code directly, without tiles: this is where nearly all of a hint's time
// goes (plan §5.9).
PatternCode feedbackCode(const Word& guess, const Word& answer) {
    PatternCode code = 0;
    std::array<bool, 5> green{};
    std::array<std::uint8_t, 26> unused{};
    for (std::size_t i = 0; i < 5; ++i) {
        if (guess.letters()[i] == answer.letters()[i]) {
            green[i] = true;
            code = static_cast<PatternCode>(code + 2 * kPlace[i]);
        } else {
            ++unused[answer.letters()[i]];
        }
    }
    for (std::size_t i = 0; i < 5; ++i) {
        if (!green[i] && unused[guess.letters()[i]] > 0) {
            --unused[guess.letters()[i]];
            code = static_cast<PatternCode>(code + kPlace[i]);
        }
    }
    return code;
}

FeedbackPattern computeFeedback(const std::string& guess, const std::string& answer) {
    const auto guessWord = Word::parse(guess);
    const auto answerWord = Word::parse(answer);
    if (!guessWord || !answerWord) {
        // A caller bug, not a game state, so not a Status. In WebAssembly this aborts.
        throw std::invalid_argument("computeFeedback needs two 5-letter words");
    }
    return decode(feedbackCode(*guessWord, *answerWord));  // one implementation of the rule
}

}  // namespace nudge
