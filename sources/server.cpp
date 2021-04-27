#include "server.hpp"

#include <iostream>
#include <sstream>
#include <string.h>

#include "main.hpp"

using std::string;
using std::shared_ptr;
using std::make_unique;
using std::make_shared;
using std::stringstream;

using std::cerr;
using std::endl;

void cb_uv_read(uv_stream_t *stream, ssize_t read, const uv_buf_t *buf) {
    // Data is ready to be read from the stream
    SSLClient *client = static_cast<SSLClient *>(uv_handle_get_data((uv_handle_t *)stream));
    if (client) {
        client->_receive(read, buf);
    }
    // I think we are supposed to free the buffer ourselves, but bugs may arise from here
    delete[] buf->base;
}

void cb_uv_write(uv_write_t *req, int status) {
    SSLClient *client = static_cast<SSLClient *>(uv_handle_get_data((uv_handle_t *)req->handle));
    // Data is ready to be written to the stream
    if (client) {
        client->_write(status);
    }
}

void cb_uv_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    // Allocate the buffer
    buf->base = new char[suggested_size];
    buf->len = suggested_size;
}

int cb_IORecv(WOLFSSL *ssl, char *buf, int size, void *ctx) {
    SSLClient *client = static_cast<SSLClient *>(ctx);
    if (!client) {
        return -1;
    }
    return client->_recv(buf, size);
}

int cb_IOSend(WOLFSSL *ssl, char *buf, int size, void *ctx) {
    SSLClient *client = static_cast<SSLClient *>(ctx);
    if (!client) {
        return -1;
    }
    return client->_send(buf, size);
}

SSLClient::SSLClient(uv_tcp_t *client, WOLFSSL *ssl)
        : client(client),
          ssl(ssl) {
    wolfSSL_SetIOReadCtx(ssl, this);
    wolfSSL_SetIOWriteCtx(ssl, this);
    uv_handle_set_data((uv_handle_t *)client, this);
}

SSLClient::~SSLClient() {
    wolfSSL_free(ssl);
    uv_tcp_close_reset(client, delete_handle);
    ssl = nullptr;
    client = nullptr;
}

void SSLClient::listen() {
    uv_read_start((uv_stream_t *)client, cb_uv_alloc, cb_uv_read);
}

void SSLClient::stop_listening() {
    uv_read_stop((uv_stream_t *)client);
}

int SSLClient::_send(const char *buf, size_t size) {
    if (writing) {
        return EAGAIN;
    }

    // Make sure that the writing buffer has enough space
    if (size > write_buf.len) {
        if (write_buf.base != nullptr) {
            delete[] write_buf.base;
        }
        write_buf.base = new char[size];
    }
    // Copy the buffer to the writing buffer
    write_buf.len = size;
    strncpy(write_buf.base, buf, size);

    writing = true;
    uv_write(&write_req, (uv_stream_t *)client, &write_buf, 1, cb_uv_write);

    // TODO return errors
    return size;
}

int SSLClient::_recv(char *buf, size_t size) {
    if (buffer.empty()) {
        return EAGAIN;
    }
    string read = buffer.substr(0, size);
    buffer = buffer.substr(size);
    int len = read.length();
    strncpy(buf, read.c_str(), len);

    // TODO return errors
    return len;
}

void SSLClient::_receive(ssize_t read, const uv_buf_t *buf) {
    if (read < 0) {
        uv_read_stop((uv_stream_t *)client);
        if (ccb) {
            ccb(this, cctx);
        }
        return;
    }
    if (read > 0) {
        buffer.append(buf->base, buf->len);
    }
    if (rrcb) {
        rrcb(this, rrctx);
    }
}

void SSLClient::_write(int status) {
    writing = false;
    if (wrcb) {
        wrcb(this, wrctx);
    }
}

////////////////// SSLServer //////////////////

void new_connection(uv_stream_t *stream, int status) {
    if (status < 0) {
        cerr << "Error while accepting new connection: " << uv_strerror(status) << endl;
        return;
    }
    SSLServer *server = static_cast<SSLServer *>(uv_handle_get_data((uv_handle_t *)stream));

    uv_tcp_t *conn = new uv_tcp_t;
    uv_tcp_init(uv_handle_get_loop((uv_handle_t *)stream), conn);
    if (uv_accept(stream, (uv_stream_t *)conn) != 0) {
        uv_close((uv_handle_t *)conn, delete_handle);
        return;
    }
    server->_accept(conn);
}

SSLServer::~SSLServer() {
    if (context != nullptr) {
        wolfSSL_CTX_free(context);
    }
    if (server != nullptr) {
        uv_close((uv_handle_t *)server, delete_handle);
    }
}

bool SSLServer::load(uv_loop_t *loop, const string &host, int port, const string &cert, const string &key) {
    if (context != nullptr) {
        wolfSSL_CTX_free(context);
    }

    WOLFSSL_METHOD *method = wolfTLSv1_3_server_method();

    context = wolfSSL_CTX_new(method);
    if (context == nullptr) {
        return false;
    }

    // Load the certificates
    if (wolfSSL_CTX_use_certificate_file(context, cert.c_str(), SSL_FILETYPE_PEM) != SSL_SUCCESS) {
        cerr << "Error [SSLLoad] Could not load certificate file: " << cert << endl;
        return false;
    }
    if (wolfSSL_CTX_use_PrivateKey_file(context, key.c_str(), SSL_FILETYPE_PEM) != SSL_SUCCESS) {
        cerr << "Error [SSLLoad] Could not load private key file: " << key << endl;
        return false;
    }

    wolfSSL_CTX_SetIOSend(context, cb_IOSend);
    wolfSSL_CTX_SetIORecv(context, cb_IORecv);

    if (server != nullptr) {
        uv_close((uv_handle_t *)server, delete_handle);
    }
    server = new uv_tcp_t;
    uv_tcp_init(loop, server);
    uv_handle_set_data((uv_handle_t *)server, this);

    sockaddr_in addr;
    uv_ip4_addr(host.c_str(), port, &addr);
    int r = uv_listen((uv_stream_t *)server, 5, new_connection);
    if (r) {
        cerr << "Listen Error: " << uv_strerror(r) << endl;
        return false;
    }

    return true;
}

bool SSLServer::isLoaded() const {
    return context != nullptr;
}

void SSLServer::_accept(uv_tcp_t *conn) {
    if (!accept_cb) {
        cerr << "Error accepting connection: No accept callback has been defined" << endl;
        return;
    }
    WOLFSSL *ssl = wolfSSL_new(context);
    if (ssl == nullptr) {
        return;
    }
    accept_cb(new SSLClient(conn, ssl), accept_ctx);
}
