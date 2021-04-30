#include <gtest/gtest.h>

#include "manager.hpp"

TEST(GeminiRequest, SimpleRequest) {
    GeminiRequest request("gemini://foo.bar\r\n");

    ASSERT_TRUE(request.isValid());
    ASSERT_EQ("gemini", request.getSchema());
    ASSERT_EQ("foo.bar", request.getHost());
    ASSERT_EQ(0, request.getPort());
    ASSERT_EQ("", request.getPath());
    ASSERT_EQ("", request.getQuery());
    ASSERT_EQ("gemini://foo.bar", request.getRequest());
}

TEST(GeminiRequest, PortRequest) {
    GeminiRequest request("gemini://foo.bar:1234\r\n");

    ASSERT_TRUE(request.isValid());
    ASSERT_EQ("gemini", request.getSchema());
    ASSERT_EQ("foo.bar", request.getHost());
    ASSERT_EQ(1234, request.getPort());
    ASSERT_EQ("", request.getPath());
    ASSERT_EQ("", request.getQuery());
    ASSERT_EQ("gemini://foo.bar:1234", request.getRequest());
}

TEST(GeminiRequest, PathRequest) {
    GeminiRequest request("gemini://foo.bar/foo/baz/\r\n");

    ASSERT_TRUE(request.isValid());
    ASSERT_EQ("gemini", request.getSchema());
    ASSERT_EQ("foo.bar", request.getHost());
    ASSERT_EQ(0, request.getPort());
    ASSERT_EQ("/foo/baz/", request.getPath());
    ASSERT_EQ("", request.getQuery());
    ASSERT_EQ("gemini://foo.bar/foo/baz/", request.getRequest());
}

TEST(GeminiRequest, PathPortRequest) {
    GeminiRequest request("gemini://foo.bar:80/foo/baz/\r\n");

    ASSERT_TRUE(request.isValid());
    ASSERT_EQ("gemini", request.getSchema());
    ASSERT_EQ("foo.bar", request.getHost());
    ASSERT_EQ(80, request.getPort());
    ASSERT_EQ("/foo/baz/", request.getPath());
    ASSERT_EQ("", request.getQuery());
    ASSERT_EQ("gemini://foo.bar:80/foo/baz/", request.getRequest());
}

TEST(GeminiRequest, QueryRequest) {
    GeminiRequest request("gemini://foo.bar?help\r\n");

    ASSERT_TRUE(request.isValid());
    ASSERT_EQ("gemini", request.getSchema());
    ASSERT_EQ("foo.bar", request.getHost());
    ASSERT_EQ(0, request.getPort());
    ASSERT_EQ("", request.getPath());
    ASSERT_EQ("?help", request.getQuery());
    ASSERT_EQ("gemini://foo.bar?help", request.getRequest());
}

TEST(GeminiRequest, QueryPortRequest) {
    GeminiRequest request("gemini://foo.bar:1200?help\r\n");

    ASSERT_TRUE(request.isValid());
    ASSERT_EQ("gemini", request.getSchema());
    ASSERT_EQ("foo.bar", request.getHost());
    ASSERT_EQ(1200, request.getPort());
    ASSERT_EQ("", request.getPath());
    ASSERT_EQ("?help", request.getQuery());
    ASSERT_EQ("gemini://foo.bar:1200?help", request.getRequest());
}

TEST(GeminiRequest, PathQueryRequest) {
    GeminiRequest request("gemini://foo.bar/cheese/?help\r\n");

    ASSERT_TRUE(request.isValid());
    ASSERT_EQ("gemini", request.getSchema());
    ASSERT_EQ("foo.bar", request.getHost());
    ASSERT_EQ(0, request.getPort());
    ASSERT_EQ("/cheese/", request.getPath());
    ASSERT_EQ("?help", request.getQuery());
    ASSERT_EQ("gemini://foo.bar/cheese/?help", request.getRequest());
}

TEST(GeminiRequest, PathQueryPortRequest) {
    GeminiRequest request("gemini://foo.bar:1965/cheese/?help\r\n");

    ASSERT_TRUE(request.isValid());
    ASSERT_EQ("gemini", request.getSchema());
    ASSERT_EQ("foo.bar", request.getHost());
    ASSERT_EQ(1965, request.getPort());
    ASSERT_EQ("/cheese/", request.getPath());
    ASSERT_EQ("?help", request.getQuery());
    ASSERT_EQ("gemini://foo.bar:1965/cheese/?help", request.getRequest());
}

TEST(GeminiRequest, BadSchema) {
    GeminiRequest request("GEMINI://foo.bar:1965/cheese/?help\r\n");
    ASSERT_TRUE(request.isValid());

    request = GeminiRequest("foobar://foo.bar/cheese/?help\r\n");
    ASSERT_FALSE(request.isValid());

    request = GeminiRequest("http://foo.bar/cheese/?help\r\n");
    ASSERT_FALSE(request.isValid());
}

TEST(GeminiRequest, BadSchemaSeparator) {
    GeminiRequest request("gemini:///foo.bar:1965/cheese/?help\r\n");
    ASSERT_FALSE(request.isValid());

    request = GeminiRequest("gemini:///foo.bar/cheese/?help\r\n");
    ASSERT_FALSE(request.isValid());

    request = GeminiRequest("gemini:///foo.bar?help\r\n");
    ASSERT_FALSE(request.isValid());

    request = GeminiRequest("gemini:///foo.bar\r\n");
    ASSERT_FALSE(request.isValid());

    request = GeminiRequest("gemini:/foo.bar:1965/cheese/?help\r\n");
    ASSERT_FALSE(request.isValid());

    request = GeminiRequest("gemini:/foo.bar/cheese/?help\r\n");
    ASSERT_FALSE(request.isValid());

    request = GeminiRequest("gemini:/foo.bar?help\r\n");
    ASSERT_FALSE(request.isValid());

    request = GeminiRequest("gemini:/foo.bar\r\n");
    ASSERT_FALSE(request.isValid());
}

TEST(GeminiRequest, LeadingSpaces) {
    GeminiRequest request("  gemini://foo.bar:1965/cheese/?help\r\n");

    ASSERT_TRUE(request.isValid());
    ASSERT_EQ("gemini", request.getSchema());
    ASSERT_EQ("foo.bar", request.getHost());
    ASSERT_EQ(1965, request.getPort());
    ASSERT_EQ("/cheese/", request.getPath());
    ASSERT_EQ("?help", request.getQuery());
    ASSERT_EQ("gemini://foo.bar:1965/cheese/?help", request.getRequest());
}

TEST(GeminiRequest, TrailingSpaces) {
    GeminiRequest request("gemini://foo.bar:1965/cheese/?help  \r\n");

    ASSERT_TRUE(request.isValid());
    ASSERT_EQ("gemini", request.getSchema());
    ASSERT_EQ("foo.bar", request.getHost());
    ASSERT_EQ(1965, request.getPort());
    ASSERT_EQ("/cheese/", request.getPath());
    ASSERT_EQ("?help", request.getQuery());
    ASSERT_EQ("gemini://foo.bar:1965/cheese/?help", request.getRequest());
}