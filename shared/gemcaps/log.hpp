#ifndef __GEMCAPS_SHARED_LOG__
#define __GEMCAPS_SHARED_LOG__

#include <iostream>

#include "gemcaps/stringutil.hpp"

#define LOG_DEBUG(x) logging::debug(__FILE__, __LINE__) << x << std::endl
#define LOG_INFO(x) logging::info(__FILE__, __LINE__) << x << std::endl
#define LOG_WARN(x) logging::warn(__FILE__, __LINE__) << x << std::endl
#define LOG_ERROR(x) logging::error(__FILE__, __LINE__) << x << std::endl

namespace color {

constexpr const char *COLOR_ESCAPE = "\u001b[";
enum Color {
    BLACK = 0,
    RED = 1,
    GREEN = 2,
    YELLOW = 3,
    BLUE = 4,
    MAGENTA = 5,
    CYAN = 6,
    WHITE = 7
};
enum Style {
    RESET = 0,
    BOLD = 1,
    UNDERLINE = 4,
    REVERSED = 7
};

constexpr StringLiteral<8> getColor(Color color, bool bold = false, bool background = false) {
    int modifier = background ? 40 : 30;
    StringLiteral<8> c;
    c.append(COLOR_ESCAPE);
    c.append(modifier + ((int) color));
    if (bold) {
        c.append(";1m");
    } else {
        c.append("m");
    }

    return c;
}

constexpr StringLiteral<8> getStyle(Style style) {
    StringLiteral<8> c;
    c.append(COLOR_ESCAPE);
    c.append((int) style);
    c.append("m");
    return c;
}

constexpr const auto reset = getStyle(RESET);
constexpr const auto bold = getStyle(BOLD);
constexpr const auto underline = getStyle(UNDERLINE);
constexpr const auto reversed = getStyle(REVERSED);

}


namespace logging {

#ifdef ERROR
#undef ERROR
#endif

enum Mode {
	NONE = 0,
	ERROR = 10,
	WARN = 20,
	INFO = 30,
	DEBUG = 40
};

/**
 * Enable/disable colors
 * 
 * @param enable whether to enable or disable colors
 */
void enable_colors(bool enable);
/**
 * Set the log mode
 * 
 * @param mode mode
 */
void set_mode(Mode mode);
/**
 * Enable/disable verbose mode
 * 
 * @param enable whether to enable or disable verbose mode
 */
void set_verbose(bool enable);

/**
 * Get the current log mode
 * 
 * @return mode
 */
Mode get_mode();

/**
 * Check if logging is enabled for the given mode
 * 
 * @param mode mode to check
 * 
 * @return whether logging is allowed
 */
bool is_enabled(Mode mode);

/**
 * Get the stream for debug messages
 * 
 * @param file filename
 * @param line line number
 * 
 * @return the stream to debug
 */
std::ostream &debug(const char *file, int line);
/**
 * Get the stream for warning messages
 * 
 * @param file filename
 * @param line line number
 * 
 * @return the stream to debug
 */
std::ostream &warn(const char *file, int line);
/**
 * Get the stream for info messages
 * 
 * @param file filename
 * @param line line number
 * 
 * @return the stream to debug
 */
std::ostream &info(const char *file, int line);
/**
 * Get the stream for error messages
 * 
 * @param file filename
 * @param line line number
 * 
 * @return the stream to debug
 */
std::ostream &error(const char *file, int line);

}

#endif