#ifndef __GEMCAPS_CONTEXT__
#define __GEMCAPS_CONTEXT__

#include <string>

#include "server.hpp"
#include "cache.hpp"

class SSLClient;

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


class ClientContext {
private:
    GeminiRequest request;
    SSLClient *client;
    Cache *cache;
public:
    std::string buffer;

    ClientContext(SSLClient *client, Cache *cache, GeminiRequest request)
        : request(request),
          client(client),
          cache(cache) {}
    
    const GeminiRequest &getRequest() const { return request; }
    SSLClient *getClient() { return client; }
    Cache *getCache() { return cache; }

    virtual void onClose() = 0;
    virtual void onRead() = 0;
    virtual void onWrite() = 0;
    virtual void onTimeout();
};

#endif