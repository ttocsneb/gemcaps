#include "manager.hpp"

#include <iostream>

#include <yaml-cpp/yaml.h>

#include "loader.hpp"

#include "gemcaps/log.hpp"

using std::string;
using std::make_unique;

using std::cerr;
using std::endl;

string join(string a, string b) {
    if (a.back() == '/' || a.back() == '\\') {
        return a + b;
    }
    return a + '/' + b;
}

bool is_yaml(uv_dirent_t *entry) {
    // Assert that the entry is a file
    if (entry->type != UV_DIRENT_FILE) {
        return false;
    }
    // Assert that the file extension is yml or yaml
    string name = entry->name;
    size_t pos = name.rfind('.');
    if (pos == string::npos) {
        return false;
    }
    string ext = name.substr(pos);
    for (int i = 0; i < ext.length(); ++i) {
        ext[i] = tolower(ext[i]);
    }
    return ext == ".yml" || ext == ".yaml";
}


void GeminiConnection::send(const void *data, size_t length) noexcept {
	buffer.write((const char *)data, length);
	char buf[1024];
	size_t read = buffer.read(1024, buf);

	client->write(buf, read);
}

void GeminiConnection::close() noexcept {
	client->close();
}

void GeminiConnection::on_close(SSLClient *client) noexcept {
	manager->on_close(client);
}

void GeminiConnection::on_write(SSLClient *client) noexcept {
	if (buffer.ready() > 0) {
		char buf[1024];
		size_t read = buffer.read(1024, buf);
		client->write(buf, read);
	}
}


void Manager::loadServers(string config_dir) noexcept {
    servers.clear();

    uv_loop_t *loop = uv_default_loop();
    uv_fs_t scan_req;

    uv_fs_scandir(loop, &scan_req, config_dir.c_str(), 0, nullptr);
    
    if (scan_req.result < 0) {
        LOG_ERROR("[Manager::loadServers] Could not read from '" << config_dir << "': " << uv_strerror(scan_req.result));
        return;
    }

    uv_dirent_t entry;
    while (uv_fs_scandir_next(&scan_req, &entry) != UV_EOF) {
        if (!is_yaml(&entry)) {
            continue;
        }
        string filename = join(config_dir, entry.name);

        YAML::Node node = YAML::LoadFile(filename);
        try {
            auto server = loadServer(node, config_dir, loop);
			string name = getProperty<string>(node, NAME);
            servers.insert({name, server});
			LOG_INFO("Loaded server '" << name << "'");
        } catch (InvalidSettingsException e) {
            LOG_ERROR("[Manager::loadServers] while loading " << e.getMessage(filename));
        }
    }

    if (servers.empty()) {
        LOG_WARN("[Manager::loadServers] No servers were loaded");
    }
}

void Manager::loadHandlers(string config_dir) noexcept {
    handlers.clear();

    HandlerLoader loader;
    loader.loadFactories();

    uv_loop_t *loop = uv_default_loop();
    uv_fs_t scan_req;

    uv_fs_scandir(loop, &scan_req, config_dir.c_str(), 0, nullptr);
    
    if (scan_req.result < 0) {
        LOG_ERROR("[Manager::loadHandlers] Could not read from '" << config_dir << "': " << uv_strerror(scan_req.result));
        return;
    }

    uv_dirent_t entry;
    while (uv_fs_scandir_next(&scan_req, &entry) != UV_EOF) {
        if (!is_yaml(&entry)) {
            continue;
        }
        string filename = join(config_dir, entry.name);

        YAML::Node node = YAML::LoadFile(filename);
        try {
            auto handler = loader.loadHandler(node, config_dir);
            handlers.push_back(handler);
        } catch (InvalidSettingsException e) {
            LOG_ERROR("[Manager::loadHandlers] while loading " << e.getMessage(filename));
        }
    }

    if (handlers.empty()) {
        LOG_WARN("[Manager::loadHandlers] No handlers were loaded");
    }
}

void Manager::startServers() noexcept {
    for (auto server : servers) {
        server.second->listen();
    }
}

void Manager::on_accept(SSLServer *server, SSLClient *client) noexcept {
    requests.insert({client, make_unique<GeminiConnection>(this, client)});
    client->setContext(this);
    client->setTimeout(1000);
    client->listen();
}

// These next functions for parsing a host is gross, but I guess it works
bool parseQuery(Request *request, size_t start);
bool parsePath(Request *request, size_t start);
bool parsePort(Request *request, size_t start);
bool parseHost(Request *request) {
    size_t start = request->header.find("://");
    if (start == string::npos) {
        return false;
    }
    start += 3;

    bool result = true;
    size_t end = request->header.find(':', start);
    if (end != string::npos) {
        result = parsePort(request, end + 1);
    } else {
        request->port = 1965;
        end = request->header.find('/', start);
        if (end != string::npos) {
            result = parsePath(request, end);
        } else {
            request->path.clear();
            end = request->header.find('?', start);
            if (end != string::npos) {
                result = parseQuery(request, end);
            } else {
                request->query.clear();
                end = request->header.find('\r', start);
                if (end == string::npos) {
                    end = request->header.find('\n', start);
                    if (end == string::npos) {
                        return false;
                    }
                }
            }
        }
    }
    if (!result) {
        return false;
    }
    request->host = request->header.substr(start, end - start);
    return true;
}
bool parsePort(Request *request, size_t start) {
    bool result = true;
    size_t end = request->header.find('/', start);
    if (end != string::npos) {
        result = parsePath(request, end);
    } else {
        request->path.clear();
        end = request->header.find('?', start);
        if (end != string::npos) {
            result = parseQuery(request, end);
        } else {
            request->query.clear();
            end = request->header.find('\r', start);
            if (end == string::npos) {
                end = request->header.find('\n', start);
                if (end == string::npos) {
                    return false;
                }
            }
        }
    }
    if (result == false) {
        return false;
    }
    int port = atoi(request->header.substr(start, end - start).c_str());
    if (port <= 0 || port > 65535) {
        return false;
    }
    request->port = port;
    return true;
}
bool parsePath(Request *request, size_t start) {
    bool result = true;
    size_t end = request->header.find('?', start);
    if (end != string::npos) {
        result = parseQuery(request, end);
    } else {
        request->query.clear();
        end = request->header.find('\r', start);
        if (end == string::npos) {
            end = request->header.find('\n', start);
            if (end == string::npos) {
                return false;
            }
        }
    }
    if (result == false) {
        return false;
    }
    request->path = request->header.substr(start, end - start);
    return true;
}
bool parseQuery(Request *request, size_t start) {
    bool result = true;
    size_t end = request->header.find('\r', start);
    if (end == string::npos) {
        end = request->header.find('\n', start);
        if (end == string::npos) {
            return false;
        }
    }
    if (result == false) {
        return false;
    }
    request->query = request->header.substr(start, end - start);
    return true;
}

void Manager::on_read(SSLClient *client) noexcept {
    char buf[1024];
    GeminiConnection *gemini = requests[client].get();
    Request &request = gemini->getRequest();
    int read = client->read(1024, buf);
    if (read < 0) {
        if (read == WOLFSSL_ERROR_WANT_READ || read == WOLFSSL_ERROR_WANT_WRITE) {
            return;
        }
        // There was probably an error while performing the handshake, so crash the connection
        client->crash();
        return;
    }
    if (read > 0) {
        request.header += string(buf, read);
    }

    if (request.header.length() > 1024) {
        // The header is invalid, and will be discarded
        client->crash();
        return;
    }

    size_t pos = request.header.find('\n');
    if (pos == string::npos) {
        // The header isn't finished yet, wait for more data
        return;
    }

    // The request is finished
    client->stop_listening();
    request.header = request.header.substr(0, pos + 1);

    // Parse the header
    if (!parseHost(&request)) {
        client->crash();
        return;
    }

    // Figure out which handler should process the manager

    for (auto handler : handlers) {
        if (handler->shouldHandle(request.host, request.path)) {
            client->setTimeout(30000);
            handler->handle(gemini);
            return;
        }
    }
    client->crash();
}

void Manager::on_close(SSLClient *client) noexcept {
    auto found = requests.find(client);
    if (found == requests.end()) {
        return;
    }
    requests.erase(found);
}
