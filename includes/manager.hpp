#ifndef __GEMCAPS_MANAGER__
#define __GEMCAPS_MANAGER__

#include <string>

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

class Manager {
private:
public:
};

#endif