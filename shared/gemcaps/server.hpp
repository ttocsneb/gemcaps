#ifndef __GEMCAPS_SHARED_SERVER__
#define __GEMCAPS_SHARED_SERVER__

#include <memory>

#include <wolfssl/options.h>
#include <wolfssl/ssl.h>

#include <uv.h>

#include <parallel_hashmap/phmap.h>

#include "gemcaps/util.hpp"


class SSLServer;
class SSLClient;
class ClientContext;


class ClientContext {
public:
    virtual void on_close(SSLClient *client) = 0;
    virtual void on_read(SSLClient *client) = 0;
    virtual void on_write(SSLClient *client) = 0;
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

    static void __on_recv(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) noexcept;
    static void __on_send(uv_write_t *req, int status) noexcept;
    static void __on_close(uv_handle_t *handle) noexcept;
    static void __on_timeout(uv_timer_t *timeout) noexcept;

    phmap::flat_hash_map<uv_write_t *, std::vector<uv_buf_t>> write_requests;
protected:
    int _send(const char *buf, int size) noexcept;
    int _recv(int size, char *buf) noexcept;
    int _ready() const noexcept;

    friend SSLServer;
public:
    SSLClient(SSLServer *server, uv_tcp_t *client, WOLFSSL *ssl);
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

    void close() noexcept;
    void crash() noexcept;

    void setContext(ClientContext *context) noexcept;
    ClientContext *getContext() const noexcept { return context; }
};

class ServerContext {
public:
    virtual void on_accept(SSLServer *server, SSLClient *client) = 0;
};

class SSLServer {
private:
    WOLFSSL_CTX *wolfssl = nullptr;
    uv_tcp_t *server = nullptr;
    
    std::shared_ptr<ServerContext> context;

    phmap::flat_hash_set<SSLClient *> clients;

    static void __on_accept(uv_stream_t *stream, int status) noexcept;

    static int __send(WOLFSSL *ssl, char *buf, int size, void *ctx) noexcept;
    static int __recv(WOLFSSL *ssl, char *buf, int size, void *ctx) noexcept;
protected:
    void _on_client_close(SSLClient *client) noexcept;

    friend SSLClient;
public:
    ~SSLServer() noexcept;

    void load(uv_loop_t *loop, const std::string &host, int port, const std::string &cert, const std::string &key) noexcept;

    void listen() noexcept;

    void setContext(std::shared_ptr<ServerContext> context) { this->context = context; }

    bool isLoaded() const noexcept { return wolfssl != nullptr; }
};

#endif