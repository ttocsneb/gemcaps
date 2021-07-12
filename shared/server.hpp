#ifndef __GEMCAPS_SHARED_SERVER__
#define __GEMCAPS_SHARED_SERVER__

#include <memory>

#include <wolfssl/options.h>
#include <wolfssl/ssl.h>

#include <uv.h>

#include <parallel_hashmap/phmap.h>

#include "util.hpp"


class SSLServer;
class ClientContext;


class ClientContext {
public:
    virtual void onClose() = 0;
    virtual void onRead() = 0;
    virtual void onWrite() = 0;
    virtual void onTimeout() = 0;
};


class SSLClient {
private:
    uv_tcp_t *client;
    uv_timer_t *timeout;
    WOLFSSL *ssl;

    BufferPipe buffer;
    
    size_t queued_writes = 0;
    bool queued_close = false;
    bool closing = false;
    bool destroying = false;

    SSLServer *server;
    
    ClientContext *context = nullptr;

    unsigned long timeout_time;

    static int __send(WOLFSSL *ssl, char *buf, int size, void *ctx) noexcept;
    static int __recv(WOLFSSL *ssl, char *buf, int size, void *ctx) noexcept;

    static void __on_recv(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) noexcept;
    static void __on_send(uv_write_t *req, int status) noexcept;
    static void __on_close(uv_handle_t *handle) noexcept;
    static void __on_timeout(uv_timer_t *timeout) noexcept;

    phmap::flat_hash_map<uv_write_t *, uv_buf_t> write_requests;
protected:
    SSLClient(SSLServer *server, uv_tcp_t *client, WOLFSSL *ssl);

    friend SSLServer;
public:
    ~SSLClient() noexcept;

    uv_loop_t *getLoop() const noexcept;

    bool hasData() const noexcept { return buffer.ready() > 0; }

    void listen() noexcept;
    void stop_listening() noexcept;

    void setTimeout(unsigned long time) noexcept;
    void resetTimeout() noexcept;

    int read(size_t size, void *buffer) noexcept;
    int write(const void *data, size_t size) noexcept;

    bool wants_read() const noexcept;
    bool is_open() const noexcept;
    int get_error() const noexcept;

    std::string get_error_string() const noexcept;

    void close() noexcept;
    void crash() noexcept;

    void setContext(ClientContext *context) noexcept;
    ClientContext *getContext() const noexcept { return context; }
};

class ServerContext {
public:
    virtual void on_accept(SSLClient *client) = 0;
};

class SSLServer {
private:
    WOLFSSL_CTX *wolfssl = nullptr;
    uv_tcp_t *server = nullptr;
    
    std::shared_ptr<ServerContext> context;

    phmap::flat_hash_set<SSLClient *> clients;

    static void __on_accept(uv_tcp_t *conn);
protected:
    void _on_client_close(SSLClient *client);

    friend SSLClient;
public:
    SSLServer(std::shared_ptr<ServerContext> context)
        : context(context) {}

    ~SSLServer();

    void load(uv_loop_t *loop, const std::string &host, int port, const std::string &cert, const std::string &key);

    bool isLoaded() const noexcept;
};

#endif