#ifndef __GEMCAPS_FILEHANDLER__
#define __GEMCAPS_FILEHANDLER__

#include <memory>

#include "settings.hpp"
#include "manager.hpp"

class FileContext {
private:
    uv_fs_t req;
    const GeminiRequest request;
    std::string path;
public:
    FileContext(const GeminiRequest &request)
        : request(request) {}
    uv_fs_t *getReq() { return &req; }
    const GeminiRequest &getRequest() const { return request; }

    void setPath(const std::string &path) { this->path = path; }
    const std::string &getPath() const { return path; }
};

/**
 * The file request handler
 */
class FileHandler : public Handler {
private:
    std::shared_ptr<FileSettings> settings;
public:
    FileHandler(Cache *cache, std::shared_ptr<FileSettings> settings, Glob host, int port)
        : settings(settings),
          Handler(cache, host, port) {}
    /**
     * Handle the reading of a file
     * 
     * The process of reading a file is outlined as follows:
     * 
     * 1. Find the absolute pathname on disk.
     * 2. If the file is cached, send the cached data to the client
     * 3. Let the cache know that we are actively creating the cache
     * 4. Perform realpath operation to remove symlinks.
     * 5. If the handler does not have permissions to read that file, then return an error
     * 6. If the file does not exist, return an error
     * 7. Read the file to the client
     * 
     * Note: when data is sent to the client, the cache will be updated with the sent data
     */
    void handle(SSLClient *client, const GeminiRequest &request);
};

#endif