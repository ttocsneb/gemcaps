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
set<SSLClient*> clients;

void onClientClose(SSLClient *client) {
    clients.erase(client);
    ClientContext *context = static_cast<ClientContext *>(client->getContext());
    if (context) {
        delete context;
    }
    delete client;
}

void close(SSLClient *client) {
    clients.erase(client);
    delete client;
}

void receive(SSLClient *client) {
    char header[1024];
    int read = client->read(header, sizeof(header));
    if (read < 0) {
        if (client->wants_read()) {
            return;
        }
        LOG_ERROR("WOLFSSL - " << client->get_error_string());
        return;
    }
    string data(header, read);
    ClientContext *context = static_cast<ClientContext *>(client->getContext());
    if (context) {
        context->getBuffer() += string(header, read);
        // Close the connection if the header is too big
        if (context->getBuffer().length() > 1024) {
            client->close();
            return;
        }
        // The client hasn't sent all of the data
        if (context->getBuffer().find('\n') == string::npos) {
            return;
        }
        GeminiRequest request(context->getBuffer());
        if (request.isValid()) {
            manager->handle(client, request);
            return;
        }
    }
    client->close();
}

void accept(SSLClient *client) {
    LOG_DEBUG("Got new client");
    ClientContext *context = new ClientContext();
    // TODO: make a timout timer
    client->setContext(context);
    client->setReadReadyCallback(receive);
    client->setCloseCallback(onClientClose);
    clients.insert(client);
    client->setTimeout(1000);
}

int main(int argc, char *argv[]) {
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

    manager = new Manager(loop, settings.getCacheSize());

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

    for (SSLClient *client : clients) {
        delete client;
    }
    servers.clear();
    uv_loop_close(uv_default_loop());
    wolfSSL_Cleanup();
    delete manager;
    return ret;
}