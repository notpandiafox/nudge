#include "nudge/text_format.hpp"

#include "nudge/entropy_strategy.hpp"
#include "nudge/minimax_strategy.hpp"

namespace nudge {

std::string_view statusToText(Status status) {
    switch (status) {
        case Status::Ok:
            return "Ok";
        case Status::InvalidWord:
            return "InvalidWord";
        case Status::InvalidPattern:
            return "InvalidPattern";
        case Status::ImpossibleState:
            return "ImpossibleState";
        case Status::UnknownStrategy:
            return "UnknownStrategy";
        case Status::GameOver:
            return "GameOver";
    }
    return "Unknown";
}

std::optional<FeedbackPattern> parsePattern(std::string_view text) {
    if (text.size() != 5) {
        return std::nullopt;
    }
    FeedbackPattern pattern{};
    for (std::size_t i = 0; i < 5; ++i) {
        switch (text[i]) {
            case 'G':
            case 'g':
                pattern[i] = Tile::Green;
                break;
            case 'Y':
            case 'y':
                pattern[i] = Tile::Yellow;
                break;
            case 'X':
            case 'x':
                pattern[i] = Tile::Gray;
                break;
            default:
                return std::nullopt;
        }
    }
    return pattern;
}

std::unique_ptr<ScoringStrategy> makeStrategy(std::string_view name) {
    if (name == "minimax") {
        return std::make_unique<MinimaxStrategy>();
    }
    if (name == "entropy") {
        return std::make_unique<EntropyStrategy>();
    }
    return nullptr;
}

}  // namespace nudge
