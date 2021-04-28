#include "glob.hpp"

#include <sstream>

using std::string;
using std::ostringstream;

void Glob::compile(string glob) {
    pattern.clear();
    while (!glob.empty()) {
        size_t pos = glob.find('*');
        if (pos != string::npos) {
            pattern.push_back(glob.substr(0, pos));
            glob = glob.substr(pos + 1);
            continue;
        }
        break;
    }
    pattern.push_back(glob);
}

bool Glob::match(string str) const {
    // Assert that the string starts with the first pattern
    if (!pattern.front().empty()) {
        size_t pos = str.rfind(pattern.front(), 0);
        if (pos == string::npos) {
            return false;
        }
        str = str.substr(pos + pattern.front().length());
    }

    for (auto it = pattern.begin() + 1; it != pattern.end(); ++it) {
        if (it->empty()) {
            continue;
        }
        size_t pos = str.find(*it);
        if (pos == string::npos) {
            return false;
        }
        str = str.substr(pos + it->length());
    }

    // Assert that the string ends with the last pattern
    if (!pattern.back().empty()) {
        if (!str.empty()) {
            return false;
        }
    }
    return true;
}

string Glob::str() const {
    ostringstream oss;

    for (auto it = pattern.begin(); it + 1 != pattern.end(); ++it) {
        oss << *it << '*';
    }
    oss << pattern.back();

    return oss.str();
}