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
#include "gemcaps/server.hpp"
#include "gemcaps/handler.hpp"


inline const std::string NAME = "name";


class Manager : public ServerContext, public ClientContext, public IBufferPipeObserver {
private:
    struct ClientData {
        Request request;
        BufferPipe body;
    };

    std::vector<std::shared_ptr<Handler>> handlers;
    phmap::flat_hash_map<std::string, std::shared_ptr<SSLServer>> servers;

    phmap::flat_hash_map<SSLClient *, std::unique_ptr<ClientData>> requests;
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
    void on_write(SSLClient *client) noexcept;

    // Override IBufferPipeObserver
    void on_buffer_write(IBufferPipe *buffer) noexcept;
};

#endif