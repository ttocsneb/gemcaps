#ifndef __GEMCAPS_FILEHANDLER__
#define __GEMCAPS_FILEHANDLER__

#include <memory>

#include "settings.hpp"
#include "handler.hpp"

/**
 * The file request handler
 */
class FileHandler : public Handler {
private:
    std::shared_ptr<FileSettings> settings;
public:
    FileHandler(std::shared_ptr<FileSettings> settings, Glob host, int port)
        : settings(settings),
          Handler(host, port) {}
    void handle(WOLFSSL *ssl, const GeminiRequest &request);
};

#endif