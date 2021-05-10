#include "main.hpp"

#include <string>
#include <set>
#include <memory>
#include <filesystem>

#include <iostream>

#include <uv.h>

#include "server.hpp"
#include "manager.hpp"
#include "settings.hpp"

namespace fs = std::filesystem;

using std::string;
using std::set;
using std::shared_ptr;
using std::make_shared;

using std::cout;
using std::cerr;
using std::endl;

Manager *manager = nullptr;
set<shared_ptr<SSLServer>> servers;

void accept(SSLClient *client) {
    manager->handle(client);
}

int main(int argc, char *argv[]) {
#ifndef WIN32
    // prevent sigpipe from killing the server
    signal(SIGPIPE, SIG_IGN);
#endif
    // TODO: deal with missing config file
    string config = "conf.yml";
    if (argc > 1) {
        config = argv[1];
    }
	fs::path confpath = config;
	if (confpath.is_relative()) {
		confpath = fs::current_path() / confpath;
	}
	config = confpath.string();
    LOG_DEBUG("config: " << config);

    GemCapSettings settings;
    try {
        settings.loadFile(config);
    } catch (YAML::BadFile &err) {
        LOG_ERROR("Could not find config file: " << config);
        return 1;
    } catch (std::exception &err) {
        return 1;
    }
    uv_loop_t *loop = uv_default_loop();

    manager = new Manager(loop, settings.getCacheSize(), settings.getTimeout());

    try {
        manager->load(settings.getCapsules());
    } catch (fs::filesystem_error &err) {
        LOG_ERROR("Could not load capsules: " << err.path1() << " does not exist.");
        return 1;
    }

    wolfSSL_Init();

    for (auto conf : manager->getServers()) {
        if (conf.cert.empty()) {
			conf.cert = settings.getCert();
		}
        if (conf.key.empty()) {
            conf.key = settings.getKey();
        }
        if (conf.listen.empty()) {
            conf.listen = settings.getListen();
        }
        if (conf.port == 0) {
            conf.port = settings.getPort();
        }
        shared_ptr<SSLServer> server = make_shared<SSLServer>();
        if (server->load(loop, conf.listen, conf.port, conf.cert, conf.key)) {
            server->setAcceptCallback(accept);
            servers.insert(server);
            LOG_INFO("Listening on " << conf.listen << ":" << conf.port);
        }
    }

    if (servers.empty()) {
        LOG_WARN("Warning: There are no capsules configured!");
    }

    int ret = uv_run(uv_default_loop(), UV_RUN_DEFAULT);

    servers.clear();
    uv_loop_close(uv_default_loop());
    wolfSSL_Cleanup();
    delete manager;
    return ret;
}