#ifndef __GEMCAPS_MANAGER__
#define __GEMCAPS_MANAGER__

#include <string>
#include <vector>
#include <memory>
#include <set>

#include <wolfssl/options.h>
#include <wolfssl/ssl.h>

#include <uv.h>

#include <yaml-cpp/yaml.h>

#include <parallel_hashmap/phmap.h>

#include "cache.hpp"
#include "gemcaps/settings.hpp"
#include "server.hpp"
#include "gemcaps/handler.hpp"


inline const std::string NAME = "name";
inline const std::string SERVER = "server";

class Manager;

/**
 * An object that sends messages to the client from a handler
 */
class GeminiConnection : public ClientConnection, public ClientContext {
private:
	Manager *manager;
	Request request;
	SSLClient *client;
	BufferPipe buffer;
    bool sentHeader = false;
public:
	GeminiConnection(Manager *manager, SSLClient *client)
		: manager(manager),
		  client(client) {}

	Request &getRequest() noexcept { return request; }

	// Overrides ClientConnection
	const Request &getRequest() const noexcept { return request; }
	void send(const void *data, size_t length) noexcept;
	void close() noexcept;

	// Override ClientContext
	void on_close(SSLClient *client) noexcept;
	void on_read(SSLClient *client) noexcept {}
	void on_write(SSLClient *client) noexcept;
};


class Manager : public ServerContext, public ClientContext {
private:
    struct ClientData {
        Request request;
        BufferPipe body;
    };
    phmap::flat_hash_map<std::string, std::shared_ptr<SSLServer>> servers;
    phmap::flat_hash_map<SSLServer *, std::vector<std::shared_ptr<Handler>>> handlers;

    phmap::flat_hash_map<SSLClient *, std::unique_ptr<GeminiConnection>> requests;
public:

    /**
     * Load the servers into memory
     * 
     * @param config_dir directory of server configs
     */
    void loadServers(std::string config_dir) noexcept;
    /**
     * Load the handlers into memory
     * 
     * @param config_dir directory of handler configs
     */
    void loadHandlers(std::string config_dir) noexcept;
    /**
     * Start the servers
     */
    void startServers() noexcept;

    // Overrides ServerContext
    void on_accept(SSLServer *server, SSLClient *client) noexcept;

    // Override ClientContext
    void on_close(SSLClient *client) noexcept;
    void on_read(SSLClient *client) noexcept;
	void on_write(SSLClient *client) noexcept {}
};

#endif