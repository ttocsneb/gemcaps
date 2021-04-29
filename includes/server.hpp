#ifndef __GEMCAPS_SERVER__
#define __GEMCAPS_SERVER__

#include <string>
#include <sstream>
#include <memory>
#include <map>
#include <set>

#include <wolfssl/options.h>
#include <wolfssl/ssl.h>

#include <uv.h>


class SSLClient;

typedef void(*SSL_ready_cb)(SSLClient *client, void *ctx);

/**
 * SSLClient connects WOLFSSL to libuv
 */
class SSLClient {
private:

    uv_tcp_t *client;
    WOLFSSL *ssl;
    char *buffer = nullptr;
    int buffer_size = 0;
    int buffer_len = 0;

    SSL_ready_cb rrcb = nullptr;
    void *rrctx = nullptr;
    SSL_ready_cb wrcb = nullptr;
    void *wrctx = nullptr;
    SSL_ready_cb ccb = nullptr;
    void *cctx = nullptr;

    std::map<uv_write_t*, uv_buf_t*> write_requests;
public:
    /**
     * Create the SSL client
     * 
     * The client will be paired with the ssl
     * 
     * @param client tcp client
     * @param ssl ssl client
     */
    SSLClient(uv_tcp_t *client, WOLFSSL *ssl);
    ~SSLClient();

    /**
     * Check if the buffer has more data available
     * 
     * @return whether there is more data
     */
    bool hasData() const { return buffer_len > 0; }

    /**
     * Start listening for data
     */
    void listen();
    /**
     * Stop listening for data
     */
    void stop_listening();

    /**
     * Set the read ready callback
     * 
     * This is called when data is ready to read
     * 
     * @param cb callback
     * @param ctx context
     */
    void setReadReadyCallback(SSL_ready_cb cb, void *ctx = nullptr) { rrcb = cb; rrctx = ctx;}
    /**
     * Set the write ready callback
     * 
     * This is called when data has been written
     * 
     * @param cb callback
     * @param ctx context
     */
    void setWriteReadyCallback(SSL_ready_cb cb, void *ctx = nullptr) { wrcb = cb; wrctx = ctx; }
    /**
     * Set the close callback
     * 
     * This is called when an error occured while reading
     * 
     * @param cb callback
     * @param ctx context
     */
    void setCloseCallback(SSL_ready_cb cb, void *ctx = nullptr) { ccb = cb; cctx = ctx; }

    /**
     * Send data to the client
     * 
     * @param buf data to send
     * @param size size of the data
     * 
     * @return amount of data sent
     */
    int _send(const char *buf, size_t size);
    /**
     * Receive ready data from the client
     * 
     * @param buf buffer to write to
     * @param size maximum number of bytes to write
     * 
     * @return amount of data read
     */
    int _recv(char *buf, size_t size);

    /**
     * Get the ssl connection
     * 
     * @return the ssl connection
     */
    WOLFSSL *getSSL() { return ssl; }

    /**
     * Put data in the input buffer
     * 
     * @param read size of the data
     * @param buf data to receive
     */
    void _receive(ssize_t read, const uv_buf_t *buf);
    /**
     * Notify that data has been written to the stream
     * 
     * @param req write request
     * @param status 0 in case of success, < 0 otherwise
     */
    void _write(uv_write_t *req, int status);
};

/**
 * A WolfSSL server wrapper
 */
class SSLServer {
private:
    WOLFSSL_CTX *context = nullptr;
    uv_tcp_t *server = nullptr;

    SSL_ready_cb accept_cb = nullptr;
    void *accept_ctx = nullptr;

    std::set<SSLClient*> clients;
public:
    ~SSLServer();
    /**
     * Load the ssl certificates
     * 
     * @param loop uv loop
     * @param host host name to listen on
     * @param port port to listen on
     * @param cert certificate file
     * @param key key file
     * 
     * @return whether the load was successful
     */
    bool load(uv_loop_t *loop, const std::string &host, int port, const std::string &cert, const std::string &key);

    /**
     * Set the accept callback function
     * 
     * it is called when a new connection has been accepted
     * 
     * @param cb callback
     * @param ctx context
     */
    void setAcceptCallback(SSL_ready_cb cb, void *ctx = nullptr) { accept_cb = cb; accept_ctx = ctx; }

    /**
     * Check whether the server is loaded
     * 
     * @return whether the server is loaded
     */
    bool isLoaded() const;

    /**
     * Accept a client connection, and notify the accept callback
     * 
     * @param conn client connection
     */
    void _accept(uv_tcp_t *conn);
};


#endif