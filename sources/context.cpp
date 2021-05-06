#include "context.hpp"

#include <sstream>

using std::ostringstream;
using std::string;

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

string GeminiRequest::getRequestName() const {
    int p = port;
    if (p == 0) {
        p = 1965;
    }
    ostringstream oss;
    oss << host << ':' << p << path;
    return oss.str();
}

void ContextManager::_close_context(ClientContext *ctx) {
    if(contexts.erase(ctx)) {
        delete ctx;
    }
}

void ContextManager::_add_context(ClientContext *ctx) {
    contexts.insert(ctx);
}

ContextManager::~ContextManager() {
    for (ClientContext *ctx : contexts) {
        delete ctx;
    }
}

void ClientContext::onTimeout() {
    const static string response("42 Request couldn't be processed in time\r\n");
    getClient()->write(response.c_str(), response.length());
    getClient()->close();
}

void ClientContext::close() {
    manager->_close_context(this);
}