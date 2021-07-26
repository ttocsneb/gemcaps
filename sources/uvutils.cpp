#include "gemcaps/uvutils.hpp"

const size_t BUFFER_SIZE = 1024;

union Buffer {
    char buffer[BUFFER_SIZE];
};

ReusableAllocator<Buffer> char_buffers;

uv_buf_t buffer_allocate() noexcept {
    uv_buf_t buf;
    buf.len = BUFFER_SIZE;
    buf.base = reinterpret_cast<char *>(char_buffers.allocate());
    return buf;
}

void buffer_deallocate(uv_buf_t buf) noexcept {
    char_buffers.deallocate(reinterpret_cast<Buffer *>(buf.base));
}
