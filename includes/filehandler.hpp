#ifndef __GEMCAPS_FILEHANDLER__
#define __GEMCAPS_FILEHANDLER__

#include "settings.hpp"
#include "handler.hpp"

/**
 * The file request handler
 */
class FileHandler : public Handler {
private:
    FileSettings settings;
public:
    FileHandler(FileSettings settings)
        : settings(settings) {}
    void handle(WOLFSSL *ssl, const GeminiRequest &request);
};

#endif