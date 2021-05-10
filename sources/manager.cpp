#include "manager.hpp"

#include "main.hpp"

#include "filehandler.hpp"
#include "gsgihander.hpp"

#include <filesystem>
#include <iostream>
#include <sstream>

using std::string;
using std::shared_ptr;
using std::make_shared;
using std::dynamic_pointer_cast;
using std::stringstream;

using std::cout;
using std::endl;

namespace fs = std::filesystem;

class RequestContext : public ClientContext {
    Manager *manager;
public:
    RequestContext(Manager *manager, SSLClient *client, Cache *cache)
        : ClientContext(manager, client, cache),
          manager(manager) {}
    void onClose() {

    }
    void onDestroy() {
        destroy_done();
    }
    void onRead() {
        char header[1024];
        SSLClient *client = getClient();
        int read = client->read(header, sizeof(header));
        if (read < 0) {
            if (client->wants_read()) {
                return;
            }
            LOG_ERROR("WOLFSSL - " << client->get_error_string());
            client->crash();
            return;
        }
        string data(header, read);
        buffer += string(header, read);
        // Close the connection if the header is too big
        if (buffer.length() > 1024) {
            client->close();
            return;
        }
        // The client hasn't sent all of the data
        if (buffer.find('\n') == string::npos) {
            return;
        }
        GeminiRequest request(buffer);
        if (request.isValid()) {
            manager->handle(client, request);
            return;
        }
    }
    void onWrite() {

    }
};

bool Handler::handleRequest(SSLClient *client, const GeminiRequest &request) {
    const string &host = request.getHost();
    int port = request.getPort();
    const string &path = request.getPath();
    if (!(this->port == port && this->host.match(host))) {
        return false;
    }
    if (!settings->getPathRegex().match(path)) {
        return false;
    }
    string new_path = path.substr(settings->getPath().length());
    if (!settings->getRules().empty()) {
        bool valid = false;
        for (const auto &rule : settings->getRules()) {
            if (rule.match(new_path)) {
                valid = true;
                break;
            }
        }
        if (!valid) {
            return false;
        }
    }
    if (shouldHandle(host, port, path)) {
        handle(client, request, new_path);
        return true;
    }
    return false;
}

bool Manager::ServerSettings::operator<(const Manager::ServerSettings &rhs) const {
    if (port != rhs.port) {
        return port < rhs.port;
    }
    return listen < rhs.listen;
}

void Manager::load(const string &directory) {
    for (const auto &entry : fs::directory_iterator(directory)) {
        string ext = entry.path().extension().string();
        if (ext == ".yml" || ext == ".yaml") {
            loadCapsule(entry.path().string(), entry.path().filename().string());
        }
    }
}

void Manager::loadCapsule(const string& filename, const string& name) {
    CapsuleSettings settings;
    settings.loadFile(filename);

    const Regex &host = settings.getHost();
    int port = settings.getPort();
    const auto &confs = settings.getHandlers();

    ServerSettings conf;
    conf.listen = settings.getListen();
    conf.port = port;
    conf.key = settings.getKey();
    conf.cert = settings.getCert();

    if(!servers.insert(conf).second) {
        if (!conf.key.empty() || !conf.cert.empty()) {
            cout << "Warning: '" << name << "' tried to use a custom certificate for a host that has already been defined. This certificate will not be used!" << endl;
        }
    }

    // Iterate through each handler's settings
    for (auto conf : confs) {
        // Create the handler from the settings type
        shared_ptr<Handler> handler;
        string type = conf->getType();
        if (type == CapsuleSettings::TYPE_FILE) {
            handler = make_shared<FileHandler>(
                &cache,
                dynamic_pointer_cast<FileSettings>(conf), 
                host, 
                port
            );
        } else if (type == CapsuleSettings::TYPE_GSGI) {
            handler = make_shared<GSGIHandler>(
                &cache,
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

void Manager::handle(SSLClient *client) {
    client->setTimeout(timeout);
    RequestContext *context = new RequestContext(this, client, &cache);
    _add_context(context);
    client->setContext(context);
}

void Manager::handle(SSLClient *client, GeminiRequest request) {
    const string &host = request.getHost();
    const string &path = request.getPath();
    int port = request.getPort();
    for (shared_ptr<Handler> handler : handlers) {
        if (handler->handleRequest(client, request)) {
            return;
        }
    }
    string error = "41 There is no server available to process your request\r\n";
    client->write(error.c_str(), error.length());
    client->close();
    return;
}