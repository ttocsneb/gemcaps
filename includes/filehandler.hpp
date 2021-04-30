#ifndef __GEMCAPS_FILEHANDLER__
#define __GEMCAPS_FILEHANDLER__

#include <memory>

#include "settings.hpp"
#include "manager.hpp"

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
    void handle(SSLClient *client, const GeminiRequest &request);
};

#endif