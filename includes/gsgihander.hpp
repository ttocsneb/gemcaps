#ifndef __GEMCAPS_GSGIHANDLER__
#define __GEMCAPS_GSGIHANDLER__

#include <memory>

#include "handler.hpp"
#include "settings.hpp"
#include "gsgi.h"

/**
 * The gemini request handler
 */
class GSGIHandler : public Handler {
private:
    std::shared_ptr<GSGISettings> settings;
    gsgi instance;

    void create();
public:
    GSGIHandler(std::shared_ptr<GSGISettings> settings, Glob host, int port)
            : settings(settings),
            Handler(host, port) {
        memset(&instance, 0, sizeof(gsgi));
    }
    ~GSGIHandler();

    void handle(WOLFSSL *ssl, const GeminiRequest &request);
};

#endif