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
#include "gemcaps/util.hpp"

namespace fs = std::filesystem;

using std::string;
using std::set;
using std::shared_ptr;
using std::make_shared;

using std::cout;
using std::cerr;
using std::endl;


int main(int argc, char *argv[]) {
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