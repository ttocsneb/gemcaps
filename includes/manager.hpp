#ifndef __GEMCAPS_MANAGER__
#define __GEMCAPS_MANAGER__

#include <string>
#include <vector>
#include <memory>
#include <set>

#include <yaml-cpp/yaml.h>

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

class Handler;
#include "handler.hpp"

/**
 * Directs requests to the proper handler
 */
class Manager {
private:
    std::vector<std::shared_ptr<Handler>> handlers;
    std::set<int> ports;

    void loadCapsule(YAML::Node &node);
public:
    /**
     * Create a new manager loading all handlers in the specified directory
     * 
     * @param directory directory with all the handlers
     */
    Manager(const std::string &directory);

    /**
     * Handle the request
     * 
     * @param ssl ssl connection
     * @param request gemini request
     * 
     * @return whether the request was processed
     */
    bool handle(WOLFSSL *ssl, const GeminiRequest &request);

    const std::set<int> &getPorts() const { return ports; }
};

#endif