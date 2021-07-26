#include <gtest/gtest.h>

#include <set>

#include <gemcaps/util.hpp>

using std::set;


TEST(util, buffer_readwrite) {
    BufferPipe pipe;
    ASSERT_EQ(pipe.ready(), 0);

    const char *message = "Hello World!";
    pipe.write(message, strlen(message));

    ASSERT_EQ(pipe.ready(), strlen(message));
    char buf[64];
    size_t read = pipe.read(63, buf);
    ASSERT_EQ(read, strlen(message));
    buf[read] = '\0';
    ASSERT_STREQ(message, buf);
}

TEST(util, buffer_close) {
    BufferPipe pipe;

    ASSERT_FALSE(pipe.is_closed());
    ASSERT_EQ(pipe.ready(), 0);

    pipe.close();
    ASSERT_TRUE(pipe.is_closed());
    ASSERT_EQ(pipe.ready(), 0);

    const char *message = "Hello World!";
    pipe.write(message, strlen(message));
    ASSERT_EQ(pipe.ready(), 0);
}

TEST(util, buffer_preclose) {
    BufferPipe pipe;

    const char *message = "Hello World!";
    pipe.write(message, strlen(message));

    ASSERT_FALSE(pipe.is_closed());
    ASSERT_EQ(pipe.ready(), strlen(message));

    pipe.close();
    ASSERT_TRUE(pipe.is_closed());
    ASSERT_EQ(pipe.ready(), strlen(message));

    char buf[64];
    size_t read = pipe.read(63, buf);
    ASSERT_EQ(read, strlen(message));
    ASSERT_EQ(pipe.ready(), 0);

    buf[read] = '\0';
    ASSERT_STREQ(message, buf);
}

TEST(util, alloc_reuse) {
    constexpr const int n = 10;
    ReusableAllocator<int, n> alloc;

    set<int *> pointers;

    // Load the stack pointers
    for (int i = 0; i < n; ++i) {
        pointers.insert(alloc.allocate());
    }
    for (int *p : pointers) {
        alloc.deallocate(p);
    }

    // Assert that all of the new allocated pointers are reused stack pointers
    for (int i = 0; i < n; ++i) {
        ASSERT_TRUE(pointers.count(alloc.allocate()));
    }
    for (int *p : pointers) {
        alloc.deallocate(p);
    }
}

TEST(util, alloc_heap) {
    constexpr const int n = 10;
    ReusableAllocator<int, n> alloc;

    set<int *> stack;
    set<int *> heap;

    // Load the stack pointers
    for (int i = 0; i < n; ++i) {
        stack.insert(alloc.allocate());
    }
    // Load the heap pointers
    for (int i = 0; i < n; ++i) {
        heap.insert(alloc.allocate());
    }

    // Assert that the stack pointer is reused
    int *p = *stack.begin();
    alloc.deallocate(p);
    ASSERT_EQ(alloc.allocate(), p);

    // Assert that the heap pointer is reused
    int *h = *heap.begin();
    alloc.deallocate(h);
    ASSERT_EQ(alloc.allocate(), h);

    // Assert that the stack pointer is preferred
    alloc.deallocate(p);
    alloc.deallocate(h);
    ASSERT_EQ(alloc.allocate(), p);
    ASSERT_EQ(alloc.allocate(), h);

    for (int *p : stack) {
        alloc.deallocate(p);
    }
    for (int *p : heap) {
        alloc.deallocate(p);
    }
}