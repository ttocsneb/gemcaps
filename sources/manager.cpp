#include "manager.hpp"

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

ClientContext::~ClientContext() {
    if (destructor) {
        destructor(context);
    }
}

void ClientContext::setContext(void *ctx, ContextDestructorCB cb) {
    if (destructor) {
        destructor(context);
    }
    destructor = cb;
    context = ctx;
}

GeminiRequest::GeminiRequest(string request)
        : port(0),
          valid(true) {
    // Remove any leading or trailing whitespace

    // Find the first character of the request
    for (int i = 0; i < request.length(); ++i) {
        if (!isspace(request.at(i))) {
            request = request.substr(i);
            break;
        }
    }
    // Find the last character of the request
    for (int i = request.length() - 1; i >= 0; --i) {
        if (!isspace(request.at(i))) {
            request = request.substr(0, i + 1);
            break;
        }
    }

    this->request = request;

    // Parse the request
    size_t pos = request.find("://");
    if (pos == string::npos) {
        valid = false;
        return;
    }
    schema = request.substr(0, pos);
    size_t start = pos + 3;
    pos = request.find(":", start);
    // Check if there is a port or not
    if (pos != string::npos) {
        // There is a port
        // Get the port
        host = request.substr(start, pos - start);
        if (host.empty() || host.at(0) == '/') {
            valid = false;
            return;
        }
        start = pos + 1;
        pos = request.find("/", start);
        if (pos == string::npos) {
            // There is no path
            pos = request.find("?", start);
            if (pos == string::npos) {
                // There is no query
                port = atoi(request.substr(start).c_str());
                return;
            }
            // There is a query
            port = atoi(request.substr(start, pos - start).c_str());
            // get query component
            query = request.substr(pos);
            return;
        }
        // There is a path
        port = atoi(request.substr(start, pos - start).c_str());
        start = pos;
    } else {
        pos = request.find("/", start);
        if (pos == string::npos) {
            pos = request.find("?", start);
            if (pos == string::npos) {
                // There is no query
                host = request.substr(start);
                if (host.empty()) {
                    valid = false;
                }
                return;
            }
            // There is a query
            host = request.substr(start, pos - start);
            if (host.empty()) {
                valid = false;
                return;
            }
            // Get the query component
            query = request.substr(pos);
            return;
        }
        // There is a path
        host = request.substr(start, pos - start);
        if (host.empty()) {
            valid = false;
            return;
        }
        start = pos;
    }
    // get path component
    pos = request.find("?", start);
    if (pos == string::npos) {
        // There is no query component
        path = request.substr(start);
        return;
    }
    path = request.substr(start, pos - start);

    // get the query component
    query = request.substr(pos);

    // Validate the schema
    static const string Schema("gemini");
    if (schema.length() != Schema.length()) {
        valid = false;
        return;
    }
    for (int i = 0; i < Schema.length(); ++i) {
        if (tolower(schema.at(i)) != tolower(Schema.at(i))) {
            valid = false;
            return;
        }
    }
}

bool Handler::shouldHandle(const string &host, int port, const string &path) {
    if (!(this->port == port && this->host == host)) {
        return false;
    }
    if (rules.empty()) {
        return true;
    }
    for (const auto &rule : rules) {
        if (rule.match(path)) {
            return true;
        }
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

void Manager::handle(SSLClient *client, const GeminiRequest &request) {
    const string &host = request.getHost();
    const string &path = request.getPath();
    int port = request.getPort();
    for (shared_ptr<Handler> handler : handlers) {
        if (handler->shouldHandle(host, port, path)) {
            handler->handle(client, request);
            return;
        }
    }
    string error = "41 There is no server available to process your request\r\n";
    client->write(error.c_str(), error.length());
    return;
}