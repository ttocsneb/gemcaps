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

Manager manager;
set<shared_ptr<SSLServer>> servers;
set<SSLClient*> clients;

void delete_handle(uv_handle_t *handle) {
    delete handle;
}

void close(SSLClient *client, void *ctx) {
    clients.erase(client);
    delete client;
}

void receive(SSLClient *client, void *ctx) {
    WOLFSSL *ssl = client->getSSL();
    while (client->hasData()) {

        char header[1024];
        int read = wolfSSL_read(ssl, header, 1024);
        if (read < 0) {
            if (wolfSSL_want_read(ssl)) {
                return;
            }
            int err = wolfSSL_get_error(ssl, 0);
            char buffer[80];
            wolfSSL_ERR_error_string(err, buffer);
            cerr << "wolfSSL error: " << buffer << endl;
            continue;
        }
        string data(header, read);

        cout << "Read data: (" << read << ") '" << data << "'" << endl;

        client->setWriteReadyCallback(close);
        string msg = "20 text/gemini\nHello World!\n";
        read = wolfSSL_write(ssl, msg.c_str(), msg.length());
    }
}

void accept(SSLClient *client, void *ctx) {
    client->setReadReadyCallback(receive, ctx);
    clients.insert(client);
}

int main(int argc, char *argv[]) {
    // TODO: deal with missing config file
    string config = "conf.yml";
    if (argc > 1) {
        config = argv[1];
    }

    GemCapSettings settings;
    try {
        settings.loadFile(config);
    } catch (YAML::BadFile &err) {
        cerr << "Could not find config file: " << config << endl;
        return 1;
    } catch (std::exception &err) {
        return 1;
    }

    try {
        manager.load(settings.getCapsules());
    } catch (fs::filesystem_error &err) {
        cerr << "Could not load capsules: " << err.path1() << " does not exist." << endl;
        return 1;
    } 

    wolfSSL_Init();
    uv_loop_t *loop = uv_default_loop();

    for (auto conf : manager.getServers()) {
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
            cout << "Listening on " << conf.listen << ":" << conf.port << endl;
        }
    }

    if (servers.empty()) {
        cerr << "Warning: There are no capsules configured!" << endl;
    }

    int ret = uv_run(uv_default_loop(), UV_RUN_DEFAULT);

    for (SSLClient *client : clients) {
        delete client;
    }
    servers.clear();
    uv_loop_close(uv_default_loop());
    wolfSSL_Cleanup();
    return ret;
}