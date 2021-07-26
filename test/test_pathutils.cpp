#include <gtest/gtest.h>

#include <vector>
#include <string>

#include <gemcaps/pathutils.hpp>

using std::vector;
using std::string;


TEST(pathutils, join) {
    ASSERT_EQ(path::join("asdf", "qwerty"), "asdf/qwerty");
    ASSERT_EQ(path::join("asdf/", "qwerty"), "asdf/qwerty");
    ASSERT_EQ(path::join("asdf", "/qwerty"), "asdf/qwerty");
    ASSERT_EQ(path::join("asdf/", "/qwerty"), "asdf/qwerty");
    ASSERT_EQ(path::join("asdf", "qwerty/"), "asdf/qwerty/");
    ASSERT_EQ(path::join("/asdf", "qwerty"), "/asdf/qwerty");
}

TEST(pathutils, join_many) {
    ASSERT_EQ(path::join({"foo", "bar"}), "foo/bar");
    ASSERT_EQ(path::join({"foo", "bar", "cheese"}), "foo/bar/cheese");
    ASSERT_EQ(path::join({"", "foo", "bar", "cheese"}), "/foo/bar/cheese");
    ASSERT_EQ(path::join({"foo", "bar", "cheese", ""}), "foo/bar/cheese/");
}

TEST(pathutils, basename) {
    ASSERT_EQ(path::basename("foo/bar/cheese"), "cheese");
    ASSERT_EQ(path::basename("foo/bar/cheese.txt"), "cheese.txt");
    ASSERT_EQ(path::basename("cheese.txt"), "cheese.txt");
    ASSERT_EQ(path::basename("/cheese.txt"), "cheese.txt");
}

TEST(pathutils, dirname) {
    ASSERT_EQ(path::dirname("foo/bar/cheese"), "foo/bar/");
    ASSERT_EQ(path::dirname("foo/cheese"), "foo/");
    ASSERT_EQ(path::dirname("/foo/cheese"), "/foo/");
    ASSERT_EQ(path::dirname("/cheese"), "/");
    ASSERT_EQ(path::dirname("cheese"), "");
}

TEST(pathutils, split) {
    ASSERT_EQ(path::split("foo/bar/cheese"), vector<string>({"foo", "bar", "cheese"}));
    ASSERT_EQ(path::split("/foo/bar/cheese"), vector<string>({"", "foo", "bar", "cheese"}));
    ASSERT_EQ(path::split("/foo/bar/cheese/"), vector<string>({"", "foo", "bar", "cheese"}));
    ASSERT_EQ(path::split("foo\\bar/cheese"), vector<string>({"foo", "bar", "cheese"}));
}

TEST(pathutils, relpath) {
    ASSERT_EQ(path::relpath("/foo/bar/cheese", "/foo/bar"), "cheese");
    ASSERT_EQ(path::relpath("/foo/bar/cheese", "/foo"), "bar/cheese");
    ASSERT_EQ(path::relpath("/foo/bar/cheese", "foo"), "/foo/bar/cheese");
    ASSERT_EQ(path::relpath("/foo/bar/cheese", "/"), "foo/bar/cheese");
    ASSERT_EQ(path::relpath("/foo/bar/cheese", "/foo/cheese"), "/foo/bar/cheese");
}

TEST(pathutils, delUps) {
    ASSERT_EQ(path::delUps("/foo/bar/bam/../cheese"), "/foo/bar/cheese");
    ASSERT_EQ(path::delUps("/foo/bar/bam/..asdf/cheese"), "/foo/bar/bam/..asdf/cheese");
    ASSERT_EQ(path::delUps("/foo/bar/bam/./cheese"), "/foo/bar/bam/cheese");
    ASSERT_EQ(path::delUps("/foo/bar/bam/.c/cheese"), "/foo/bar/bam/.c/cheese");
}