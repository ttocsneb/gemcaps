#ifndef __GEMCAPS_FILEHANDLER__
#define __GEMCAPS_FILEHANDLER__

#include <memory>
#include <vector>

#include "settings.hpp"
#include "manager.hpp"

class FileHandler;

class FileContext {
public:
    enum State {
        REALPATH,
        READFILE,
        READY
    };
private:
    uv_fs_t req;
    const GeminiRequest request;
    std::string path;
    std::string disk;
    std::shared_ptr<FileSettings> settings;
    State state;
    FileHandler *handler;
    std::vector<std::string> files;
public:
    FileContext(FileHandler *handler, const GeminiRequest &request, std::shared_ptr<FileSettings> settings)
        : handler(handler),
          request(request),
          settings(settings) {}
    uv_fs_t *getReq() { return &req; }
    const GeminiRequest &getRequest() const { return request; }

    void setPath(const std::string &path) { this->path = path; }
    const std::string &getPath() const { return path; }

    void setDisk(const std::string &disk) { this->disk = disk; }
    const std::string &getDisk() const { return disk; }

    void setState(State state) { this->state = state; } 
    State getState() const { return state; }

    FileHandler *getHandler() const { return handler; }

    std::vector<std::string> &getFiles() { return files; }
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

    void internalError(SSLClient *client, FileContext *context);

    void sendCache(SSLClient *client, FileContext *context);
    void gotRealPath(SSLClient *client, FileContext *context, std::string realPath);
    void gotInvalidPath(SSLClient *client, FileContext *context);
    void gotAccess(SSLClient *client, FileContext *context, ssize_t result);
    void gotStat(SSLClient *client, FileContext *context, uv_stat_t *statbuf);
};

#endif