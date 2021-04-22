#ifndef __GEMCAPS_GLOB__
#define __GEMCAPS_GLOB__

#include <string>
#include <vector>

/**
 * A glob pattern
 * 
 * match strings using glob patterns such as `foo*`
 */
class Glob {
private:
    std::vector<std::string> pattern;

    void compile(std::string glob);
public:
    Glob() {}
    Glob(const std::string &glob) { compile(glob); }
    Glob(const Glob &other) 
        : pattern(other.pattern) {}

    bool match(std::string str) const;
    std::string str() const;

    Glob &operator=(const std::string &glob) { compile(glob); return *this; }

    bool operator==(const std::string &other) const { return match(other); }
    bool operator==(const Glob &other) const { return pattern == other.pattern; }

    operator std::string() const { return str(); }
};

#endif