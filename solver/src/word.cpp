#include "nudge/word.hpp"

namespace nudge {

std::optional<Word> Word::parse(std::string_view text) {
    if (text.size() != 5) {
        return std::nullopt;
    }
    Word word{};
    for (std::size_t i = 0; i < 5; ++i) {
        const char c = text[i];
        if (c >= 'a' && c <= 'z') {
            word.letters_[i] = static_cast<std::uint8_t>(c - 'a');
        } else if (c >= 'A' && c <= 'Z') {
            word.letters_[i] = static_cast<std::uint8_t>(c - 'A');
        } else {
            return std::nullopt;
        }
    }
    return word;
}

}  // namespace nudge
