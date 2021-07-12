#include "util.hpp"

#include <string.h>


BufferPipe::~BufferPipe() {
    if (buffer != nullptr) {
        delete[] buffer;
    }
}

void BufferPipe::write(const char *buf, size_t len) noexcept {
    if (length + len > size) {
        // Resize the buffer
        char *newbuf = new char[length + len];
        memmove(newbuf, buffer + start, length);
        delete[] buffer;
        buffer = newbuf;
        start = 0;
    }

    if (start + length + len > size) {
        // Reset the position of the buffer
        memmove(buffer, buffer + start, length);
        start = 0;
    }

    // Write the buf to the buffer
    memcpy(buffer + start + length, buf, len);
    length += len;
}

size_t BufferPipe::read(size_t len, char *buf) noexcept {
    size_t read = len > length ? length : len; // The number of bytes to read

    memcpy(buf, buffer + start, read);
    start += read;
    return read;
}