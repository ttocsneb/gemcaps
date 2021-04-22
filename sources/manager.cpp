#include "manager.hpp"

#include "filehandler.hpp"
#include "gsgihander.hpp"

#include <filesystem>

using std::string;
using std::shared_ptr;
using std::make_shared;
using std::dynamic_pointer_cast;

namespace fs = std::filesystem;

Manager::Manager(const string &directory) {
    for (const auto &entry : fs::directory_iterator(directory)) {
        string ext = entry.path().extension();
        if (ext == ".yml" || ext == ".yaml") {
            YAML::Node node = YAML::LoadFile(entry.path().string());
            loadCapsule(node);
        }
    }
}

void Manager::loadCapsule(YAML::Node &node) {
    CapsuleSettings settings;
    settings.load(node);

    auto host = settings.getHost();
    auto port = settings.getPort();
    auto confs = settings.getHandlers();

    ports.insert(port);

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

bool Manager::handle(WOLFSSL *ssl, const GeminiRequest &request) {
    const string &host = request.getHost();
    const string &path = request.getPath();
    int port = request.getPort();
    for (auto handler : handlers) {
        if (handler->shouldHandle(host, port, path)) {
            handler->handle(ssl, request);
            return true;
        }
    }
    return false;
}