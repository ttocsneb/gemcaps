#include <params.hpp>

#include <exception>

using std::string;
using std::vector;

phmap::flat_hash_map<string, string> ArgParse::parseArgs(vector<string> args) {
    phmap::flat_hash_map<string, string> result;
    int argPos = 0;
    while (!args.empty()) {
        string arg = args.front();
        if (arg.rfind("--", 0) != string::npos) {
            string tag = arg.substr(2);
            if (!params.count(tag)) {
                throw std::invalid_argument("Unknown parameter '" + arg + "'");
            }
            args.erase(args.begin());
            if (args.empty()) {
                throw std::invalid_argument("Missing value after '" + arg + "'");
            }
            result.insert({tag, args.front()});
            args.erase(args.begin());
            continue;
        }
        if (arg.rfind("-", 0) != string::npos) {
            string tag = arg.substr(1);
            if (!short_params.count(tag)) {
                throw std::invalid_argument("Unknown parameter '" + arg + "'");
            }
            args.erase(args.begin());
            if (args.empty()) {
                throw std::invalid_argument("Missing value after '" + arg + "'");
            }
            result.insert({short_params.at(tag), args.front()});
            args.erase(args.begin());
            continue;
        }
        if (argPos >= this->args.size()) {
            throw std::invalid_argument("Too many arguments given");
        }
        string tag = this->args.at(argPos++);
        result.insert({tag, args.front()});
        args.erase(args.begin());
    }

    return result;
}

phmap::flat_hash_map<string, string> ArgParse::parseArgs(const char **args, int nargs) {
    vector<string> a;
    for (int i = 0; i < nargs; ++i) {
        a.push_back(args[i]);
    }
    return parseArgs(a);
}