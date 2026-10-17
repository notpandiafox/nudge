#include "nudge/text_format.hpp"

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

}  // namespace nudge
