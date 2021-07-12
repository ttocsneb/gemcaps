#ifndef __GEMCAPS_SHARED_UTIL__
#define __GEMCAPS_SHARED_UTIL__

#include <stddef.h>
#include <memory>

#include <vector>
#include <parallel_hashmap/phmap.h>

/**
 * A char buffer that acts like a pipe or queue of bytes.
 */
class BufferPipe {
private:
    char *buffer;
    size_t size;
    size_t length = 0;
    size_t start = 0;
public:
    /**
     * Create a BufferPipe that has no starting buffer
     */
    BufferPipe()
        : buffer(nullptr),
          size(0) {}
    /**
     * Create a BufferPipe that starts with a buffer
     * 
     * @param size the size of the starting buffer
     */
    BufferPipe(size_t size)
        : buffer(new char[size]),
          size(size) {}
    
    ~BufferPipe();

    /**
     * Get the number of ready bytes in the buffer
     * 
     * @return the number of ready bytes
     */
    size_t ready() const noexcept { return length; }
    /**
     * Peek at the full buffer.
     * 
     * The size of the buffer can be found by ready()
     * 
     * @return the current buffer
     * 
     * @warning The returned buffer becomes void when either write() or read() is called
     */
    const char *peek() const noexcept { return buffer + start; }

    /**
     * Write to the buffer.
     * 
     * @param buf the bytes to write
     * @param len the number of bytes to write
     */
    void write(const char *buf, size_t len) noexcept;
    /**
     * Read from the buffer.
     * 
     * @param len the number of bytes to read
     * @param buf the buffer to read into
     * 
     * @return the number of read bytes
     */
    size_t read(size_t len, char *buf) noexcept;
};

/**
 * An Allocator that will reuse previously allocated items.
 */
template <class T, size_t alloc_size = 10, class Allocator = std::allocator<T> >
class ReusableAllocator {
private:
    T stack[alloc_size];

    phmap::flat_hash_set<T *> in_use;
    std::vector<T *> avail;
    std::vector<T *> heap;

    Allocator allocator;
public:
    ReusableAllocator()
            : avail(alloc_size) {
        for (size_t i = 0; i < alloc_size; ++i) {
            avail.push_back(&stack[i]);
        }
    }
    
    ~ReusableAllocator() {
        for (T * item : heap) {
            allocator.deallocate(item, alloc_size);
        }
    }

    /**
     * allocate a new item for use
     * 
     * @note The returned item may or may not have been already used before
     * 
     * @return an allocated item
     */
    T *allocate() noexcept {
        if (!avail.empty()) {
            T *item = avail.back();
            avail.pop_back();
            in_use.insert(item);
            return item;
        }
        T *new_items = allocator.allocate(alloc_size);
        heap.push_back(new_items);
        for (size_t i = 0; i < alloc_size; ++i) {
            avail.push_back(&new_items[i]);
        }
        return allocate();
    }

    /**
     * deallocate a previously allocated item.
     * 
     * @note the item is not freed from memory, but may be re used in future calls to allocate()
     * 
     * @param item item to deallocate
     */
    void deallocate(T *item) noexcept {
        auto found = in_use.find(item);
        if (found == in_use.end()) {
            // The item is not being used, and so should not be freed
            return;
        }
        in_use.erase(found);
        avail.push_back(item);
    }
};

#endif