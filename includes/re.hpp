#ifndef __GEMCAPS_RE__
#define __GEMCAPS_RE__

#include <regex>
#include <string>

class Regex {
private:
    std::regex regex;
    std::string pattern;
public:
    Regex()
        : regex(".*", std::regex::ECMAScript) {}
    Regex(std::string pattern, std::regex::flag_type flags = std::regex::ECMAScript)
        : regex(pattern, flags),
          pattern(pattern) {}

    const std::regex &getRegex() const { return regex; }
    const std::string &getPattern() const { return pattern; }

    bool find(const std::string &s, std::regex_constants::match_flag_type flags = std::regex_constants::match_default) const;
    bool match(const std::string &s, std::regex_constants::match_flag_type flags = std::regex_constants::match_default) const;

    operator std::string() { return pattern; }
    operator std::regex() { return regex; }
};

#endif