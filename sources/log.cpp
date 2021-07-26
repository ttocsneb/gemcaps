#include "gemcaps/log.hpp"

using namespace logging;

using std::ostream;

bool colors_enabled = true;
bool verbose_enabled = false;
Mode current_mode = INFO;

ostream empty(0);

void logging::enable_colors(bool enable) {
    colors_enabled = enable;
}

void logging::set_mode(Mode mode) {
    current_mode = mode;
}

void logging::set_verbose(bool enable) {
    verbose_enabled = enable;
}

Mode logging::get_mode() {
    return current_mode;
}

bool logging::is_enabled(Mode mode) {
    return current_mode >= mode;
}

constexpr StringLiteral<32> color_tag(const char *tag, color::Color color) {
    StringLiteral<32> t;
    t.append(color::getColor(color));
    t.append(tag);
    t.append(color::reset);
    return t;
}

void print_verbose(ostream &os, const char *file, int line) {
    os << "[";
    if (colors_enabled) {
        constexpr const auto gray = color::getColor(color::BLACK, true);
        os << gray;
    }

    os << file << ":" << line;

    if (colors_enabled) {
        os << color::reset;
    }

    os << "] ";
}

void print_tag(ostream &os, const char *tag, const char *colortag, const char *file, int line) {
    if (colors_enabled) {
        os << colortag;
    } else {
        os << tag;
    }
    os << " ";
    if (verbose_enabled) print_verbose(os, file, line);
}

ostream &logging::debug(const char *file, int line) {
    if (!is_enabled(DEBUG)) {
        return empty;
    }
    constexpr const char *tag = "DEBUG";
    constexpr auto colortag = color_tag(tag, color::MAGENTA);
    print_tag(std::cout, tag, colortag.buf, file, line);
    return std::cout;
}

ostream &logging::warn(const char *file, int line) {
    if (!is_enabled(WARN)) {
        return empty;
    }
    constexpr const char *tag = "WARNI";
    constexpr auto colortag = color_tag(tag, color::YELLOW);
    print_tag(std::cout, tag, colortag.buf, file, line);
    return std::cout;
}

ostream &logging::info(const char *file, int line) {
    if (!is_enabled(INFO)) {
        return empty;
    }
    constexpr const char *tag = "INFO ";
    constexpr auto colortag = color_tag(tag, color::WHITE);
    print_tag(std::cout, tag, colortag.buf, file, line);
    return std::cout;
}

ostream &logging::error(const char *file, int line) {
    if (!is_enabled(ERROR)) {
        return empty;
    }
    constexpr const char *tag = "ERROR";
    constexpr auto colortag = color_tag(tag, color::RED);
    print_tag(std::cout, tag, colortag.buf, file, line);
    return std::cout;
}