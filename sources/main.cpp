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
#include "gemcaps/pathutils.hpp"

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
    parser.addParam("log", "l");
    parser.addParam("colors");
    parser.addParam("verbose", "v");
    parser.addParam("config", "c");

    phmap::flat_hash_map<string, string> args;    
    try {
        args = parser.parseArgs(argv + 1, argc - 1);
    } catch (std::exception &e) {
        LOG_ERROR("Invalid arguments: " << color::getColor(color::RED) << e.what() << color::reset);
        return 1;
    }

    if (args.count("log")) {
        string mode = args.at("log");
        if (mode.front() == 'd') {
            logging::set_mode(logging::DEBUG);
        } else if (mode.front() == 'i') {
            logging::set_mode(logging::INFO);
        } else if (mode.front() == 'w') {
            logging::set_mode(logging::WARN);
        } else if (mode.front() == 'e') {
            logging::set_mode(logging::ERROR);
        } else if (mode.front() == 'n') {
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

#ifndef WIN32
    // prevent sigpipe from killing the server
    signal(SIGPIPE, SIG_IGN);
#endif

    wolfSSL_Init();

    char cwd[1024];
    size_t cwd_size = 1024;
    uv_cwd(cwd, &cwd_size);

    string config = cwd;
    if (args.count("config")) {
        string conf_path = args.at("config");
        if (path::isrel(conf_path)) {
            config = path::join(config, conf_path);
        } else {
            config = conf_path;
        }
    }

    Manager manager;
    manager.loadServers(path::join(config, "servers"));
    manager.loadHandlers(path::join(config, "handlers"));
    manager.startServers();

    int ret = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    uv_loop_close(uv_default_loop());
    wolfSSL_Cleanup();
    return ret;
}