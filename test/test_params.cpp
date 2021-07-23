#include <gtest/gtest.h>

#include "params.hpp"


TEST(params, args) {
    ArgParse args;
    args.addArg("test");
    args.addArg("test2");

    const char *arguments[] = {"foo", "bar"};

    auto result = args.parseArgs(arguments, 2);

    ASSERT_EQ(result.at("test"), "foo");
    ASSERT_EQ(result.at("test2"), "bar");
    ASSERT_EQ(result.size(), 2);
}

TEST(params, params) {
    ArgParse args;
    args.addParam("test");
    args.addParam("test2", "t2");

    const char *arguments[] = {"--test", "bar", "-t2", "foo"};

    auto result = args.parseArgs(arguments, 4);

    ASSERT_EQ(result.at("test"), "bar");
    ASSERT_EQ(result.at("test2"), "foo");
    ASSERT_EQ(result.size(), 2);
}

TEST(params, paramsArgs) {
    ArgParse args;
    args.addArg("cheese");
    args.addParam("test");
    args.addParam("test2", "t2");

    const char *arguments[] = {"--test", "bar", "yeet", "-t2", "foo"};

    auto result = args.parseArgs(arguments, 5);

    ASSERT_EQ(result.at("test"), "bar");
    ASSERT_EQ(result.at("test2"), "foo");
    ASSERT_EQ(result.at("cheese"), "yeet");
    ASSERT_EQ(result.size(), 3);
}