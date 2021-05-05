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

#include "context.hpp"


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
    int queued_writes = 0;

    ClientContext *context = nullptr;

    std::map<uv_write_t*, uv_buf_t*> write_requests;

    uv_timer_t timeout;
    unsigned int timeout_time;
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
     * Get the loop that the client is using
     * 
     * @return loop
     */
    uv_loop_t *getLoop() const;

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
     * Set the time for the timeout
     * 
     * When the timer runs out, the connection will be closed. whenever data
     * is sent to the client or received from the client, the timer resets.
     * 
     * if the time is 0, then the timeout is disabled
     * 
     * @param time timeout time (ms)
     */
    void setTimeout(unsigned int time);
    /**
     * Reset the timer on the timeout
     * 
     * This is effectively like calling setTimeout() with whichever value
     * it was last called with.
     */
    void resetTimeout();

    /**
     * Read data from the client
     * 
     * @param buffer buffer to put the data
     * @param size number of bytes to read
     * 
     * @return number of bytes read, or -1 on error
     */
    int read(void *buffer, int size);
    /**
     * Write data to the client
     * 
     * @param data data to send
     * @param size number of bytes to send
     * 
     * @return number of bytes written, or -1 on error
     */
    int write(const void *data, int size);

    /**
     * Check if the server needs to wait for data
     * 
     * @return should wait for client
     */
    bool wants_read() const;
    /**
     * Check if the connection is still open
     * 
     * @return whether the connection is open
     */
    bool is_open() const;
    /**
     * Get the last wolfSSL error
     * 
     * @return the last wolfSSL error
     */
    int get_error() const;
    /**
     * Get the last wolfSSL error string
     * 
     * @return the last wolfSSL error string
     */
    std::string get_error_string() const;

    /**
     * Close the connection
     * 
     * This will close the connection after any queued writes have been sent.
     */
    void close();
    /**
     * Crash the connection
     * 
     * reset and close the connection
     */
    void crash();

    /**
     * set the context for the client
     *
     * @param context context to set
     */
    void setContext(ClientContext *context) { this->context = context; }

    /**
     * Get the context
     * 
     * @return context
     */
    ClientContext *getContext() const { return context; }

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
     * Put data in the input buffer
     * 
     * @param read size of the data
     * @param buf data to receive
     */
    void _on_receive(ssize_t read, const uv_buf_t *buf);
    /**
     * Notify that data has been written to the stream
     * 
     * @param req write request
     * @param status 0 in case of success, < 0 otherwise
     */
    void _on_write(uv_write_t *req, int status);

    /**
     * Called after the client has been closed
     */
    void _on_close();
};

typedef void(*SSL_ready_cb)(SSLClient *client);

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
protected:
    /**
     * Notify that a client is being destroyed
     */
    void _notify_death(SSLClient *client);

    friend SSLClient;
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