#ifndef __GEMCAPS_WSGIHANDLER__
#define __GEMCAPS_WSGIHANDLER__

#include "handler.hpp"
#include "settings.hpp"
#include "gsgi.h"

/**
 * The gemini request handler
 */
class GeminiHandler : public Handler {
private:
    GSGISettings settings;
    gsgi object;
public:
    GeminiHandler(GSGISettings settings);
    ~GeminiHandler();

    void handle(WOLFSSL *ssl, const GeminiRequest &request);
};

#endif