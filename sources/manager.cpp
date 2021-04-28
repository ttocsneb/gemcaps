#include "manager.hpp"

#include "filehandler.hpp"
#include "gsgihander.hpp"

#include <filesystem>
#include <iostream>

using std::string;
using std::shared_ptr;
using std::make_shared;
using std::dynamic_pointer_cast;

using std::cout;
using std::endl;

namespace fs = std::filesystem;

bool Manager::ServerSettings::operator<(const Manager::ServerSettings &rhs) const {
    if (port != rhs.port) {
        return port < rhs.port;
    }
    return listen < rhs.listen;
}

void Manager::load(const string &directory) {
    for (const auto &entry : fs::directory_iterator(directory)) {
        string ext = entry.path().extension();
        if (ext == ".yml" || ext == ".yaml") {
            YAML::Node node = YAML::LoadFile(entry.path().string());
            loadCapsule(node, entry.path().filename());
        }
    }
}

void Manager::loadCapsule(YAML::Node &node, const string &file) {
    CapsuleSettings settings;
    settings.load(node);

    const Glob &host = settings.getHost();
    int port = settings.getPort();
    const auto &confs = settings.getHandlers();

    ServerSettings conf;
    conf.listen = settings.getListen();
    conf.port = port;
    conf.key = settings.getKey();
    conf.cert = settings.getCert();

    if(!servers.insert(conf).second) {
        if (!conf.key.empty() || !conf.cert.empty()) {
            cout << "Warning: '" << file << "' tried to use a custom certificate for a host that has already been defined. This certificate will not be used!" << endl;
        }
    }

    // Iterate through each handler's settings
    for (auto conf : confs) {
        // Create the handler from the settings type
        shared_ptr<Handler> handler;
        string type = conf->getType();
        if (type == CapsuleSettings::TYPE_FILE) {
            handler = make_shared<FileHandler>(
                dynamic_pointer_cast<FileSettings>(conf), 
                host, 
                port
            );
        } else if (type == CapsuleSettings::TYPE_GSGI) {
            handler = make_shared<GSGIHandler>(
                dynamic_pointer_cast<GSGISettings>(conf), 
                host, 
                port
            );
        } else {
            continue;
        }
        handlers.push_back(handler);
    }
}

void Manager::handle(SSLClient *client, const GeminiRequest &request) {
    const string &host = request.getHost();
    const string &path = request.getPath();
    int port = request.getPort();
    for (shared_ptr<Handler> handler : handlers) {
        if (handler->shouldHandle(host, port, path)) {
            handler->handle(client->getSSL(), request);
            return;
        }
    }
    return;
}