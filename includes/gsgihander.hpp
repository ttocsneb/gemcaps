#ifndef __GEMCAPS_GSGIHANDLER__
#define __GEMCAPS_GSGIHANDLER__

#include <memory>

#include "manager.hpp"
#include "settings.hpp"

/**
 * The gemini request handler
 */
class GSGIHandler : public Handler {
private:
    std::shared_ptr<GSGISettings> settings;

    void create();
public:
    GSGIHandler(Cache *cache, std::shared_ptr<GSGISettings> settings, Regex host, int port)
            : settings(settings),
            Handler(cache, host, port, settings.get()) {
    }
    ~GSGIHandler();

    void handle(SSLClient *client, const GeminiRequest &request, std::string pat);
};

#endif