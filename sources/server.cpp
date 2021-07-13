#include "server.hpp"

#include <iostream>

#include "uvutils.hpp"

using std::cerr;
using std::endl;


ReusableAllocator<uv_tcp_t> tcp_allocator;

void on_timeout_close(uv_handle_t *handle) {
    timer_allocator.deallocate((uv_timer_t *)handle);
}

void on_tcp_close(uv_handle_t *handle) {
    tcp_allocator.deallocate((uv_tcp_t *)handle);
}

////////////////////////////////////////////////////////////////////////////////
//
// Client
//
////////////////////////////////////////////////////////////////////////////////

int SSLClient::_send(const char *buf, int size) noexcept {
    resetTimeout();

    uv_buf_t buffer = buffer_allocate();

    int len = size > buffer.len ? buffer.len : size;
    memcpy(buffer.base, buf, len);
    buffer.len = len;

    uv_write_t *req = write_req_allocator.allocate();
    write_requests.insert_or_assign(req, buffer);
    ++queued_writes;

    int written = uv_try_write((uv_stream_t *)client, &buffer, 1);
    if (written > 0) {
        req->handle = (uv_stream_t *)client;
        req->cb = __on_send;
        __on_send(req, written);
        return written;
    }
    int res = uv_write(req, (uv_stream_t *)client, &buffer, 1, __on_send);
    return len;
}

int SSLClient::_recv(int size, char *buf) noexcept {
    return buffer.read(size, buf);
}

int SSLClient::_ready() const noexcept {
    return buffer.ready();
}

void alloc_recv_buf(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    uv_buf_t alloc = buffer_allocate();
    buf->base = alloc.base;
    buf->len = alloc.len;
}

void SSLClient::__on_recv(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) noexcept {
    SSLClient *client = static_cast<SSLClient *>(stream->data);
    if (!client) {
        buffer_deallocate(*buf);
        return;
    }

    if (nread < 0) {
        client->close();
        buffer_deallocate(*buf);
        return;
    }

    client->resetTimeout();

    if (nread > 0) {
        client->buffer.write(buf->base, nread);
    }
    buffer_deallocate(*buf);

    if (client->context) {
        do {
            client->context->onRead(client);
        } while (client->is_open() && client->hasData() && !wolfSSL_want_read(client->ssl));
    }
}

void SSLClient::__on_send(uv_write_t *req, int status) noexcept {
    SSLClient *client = static_cast<SSLClient *>(req->handle->data);
    if (!client) {
        write_req_allocator.deallocate(req);
        return;
    }

    // Deallocate the buffer used in the write request
    auto found = client->write_requests.find(req);
    if (found != client->write_requests.end()) {
        buffer_deallocate(found->second);
        client->write_requests.erase(found);
    }

    if (--client->queued_writes < 0) {
        client->queued_writes = 0;
    }
    if (client->queued_writes == 0) {
        if (client->context) {
            client->context->onWrite(client);
        }
        if (client->queued_close) {
            client->close();
        }
    }

    write_req_allocator.deallocate(req);
}

void SSLClient::__on_close(uv_handle_t *handle) noexcept {
    SSLClient *client = static_cast<SSLClient *>(handle->data);
    if (!client) {
        tcp_allocator.deallocate((uv_tcp_t *)handle);
        return;
    }
    handle->data = nullptr;

    if (client->context) {
        client->context->onClose(client);
    }

    client->client = nullptr;
    tcp_allocator.deallocate((uv_tcp_t *)handle);

    client->server->_on_client_close(client);
}

void SSLClient::__on_timeout(uv_timer_t *timeout) noexcept {
    SSLClient *client = static_cast<SSLClient *>(timeout->data);
    if (!client) {
        timer_allocator.deallocate(timeout);
        return;
    }

    client->crash();
}

SSLClient::SSLClient(SSLServer *server, uv_tcp_t *client, WOLFSSL *ssl)
        : server(server),
          client(client),
          ssl(ssl),
          timeout(timer_allocator.allocate()) {
    uv_timer_init(client->loop, timeout);
    client->data = this;
    timeout->data = this;
}

SSLClient::~SSLClient() noexcept {
    wolfSSL_free(ssl);
    if (!closing) {
        crash();
    }
    uv_close((uv_handle_t *)timeout, on_timeout_close);
    // Doing this may cause an error in the event that the client is deleted while in the middle of a write
    for (auto pair : write_requests) {
        buffer_deallocate(pair.second);
        write_req_allocator.deallocate(pair.first);
    }
}

uv_loop_t *SSLClient::getLoop() const noexcept {
    if (!client) {
        return nullptr;
    }
    return client->loop;
}

void SSLClient::listen() noexcept {
    uv_read_start((uv_stream_t *)client, alloc_recv_buf, __on_recv);
}

void SSLClient::stop_listening() noexcept {
    uv_read_stop((uv_stream_t *)client);
}

void SSLClient::setTimeout(unsigned long time) noexcept {
    timeout_time = time;
    uv_timer_stop(timeout);
    if (time == 0) {
        return;
    }
    uv_timer_start(timeout, __on_timeout, time, 0);
}

void SSLClient::resetTimeout() noexcept {
    if (timeout_time == 0) {
        return;
    }
    uv_timer_stop(timeout);
    uv_timer_start(timeout, __on_timeout, timeout_time, 0);
}

int SSLClient::read(size_t size, void *buffer) noexcept {
    return wolfSSL_read(ssl, buffer, size);
}

int SSLClient::write(const void *data, size_t size) noexcept {
    return wolfSSL_write(ssl, data, size);
}

bool SSLClient::wants_read() const noexcept {
    return wolfSSL_want_read(ssl);
}

bool SSLClient::is_open() const noexcept {
    return client != nullptr && closing != true;
}

void SSLClient::close() noexcept {
    if (queued_writes > 0 && !queued_close) {
        queued_close = true;
        return;
    }
    closing = true;
    queued_close = false;
    uv_timer_stop(timeout);
    if (client) {
        uv_close((uv_handle_t *)client, __on_close);
    }
}

void SSLClient::crash() noexcept {
    queued_close = false;
    closing = true;
    uv_timer_stop(timeout);
    if (client) {
        uv_tcp_close_reset(client, __on_close);
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// Server
//
////////////////////////////////////////////////////////////////////////////////

void SSLServer::__on_accept(uv_stream_t *stream, int status) noexcept {
    SSLServer *server = static_cast<SSLServer *>(stream->data);
    if (!server) {
        return;
    }

    uv_tcp_t *conn = tcp_allocator.allocate();
    uv_tcp_init(stream->loop, conn);
    if (uv_accept(stream, (uv_stream_t *)conn) != 0) {
        uv_close((uv_handle_t *)conn, on_tcp_close);
        return;
    }

    WOLFSSL *ssl = wolfSSL_new(server->wolfssl);
    SSLClient *client = *server->clients.insert(new SSLClient(server, conn, ssl)).first;
    wolfSSL_SetIOReadCtx(ssl, client);
    wolfSSL_SetIOWriteCtx(ssl, client);
    server->context->on_accept(server, client);
}

int SSLServer::__send(WOLFSSL *ssl, char *buf, int size, void *ctx) noexcept {
    SSLClient *client = static_cast<SSLClient *>(ctx);
    if (!client) {
        return WOLFSSL_CBIO_ERR_GENERAL;
    }

    if (!client->is_open()) {
        return WOLFSSL_CBIO_ERR_CONN_CLOSE;
    }

    return client->_send(buf, size);
}

int SSLServer::__recv(WOLFSSL *ssl, char *buf, int size, void *ctx) noexcept {
    SSLClient *client = static_cast<SSLClient *>(ctx);
    if (!client) {
        return WOLFSSL_CBIO_ERR_GENERAL;
    }

    if (client->_ready() == 0) {
        if (!client->is_open()) {
            return WOLFSSL_CBIO_ERR_CONN_CLOSE;
        }
        return WOLFSSL_CBIO_ERR_WANT_READ;
    }
    return client->_recv(size, buf);

    return client->buffer.read(size, buf);
}

void SSLServer::_on_client_close(SSLClient *client) noexcept {
    auto found = clients.find(client);
    if (found == clients.end()) {
        cerr << "Warn [SSLServer::_on_client_close] An invalid pointer was passed" << endl;
        return;
    }
    clients.erase(found);
    delete client;
}


SSLServer::~SSLServer() noexcept {
    if (wolfssl != nullptr) {
        wolfSSL_CTX_free(wolfssl);
    }
    if (server != nullptr) {
        uv_close((uv_handle_t *)server, on_tcp_close);
    }
    for (SSLClient *client : clients) {
        delete client;
    }
}

void SSLServer::load(uv_loop_t *loop, const std::string &host, int port, const std::string &cert, const std::string &key) noexcept {
    if (wolfssl != nullptr) {
        wolfSSL_CTX_free(wolfssl);
    }

    wolfssl = wolfSSL_CTX_new(wolfTLSv1_2_server_method());
    if (wolfssl == nullptr) {
        return;
    }

    // Load the certificates
    if (wolfSSL_CTX_use_certificate_file(wolfssl, cert.c_str(), SSL_FILETYPE_PEM) != SSL_SUCCESS) {
        cerr << "Error [SSLServer::load] Could not load certificate file '" << cert << "'" << endl;
        wolfSSL_CTX_free(wolfssl);
        wolfssl = nullptr;
        return;
    }
    if (wolfSSL_CTX_use_PrivateKey_file(wolfssl, key.c_str(), SSL_FILETYPE_PEM) != SSL_SUCCESS) {
        cerr << "Error [SSLServer::load] Could not load key file '" << key << "'" << endl;
        wolfSSL_CTX_free(wolfssl);
        wolfssl = nullptr;
        return;
    }

    wolfSSL_CTX_SetIORecv(wolfssl, __recv);
    wolfSSL_CTX_SetIOSend(wolfssl, __send);

    if (server != nullptr) {
        uv_close((uv_handle_t *)server, on_tcp_close);
    }
    server = tcp_allocator.allocate();
    uv_tcp_init(loop, server);
    server->data = this;

    sockaddr_in addr;
    uv_ip4_addr(host.c_str(), port, &addr);
    uv_tcp_bind(server, (const sockaddr *)&addr, 0);
    int errors = uv_listen((uv_stream_t *)server, 5, __on_accept);
    if (errors != 0) {
        cerr << "Error [SSLServer::load] Could not listen on '" << host << ":" << port << ":" << endl;
        wolfSSL_CTX_free(wolfssl);
        wolfssl = nullptr;
        uv_close((uv_handle_t *)server, on_tcp_close);
        server = nullptr;
        return;
    }
}
