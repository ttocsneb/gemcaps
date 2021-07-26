#include "gemcaps/pathutils.hpp"

#include <regex>
#include <sstream>

using std::string;
using std::vector;
using std::regex;
using std::ostringstream;

string path::join(string root, string path) noexcept {
    if (!path.empty() && (path.front() == '/' || path.front() == '\\')) {
        path = path.substr(1);
    }

    if (!root.empty() && (root.back() == '/' || root.back() == '\\')) {
        return root + path;
    }
    return root + '/' + path;
}

string path::join(vector<string> paths, string join) noexcept {
    if (paths.empty()) {
        return "";
    }
    ostringstream oss;
    for (auto it = paths.begin(); it != paths.end() - 1; ++it) {
        oss << *it << join;
    }
    oss << paths.back();
    return oss.str();
}

string path::basename(string path) noexcept {
    while (path.back() == '/' || path.back() == '\\') {
        path.pop_back();
    }

    size_t found_fwd = path.rfind('/');
    size_t found_bck = path.rfind('\\');
    // The path is only one element
    if (found_fwd == string::npos && found_bck == string::npos) {
        return path;
    }

    if (found_fwd != string::npos) {
        return path.substr(found_fwd + 1);
    }
    if (found_bck != string::npos) {
        return path.substr(found_bck + 1);
    }

    if (found_fwd > found_bck) {
        return path.substr(found_fwd + 1);
    }
    return path.substr(found_bck + 1);
}

string path::dirname(string path) noexcept {
    while (path.back() == '/' || path.back() == '\\') {
        path.pop_back();
    }

    size_t found_fwd = path.rfind('/');
    size_t found_bck = path.rfind('\\');
    // The path is only one element
    if (found_fwd == string::npos && found_bck == string::npos) {
        return "";
    }

    if (found_fwd == string::npos) {
        return path.substr(0, found_bck + 1);
    }
    if (found_bck == string::npos) {
        return path.substr(0, found_fwd + 1);
    }

    if (found_fwd > found_bck) {
        return path.substr(0, found_fwd + 1);
    }
    return path.substr(0, found_bck + 1);
}

vector<string> path::split(string path) noexcept {
    vector<string> sections;

    size_t start = 0;

    while (start != path.length()) {
        size_t end_fwd = path.find('/', start);
        size_t end_bck = path.find('\\', start);

        if (end_fwd == string::npos && end_bck == string::npos) {
            sections.push_back(path.substr(start));
            return sections;
        }
        if (end_fwd == string::npos) {
            sections.push_back(path.substr(start, end_bck - start));
            start = end_bck + 1;
            continue;
        }
        if (end_bck == string::npos) {
            sections.push_back(path.substr(start, end_fwd - start));
            start = end_fwd + 1;
            continue;
        }

        if (end_fwd < end_bck) {
            sections.push_back(path.substr(start, end_fwd - start));
            start = end_fwd + 1;
            continue;
        }
        sections.push_back(path.substr(start, end_bck - start));
        start = end_bck + 1;
    }
    return sections;
}

bool path::isSubpath(string path, string subpath) noexcept {
    vector<string> path_parts = split(path);
    vector<string> subpath_parts = split(subpath);

    while (!path_parts.empty() && !subpath_parts.empty()) {
        if (path_parts.front() != subpath_parts.front()) {
            return false;
        }
        path_parts.erase(path_parts.begin());
        subpath_parts.erase(subpath_parts.begin());
    }
    return true;
}

string path::relpath(string path, string rel) noexcept {
    vector<string> path_parts = split(path);
    vector<string> rel_parts = split(rel);

    while (!path_parts.empty() && !rel_parts.empty()) {
        if (path_parts.front() != rel_parts.front()) {
            return path;
        }
        path_parts.erase(path_parts.begin());
        rel_parts.erase(rel_parts.begin());
    }
    return join(path_parts);
}

string path::delUps(string path) noexcept {
    vector<string> parts;

    for (string part : split(path)) {
        if (part == ".") {
            continue;
        }
        if (part == "..") {
            if (!parts.empty()) {
                parts.pop_back();
            }
            continue;
        }
        if (part.empty() && !parts.empty()) {
            continue;
        }
        parts.push_back(part);
    }

    return join(parts);
}

bool path::isrel(string path) noexcept {
    if (path.front() == '/' || path.front() == '\\') {
        return false;
    }
#ifdef WIN32
    if (isalpha(path.front()) && path.substr(1).rfind(":\\", 0) != string::npos) {
        return false;
    }
#endif
    return true;
}