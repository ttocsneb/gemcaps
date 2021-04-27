#ifndef __GEMCAPS_SERVER__
#define __GEMCAPS_SERVER__

#include <string>
#include <sstream>
#include <memory>

#include <wolfssl/options.h>
#include <wolfssl/ssl.h>

#include <uv.h>

typedef void(*SSL_read_ready_cb)(uv_tcp_t *client, WOLFSSL *ssl);

/**
 * SSLClient connects WOLFSSL to libuv
 * 
 * My plan is to create an SSLClient on accept that links to both the WOLFSSL context and the uv_handler data to allow their respective handlers to access the SSL Client
 */
class SSLClient {
private:
    uv_tcp_t *client;
    WOLFSSL *ssl;
    std::string buffer;

    SSL_read_ready_cb rrcb = nullptr;
public:
    SSLClient(uv_tcp_t *client, WOLFSSL *ssl)
        : client(client),
          ssl(ssl) {}
    ~SSLClient();

    void setReadReadyCallback(SSL_read_ready_cb cb) { rrcb = cb; }

    /**
     * Send data to the client
     * 
     * @param buf data to send
     * @param size size of the data
     * 
     * @return amount of data sent
     */
    int send(const char *buf, size_t size);
    /**
     * Receive ready data from the client
     * 
     * @param buf buffer to write to
     * @param size maximum number of bytes to write
     * 
     * @return amount of data read
     */
    int recv(char *buf, size_t size);

    /**
     * Put data in the input buffer
     * 
     * @param buf data to receive
     * @param size size of the data
     */
    void _receive(const char *buf, size_t size);
};

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
     * @param client client to accept
     * 
     * @return the created ssl or nullptr
     */
    std::shared_ptr<WOLFSSL> accept(uv_tcp_t *client, SSL_read_ready_cb callback);

    /**
     * Get the ssl context
     */
    WOLFSSL_CTX *getContext() { return context.get(); }
};


#endif