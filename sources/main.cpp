#include "main.hpp"

#include <string>
#include <set>
#include <memory>
#include <filesystem>

#include <iostream>

#include <uv.h>

#include "server.hpp"
#include "manager.hpp"
#include "gemcaps/settings.hpp"
#include "gemcaps/log.hpp"
#include "params.hpp"

namespace fs = std::filesystem;

using std::string;
using std::set;
using std::shared_ptr;
using std::make_shared;

using std::cout;
using std::cerr;
using std::endl;


int main(int argc, const char **argv) {
    ArgParse parser;
    parser.addParam("mode", "m");
    parser.addParam("colors", "c");
    parser.addParam("verbose", "v");

    phmap::flat_hash_map<string, string> args;    
    try {
        args = parser.parseArgs(argv + 1, argc - 1);
    } catch (std::exception &e) {
        LOG_ERROR("Invalid arguments: " << color::getColor(color::RED) << e.what() << color::reset);
        return 1;
    }

    if (args.count("mode")) {
        string mode = args.at("mode");
        if (mode == "debug") {
            logging::set_mode(logging::DEBUG);
        } else if (mode == "info") {
            logging::set_mode(logging::INFO);
        } else if (mode == "warn") {
            logging::set_mode(logging::WARN);
        } else if (mode == "error") {
            logging::set_mode(logging::ERROR);
        } else if (mode == "none") {
            logging::set_mode(logging::NONE);
        }
    }

    if (args.count("colors")) {
        string colors = args.at("colors");
        if (colors.front() == 'y') {
            logging::enable_colors(true);
        } else if (colors.front() == 'n') {
            logging::enable_colors(false);
        }
    }

    if (args.count("verbose")) {
        string verbose = args.at("verbose");
        if (verbose.front() == 'y') {
            logging::set_verbose(true);
        } else if (verbose.front() == 'n') {
            logging::set_verbose(false);
        }
    }

    LOG_DEBUG("This is a debug message");
    LOG_INFO("Hello World");
    LOG_WARN("Warning Warning");
    LOG_ERROR("SOMETHIGN BAD HAPPENDED!");
    return 1;

#ifndef WIN32
    // prevent sigpipe from killing the server
    signal(SIGPIPE, SIG_IGN);
#endif

    wolfSSL_Init();

    Manager manager;
    manager.loadServers("servers"); // TODO use propper config file
    manager.loadHandlers("handlers"); // TODO use propper config file
    manager.startServers();

    int ret = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    uv_loop_close(uv_default_loop());
    wolfSSL_Cleanup();
    return ret;
}