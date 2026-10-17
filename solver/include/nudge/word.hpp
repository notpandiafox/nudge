#pragma once

#include <array>
#include <compare>
#include <cstdint>
#include <optional>
#include <string_view>

namespace nudge {

// A checked word: exactly 5 letters, stored as 0-25 (a = 0). parse() is the
// only way to make one, so nothing past that point has to check it again
// (plan §5.3).
class Word {
public:
    // Accepts upper or lower case; nothing for anything else.
    static std::optional<Word> parse(std::string_view text);

    const std::array<std::uint8_t, 5>& letters() const { return letters_; }

    // Compares letter by letter, so sorting Words sorts them alphabetically.
    auto operator<=>(const Word&) const = default;

private:
    Word() = default;
    std::array<std::uint8_t, 5> letters_{};
};

}  // namespace nudge
