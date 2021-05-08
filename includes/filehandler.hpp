#ifndef __GEMCAPS_FILEHANDLER__
#define __GEMCAPS_FILEHANDLER__

#include <memory>
#include <vector>

#include "settings.hpp"
#include "manager.hpp"
#include "context.hpp"

class FileHandler;


class FileContext : public ClientContext {
private:
    CacheKey key;
    GeminiRequest request;
    bool destroying = false;
public:
    uv_fs_t *req;
    std::string file;
    std::shared_ptr<FileSettings> settings;
    FileHandler *handler;
    uv_file file_fd = 0;
    unsigned long offset = 0;
    char filebuf[1024];
    uv_buf_t buf;
    bool processing_cache = false;
    bool closing = false;

    FileContext(FileHandler *handler, SSLClient *client, Cache *cache, GeminiRequest request, std::shared_ptr<FileSettings> settings);
    ~FileContext();

    void onDestroy();
    void onClose();
    void onRead();
    void onWrite();

    void handle();
    void send(const CachedData &data);
    CachedData createCache(int response, std::string meta);

    const CacheKey &getCacheKey() const { return key; }
    const GeminiRequest &getRequest() const { return request; }

    void _closed();
};

/**
 * The file request handler
 */
class FileHandler : public Handler, public ContextManager {
private:
    std::shared_ptr<FileSettings> settings;
public:
    FileHandler(Cache *cache, std::shared_ptr<FileSettings> settings, Regex host, int port)
        : settings(settings),
          Handler(cache, host, port, settings->getRules()) {}

    void handle(SSLClient *client, const GeminiRequest &request);
};

#endif