#include "re.hpp"

using std::string;
using std::regex;
using std::regex_match;
using std::regex_search;
using std::regex_constants::match_flag_type;

bool Regex::find(const string &s, match_flag_type flags) const {
    return regex_search(s, regex, flags);
}

bool Regex::match(const string &s, match_flag_type flags) const {
    return regex_match(s, regex, flags);
}