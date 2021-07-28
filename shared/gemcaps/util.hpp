#ifndef __GEMCAPS_SHARED_UTIL__
#define __GEMCAPS_SHARED_UTIL__

#include <stddef.h>
#include <memory>

#include <vector>
#include <parallel_hashmap/phmap.h>

inline const char *SOFTWARE = "GemCaps-0.3.2-alpha";

class IBufferPipe;

class IBufferPipeObserver {
public:
    virtual void on_buffer_write(IBufferPipe *buffer) = 0;
};

/**
 * A read-only Buffer Pipe
 */
class IBufferPipe {
private:
    IBufferPipeObserver *observer = nullptr;
    void *context = nullptr;
protected:
    /**
     * Notify that IBufferPipe has new data to read
     */
    void notify() {
        if (observer) {
            observer->on_buffer_write(this);
        }
    }
public:
    /**
     * Read from the buffer
     * 
     * @param len size of the buffer to read into
     * @param buf buffer to read into
     * 
     * @return the number of bytes actually read
     */
    virtual size_t read(size_t len, char *buf) = 0;
    /**
     * Get the number of bytes ready to read
     * 
     * @return the number of ready bytes
     */
    virtual size_t ready() const = 0;
    /**
     * Check if the buffer is closed.
     * 
     * @note you can still read from the buffer if there is available data, 
     *     but no more data will be written to it.
     * 
     * @return whether the buffer is closed
     */
    virtual bool is_closed() const = 0;

    /**
     * Set an observer callback to be notified when new data is available, or the buffer is closed.
     * 
     * @param observer observer to call when changes are made
     */
    void setObserver(IBufferPipeObserver *observer) noexcept { this->observer = observer; }
    /**
     * Set the context of the buffer
     * 
     * @param context context
     */
    void setContext(void *context) noexcept { this->context = context; }

    /**
     * Get the context of the buffer
     * 
     * @return the context
     */
    void *getContext() const noexcept { return context; }
};

/**
 * A write-only BufferPipe
 */
class OBufferPipe {
public:
    /**
     * Write to the buffer
     * 
     * @note writing to a closed buffer has no effect
     * 
     * @param buf buffer to write
     * @param len size of the buffer
     */
    virtual void write(const char *buf, size_t len) = 0;
    /**
     * Close the buffer
     * 
     * This prevents future writes from succeeding, and lets the reader
     * no that no more wrights will be made.
     */
    virtual void close() = 0;
};

/**
 * A char buffer that acts like a pipe or queue of bytes.
 */
class BufferPipe : public IBufferPipe, public OBufferPipe {
private:
    char *buffer;
    size_t size;
    size_t length = 0;
    size_t start = 0;
    bool closed = false;
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

    size_t ready() const noexcept { return length; }
    bool is_closed() const noexcept { return closed; }

    void write(const char *buf, size_t len) noexcept;
    size_t read(size_t len, char *buf) noexcept;
    void close() noexcept { closed = true; }
};

/**
 * An Allocator that will reuse previously allocated items.
 */
template <class T, size_t alloc_size = 10, class Allocator = std::allocator<T> >
class ReusableAllocator {
private:
    T stack[alloc_size];

    phmap::flat_hash_map<T *, std::vector<T *>> heap_avail;
    std::vector<T *> stack_avail;

    phmap::flat_hash_set<T *> stack_in_use;
    phmap::flat_hash_map<T *, T *> heap_in_use;

    Allocator allocator;
public:
    ReusableAllocator() {
        for (T &item : stack) {
            stack_avail.push_back(&item);
        }
    }
    
    ~ReusableAllocator() {
        // De-allocate all allocated heap chunks
        for (auto pair : heap_avail) {
            allocator.deallocate(pair.first, alloc_size);
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
        if (!stack_avail.empty()) {
            T *item = stack_avail.back();
            stack_avail.pop_back();
            stack_in_use.insert(item);
            return item;
        }
        for (auto &pair : heap_avail) {
            if (!pair.second.empty()) {
                T* item = pair.second.back();
                pair.second.pop_back();
                heap_in_use.insert({item, pair.first});
                return item;
            }
        }
        T *new_items = allocator.allocate(alloc_size);
        std::vector<T *> new_avail;
        for (int i = 0; i < alloc_size; ++i) {
            new_avail.push_back(&new_items[i]);
        }
        heap_avail.insert({new_items, new_avail});
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
        auto stack_found = stack_in_use.find(item);
        if (stack_found != stack_in_use.end()) {
            stack_avail.push_back(*stack_found);
            stack_in_use.erase(stack_found);
            return;
        }
        auto heap_found = heap_in_use.find(item);
        if (heap_found != heap_in_use.end()) {
            heap_avail[heap_found->second].push_back(heap_found->first);

            // If the heap chunk is not used, de-allocate it
            if (heap_avail[heap_found->second].size() == alloc_size) {
                allocator.deallocate(heap_found->second, alloc_size);
                heap_avail.erase(heap_found->second);
            }

            heap_in_use.erase(heap_found);
            return;
        }
    }
};

#endif