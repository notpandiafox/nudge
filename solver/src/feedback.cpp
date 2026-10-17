#include <stdexcept>

#include "feedback_code.hpp"

namespace nudge {
namespace {

// The shared spec's rule: Greens first, then Yellows left to right, each using
// up one copy of that letter from the answer's non-Green positions.
FeedbackPattern feedbackTiles(const Word& guess, const Word& answer) {
    FeedbackPattern tiles{};
    std::array<std::uint8_t, 26> unused{};
    for (std::size_t i = 0; i < 5; ++i) {
        if (guess.letters()[i] == answer.letters()[i]) {
            tiles[i] = Tile::Green;
        } else {
            ++unused[answer.letters()[i]];
        }
    }
    for (std::size_t i = 0; i < 5; ++i) {
        if (tiles[i] != Tile::Green && unused[guess.letters()[i]] > 0) {
            tiles[i] = Tile::Yellow;
            --unused[guess.letters()[i]];
        }
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

PatternCode feedbackCode(const Word& guess, const Word& answer) {
    return encode(feedbackTiles(guess, answer));
}

FeedbackPattern computeFeedback(const std::string& guess, const std::string& answer) {
    const auto guessWord = Word::parse(guess);
    const auto answerWord = Word::parse(answer);
    if (!guessWord || !answerWord) {
        // A caller bug, not a game state, so not a Status. In WebAssembly this aborts.
        throw std::invalid_argument("computeFeedback needs two 5-letter words");
    }
    return feedbackTiles(*guessWord, *answerWord);
}

}  // namespace nudge
