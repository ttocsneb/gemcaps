#include "main.hpp"

#include <string>
#include <set>
#include <memory>

#include <iostream>

#include <uv.h>

#include "server.hpp"
#include "manager.hpp"
#include "settings.hpp"

using std::string;
using std::set;
using std::shared_ptr;
using std::make_shared;

using std::cout;
using std::cerr;
using std::endl;

Manager manager;
set<shared_ptr<SSLServer>> servers;

void delete_handle(uv_handle_t *handle) {
    delete handle;
}

void accept(SSLClient *client, void *ctx) {
    WOLFSSL *ssl = client->getSSL();

    int ret = wolfSSL_accept_TLSv13(ssl);
    // I don't know what to do if accept gets a would-block error

    delete client;
}

int main(int argc, char *argv[]) {
    // TODO: deal with missing config file
    string config = "conf.yml";
    if (argc > 1) {
        config = argv[1];
    }

    GemCapSettings settings;
    settings.loadFile(config);
    manager.load(settings.getCapsules());

    wolfSSL_Init();
    uv_loop_t *loop = uv_default_loop();

    for (auto conf : manager.getServers()) {
        shared_ptr<SSLServer> server = make_shared<SSLServer>();
        if (server->load(loop, conf.host, conf.port, conf.cert, conf.key)) {
            server->setAcceptCallback(accept);
            servers.insert(server);
        }
    }

    int ret = uv_run(uv_default_loop(), UV_RUN_DEFAULT);

    servers.clear();
    uv_loop_close(uv_default_loop());
    wolfSSL_Cleanup();
    return ret;
}