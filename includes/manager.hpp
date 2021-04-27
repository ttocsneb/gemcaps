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

#include "cache.hpp"
#include "glob.hpp"
#include "settings.hpp"

class GeminiRequest {
private:
    std::string host;
    int port;
    std::string path;
    std::string query;
    std::string request;
public:
    GeminiRequest(std::string request);

    const std::string &getHost() const { return host; }
    int getPort() const { return port; }
    const std::string &getPath() const { return path; }
    const std::string &getQuery() const { return query; }
    const std::string &getRequest() const { return request; }
};

/**
 * The base class for all handlers
 */
class Handler {
private:
   Glob host;
   int port;
public:
    /**
     * Create a new handler
     * 
     * @param host host to match against
     * @param port port to match against
     */
    Handler(Glob host, int port)
        : host(host),
          port(port) {}

    /**
     * Check whether the handler should handle this request
     * 
     * @param host requested host
     * @param port received port
     * @param path requested path
     * 
     * @return whether the request should handle
     */
    virtual bool shouldHandle(const std::string &host, int port, const std::string &path) { 
        return this->port == port && this->host == host;
    }

    /**
     * Handle the requset
     * 
     * @param ssl ssl context
     * @param request gemini request
     */
    virtual CacheData handle(WOLFSSL *ssl, const GeminiRequest &request) = 0;
};

/**
 * Directs requests to the proper handler
 */
class Manager {
public:
    struct ServerSettings {
        int port;
        std::string cert;
        std::string key;
        std::string host;
    };
private:
    std::vector<std::shared_ptr<Handler>> handlers;
    std::set<ServerSettings> servers;
    Cache cache;

    void loadCapsule(YAML::Node &node);
public:
    /**
     * Load all handlers in the specified directory
     * 
     * @param directory directory with all the handlers
     */
    void load(const std::string &directory);

    /**
     * Handle the request
     * 
     * @param client uv client connection
     * @param ssl ssl connection
     * @param request request header
     */
    void handle(std::shared_ptr<uv_tcp_t> client, std::shared_ptr<WOLFSSL> ssl, const GeminiRequest &request);

    const std::set<ServerSettings> &getServers() const { return servers; }
};

#endif