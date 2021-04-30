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

void onClientClose(SSLClient *client, void *ctx) {
    clients.erase(client);
    ClientContext *context = static_cast<ClientContext *>(ctx);
    if (context) {
        delete context;
    }
    delete client;
}

void close(SSLClient *client, void *ctx) {
    clients.erase(client);
    delete client;
}

void receive(SSLClient *client, void *ctx) {
    char header[1024];
    int read = client->read(header, sizeof(header));
    if (read < 0) {
        if (client->wants_read()) {
            return;
        }
        cerr << "wolfSSL error: " << client->get_error_string() << endl;
        return;
    }
    string data(header, read);
    ClientContext *context = static_cast<ClientContext *>(ctx);
    if (context) {
        context->buffer += string(header, read);
        // Close the connection if the header is too big
        if (context->buffer.length() > 1024) {
            client->close();
            return;
        }
        // The client hasn't sent all of the data
        if (context->buffer.find('\n') == string::npos) {
            return;
        }
        GeminiRequest request(context->buffer);
        if (request.isValid()) {
            manager.handle(client, request);
            return;
        }
    }
    client->close();
}

void accept(SSLClient *client, void *ctx) {
    ClientContext *context = new ClientContext({
        .buffer = ""
    });
    // TODO: make a timout timer
    client->setReadReadyCallback(receive, context);
    client->setCloseCallback(onClientClose, context);
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