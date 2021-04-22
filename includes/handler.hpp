#ifndef __GEMCAPS_HANDLER__
#define __GEMCAPS_HANDLER__

#include <string>

#include <wolfssl/options.h>
#include <wolfssl/ssl.h>

#include "glob.hpp"
#include "manager.hpp"
#include "settings.hpp"

/**
 * The base class for all handlers
 */
class Handler {
private:
   Glob host;
   int port;
public:
    /**
     * Create a new handler
     * 
     * @param host host to match against
     * @param port port to match against
     */
    Handler(Glob host, int port)
        : host(host),
          port(port) {}

    /**
     * Check whether the handler should handle this request
     * 
     * @param host requested host
     * @param port received port
     * @param path requested path
     * 
     * @return whether the request should handle
     */
    virtual bool shouldHandle(const std::string &host, int port, const std::string &path) { 
        return this->port == port && this->host == host;
    }

    /**
     * Handle the requset
     * 
     * @param ssl ssl context
     * @param request gemini request
     */
    virtual void handle(WOLFSSL *ssl, const GeminiRequest &request) = 0;
};

#endif