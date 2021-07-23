#ifndef __GEMCAPS_SHARED_STRINGUTIL__
#define __GEMCAPS_SHARED_STRINGUTIL__

#include <cstddef>
#include <iostream>

/**
 * A String Literal Builder. This can be used to generate string literals at compile time.
 */
template<size_t Size>
struct StringLiteral {
    char buf[Size + 1] = {'\0'};

    /**
     * Get the length of the string
     * 
     * @return the length of the string
     */
    constexpr size_t length() const noexcept {
        size_t l = 0;
        while (buf[l] != '\0') ++l;
        return l;
    }

    /**
     * Append another string to the string
     * 
     * @param text string to append
     */
    constexpr void append(const char *text) noexcept {
        size_t start = length();
        size_t i = 0;
        while (text[i] != '\0') {
            buf[start + i] = text[i];
            ++i;
        }
        buf[start + i] = '\0';
    }

    template<size_t s>
    constexpr void append(StringLiteral<s> literal) noexcept {
        append(literal.buf);
    }

    /**
     * Append an integer value to the string
     * 
     * @param value value to append
     */
    constexpr void append(int value) noexcept {
        // Convert the number into a reversed char buffer
        char valbuf[10] = {'\0'};
        int i = 0;
        bool negative = value < 0;
        if (negative) {
            value = -value;
        }
        do {
            valbuf[i++] = '0' + (value % 10);
            value = value / 10;
        } while(value != 0);

        // Add the negative sign
        if (negative) {
            valbuf[i] = '-';
        } else {
            --i;
        }

        // Append the char buffer in reverse to put it in the proper order
        size_t start = length();
        size_t pos = 0;
        while (i >= 0) {
            buf[start + pos++] = valbuf[i--];
        }
        buf[start + pos] = '\0';
    }

};

template<size_t Size>
std::ostream &operator<<(std::ostream &os, const StringLiteral<Size> rhs) {
    return os << rhs.buf;
}

#endif