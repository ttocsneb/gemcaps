#ifndef __GEMCAPS_FILEHANDLER__
#define __GEMCAPS_FILEHANDLER__

#include <memory>
#include <vector>

#include "settings.hpp"
#include "manager.hpp"
#include "context.hpp"

class FileHandler;


class FileContext : public ClientContext {
public:
    uv_fs_t req;
    std::string file;
    std::shared_ptr<FileSettings> settings;
    FileHandler *handler;
    uv_file file_fd;
    unsigned long offset;
    char filebuf[1024];
    uv_buf_t buf;
    bool processing_cache;

    FileContext(SSLClient *client, Cache *cache, GeminiRequest request, FileHandler *handler, std::shared_ptr<FileSettings> settings)
        : ClientContext(client, cache, request),
          handler(handler),
          settings(settings),
          offset(0),
          file_fd(0),
          processing_cache(false) {
        buf.base = filebuf;
        buf.len = sizeof(filebuf);
    }
    ~FileContext();

    void onClose();
    void onRead();
    void onWrite();
};

/**
 * The file request handler
 */
class FileHandler : public Handler {
private:
    std::shared_ptr<FileSettings> settings;
    std::set<FileContext*> contexts;
protected:
    void _notify_close(FileContext *context);
    friend FileContext;
public:
    FileHandler(Cache *cache, std::shared_ptr<FileSettings> settings, Glob host, int port)
        : settings(settings),
          Handler(cache, host, port, settings->getRules()) {}
    ~FileHandler();

    void handle(SSLClient *client, const GeminiRequest &request);
};

#endif