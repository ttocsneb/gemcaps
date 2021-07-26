#include <gtest/gtest.h>

#include <gemcaps/stringutil.hpp>


constexpr StringLiteral<64> testNum(int val) {
    StringLiteral<64> lit;
    lit.append(val);
    return lit;
}

constexpr StringLiteral<64> testStr(const char *text) {
    StringLiteral<64> lit;
    lit.append("test ");
    lit.append(text);
    return lit;
}

TEST(stringutil, numbers) {
    ASSERT_STREQ(testNum(5).buf, "5");
    ASSERT_STREQ(testNum(50).buf, "50");
    ASSERT_STREQ(testNum(-50).buf, "-50");
    ASSERT_STREQ(testNum(0).buf, "0");
}

TEST(stringutil, append) {
    ASSERT_STREQ(testStr("hello").buf, "test hello");
}
