#include "nudge/text_format.hpp"

#include <gtest/gtest.h>

#include "tile_printer.hpp"

namespace nudge {
namespace {

constexpr Tile G = Tile::Green;
constexpr Tile Y = Tile::Yellow;
constexpr Tile X = Tile::Gray;

TEST(StatusToText, NamesEveryStatus) {
    EXPECT_EQ(statusToText(Status::Ok), "Ok");
    EXPECT_EQ(statusToText(Status::InvalidWord), "InvalidWord");
    EXPECT_EQ(statusToText(Status::InvalidPattern), "InvalidPattern");
    EXPECT_EQ(statusToText(Status::ImpossibleState), "ImpossibleState");
    EXPECT_EQ(statusToText(Status::UnknownStrategy), "UnknownStrategy");
    EXPECT_EQ(statusToText(Status::GameOver), "GameOver");
}

TEST(StatusToText, ValueOutsideTheEnumIsUnknown) {
    // Status has a fixed underlying type (std::uint8_t), so any value of that type
    // is a valid Status. One that matches no enumerator must not crash the caller.
    EXPECT_EQ(statusToText(static_cast<Status>(99)), "Unknown");
}

TEST(ParsePattern, ReadsGreenYellowGrayInEitherCase) {
    EXPECT_EQ(parsePattern("GYXXG"), (FeedbackPattern{G, Y, X, X, G}));
    EXPECT_EQ(parsePattern("gyxxg"), (FeedbackPattern{G, Y, X, X, G}));
}

TEST(ParsePattern, RejectsWrongLengthOrUnknownLetters) {
    EXPECT_FALSE(parsePattern(""));
    EXPECT_FALSE(parsePattern("GYXX"));
    EXPECT_FALSE(parsePattern("GYXXGG"));
    EXPECT_FALSE(parsePattern("GYXQG"));
    EXPECT_FALSE(parsePattern("GY XG"));
}

}  // namespace
}  // namespace nudge
