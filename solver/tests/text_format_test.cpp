#include "nudge/text_format.hpp"

#include <gtest/gtest.h>

namespace nudge {
namespace {

TEST(StatusToText, NamesEveryStatus) {
    EXPECT_EQ(statusToText(Status::Ok), "Ok");
    EXPECT_EQ(statusToText(Status::InvalidWord), "InvalidWord");
    EXPECT_EQ(statusToText(Status::InvalidPattern), "InvalidPattern");
    EXPECT_EQ(statusToText(Status::ImpossibleState), "ImpossibleState");
    EXPECT_EQ(statusToText(Status::UnknownStrategy), "UnknownStrategy");
    EXPECT_EQ(statusToText(Status::GameOver), "GameOver");
}

TEST(StatusToText, ValueOutsideTheEnumIsUnknown) {
    // Status has a fixed underlying type (int), so any int converts to a valid
    // Status value. One that matches no enumerator must not crash the caller.
    EXPECT_EQ(statusToText(static_cast<Status>(99)), "Unknown");
}

}  // namespace
}  // namespace nudge
