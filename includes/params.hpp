#ifndef __GEMCAPS_PARAMS__
#define __GEMCAPS_PARAMS__

#include <string>
#include <vector>

#include <parallel_hashmap/phmap.h>

class ArgParse {
private:
    std::vector<std::string> args;
    phmap::flat_hash_set<std::string> params;
    phmap::flat_hash_map<std::string, std::string> short_params;
public:
    void addArg(std::string arg) noexcept { args.push_back(arg); }
    void addParam(std::string param) noexcept { params.insert(param); }
    void addParam(std::string param, std::string short_param) noexcept {
        addParam(param);
        short_params.insert({short_param, param});
    }

    phmap::flat_hash_map<std::string, std::string> parseArgs(const char **args, int nargs);
    phmap::flat_hash_map<std::string, std::string> parseArgs(std::vector<std::string> args);
};

#endif