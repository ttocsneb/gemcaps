#ifndef __GEMCAPS_SERVER__
#define __GEMCAPS_SERVER__

#include <string>
#include <memory>

#include <wolfssl/options.h>
#include <wolfssl/ssl.h>

/**
 * A WolfSSL server wrapper
 */
class SSLServer {
private:
    std::unique_ptr<WOLFSSL_CTX> context;
public:
    /**
     * Load the ssl certificates
     * 
     * @param cert certificate file
     * @param key key file
     * 
     * @return whether the load was successful
     */
    bool load(const std::string &cert, const std::string &key);

    /**
     * Check whether the server is loaded
     * 
     * @return whether the server is loaded
     */
    bool isLoaded() const;

    /**
     * Accept a client connection
     * 
     * @param socketfd socket file descriptor
     * 
     * @return the created ssl or nullptr
     */
    std::shared_ptr<WOLFSSL> accept(int socketfd);
};


#endif