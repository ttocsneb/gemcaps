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
#include "server.hpp"

#define RES_INPUT 10
#define RES_SENSITIVE_INPUT 11
#define RES_SUCCESS 20
#define RES_REDIRECT_TEMP 30
#define RES_REDIRECT_PERM 31
#define RES_FAIL_TEMP 40
#define RES_SERVER_UNAVAIL 41
#define RES_ERROR_CGI 42
#define RES_ERROR_PROXY 43
#define RES_SLOW_DOWN 44
#define RES_FAIL_PERM 50
#define RES_NOT_FOUND 51
#define RES_GONE 52
#define RES_BAD_REQUEST 59
#define RES_CERT_REQUIRED 60
#define RES_CERT_NOT_AUTH 61
#define RES_CERT_NOT_VALID 62

typedef void(*ContextDestructorCB)(void *ctx);

class GeminiRequest {
private:
    std::string schema;
    std::string host;
    int port;
    std::string path;
    std::string query;
    std::string request;
    bool valid;
public:
    GeminiRequest(std::string request);

    bool isValid() const { return valid; }
    const std::string &getSchema() const {return schema; }
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
   Cache *cache;
protected:
    /**
     * Get the cache
     * 
     * @return cache
     */
    Cache *getCache() const { return cache; }
public:
    /**
     * Create a new handler
     * 
     * @param cache cache to use
     * @param host host to match against
     * @param port port to match against
     */
    Handler(Cache *cache, Glob host, int port)
        : cache(cache),
          host(host),
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
     * @param client SSL Client
     * @param request gemini request
     */
    virtual void handle(SSLClient *client, const GeminiRequest &request) = 0;
};

class ClientContext {
private:
    std::string buffer;
    Handler *handler = nullptr;
    void *context = nullptr;

    ContextDestructorCB destructor = nullptr;
public:
    ~ClientContext();

    void setContext(void *ctx, ContextDestructorCB cb);
    void *getContext() { return context; }

    void setHandler(Handler *handler) { this->handler = handler; }
    Handler *getHandler() { return handler; }

    std::string &getBuffer() { return buffer; }
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
        std::string listen;

        bool operator<(const ServerSettings &rhs) const;
    };
private:
    std::vector<std::shared_ptr<Handler>> handlers;
    std::set<ServerSettings> servers;
    Cache cache;

    void loadCapsule(const std::string &filename, const std::string &file);
public:
    Manager(uv_loop_t *loop, unsigned int max_cache = 0)
        : cache(loop, max_cache) {}
    /**
     * Load all handlers in the specified directory
     * 
     * @param directory directory with all the handlers
     */
    void load(const std::string &directory);

    /**
     * Handle the request
     * 
     * @param client client
     * @param request request header
     */
    void handle(SSLClient *client, const GeminiRequest &request);

    const std::set<ServerSettings> &getServers() const { return servers; }
};

#endif