#include "server.hpp"

#include <iostream>
#include <sstream>
#include <string.h>

#include <wolfssl/error-ssl.h>

#include "main.hpp"

using std::string;
using std::shared_ptr;
using std::make_unique;
using std::make_shared;
using std::stringstream;

using std::cout;
using std::cerr;
using std::endl;

void delete_handle(uv_handle_t *handle) {
    delete handle;
}

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
        client->_write(req, status);
    }

}

void cb_uv_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    // Allocate the buffer
    buf->base = new char[suggested_size];
    buf->len = suggested_size;
}

void cb_uv_close(uv_handle_t *handle) {
    SSLClient *client = static_cast<SSLClient *>(uv_handle_get_data(handle));
    if (client) {
        client->_close();
    }
}

int cb_IORecv(WOLFSSL *ssl, char *buf, int size, void *ctx) {
    SSLClient *client = static_cast<SSLClient *>(ctx);
    if (!client) {
        return WOLFSSL_CBIO_ERR_GENERAL;
    }
    return client->_recv(buf, size);
}

int cb_IOSend(WOLFSSL *ssl, char *buf, int size, void *ctx) {
    SSLClient *client = static_cast<SSLClient *>(ctx);
    if (!client) {
        return WOLFSSL_CBIO_ERR_GENERAL;
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
    if (client) {
        uv_unref((uv_handle_t *)client);
        uv_close((uv_handle_t *)client, delete_handle);
    }
    ssl = nullptr;
    client = nullptr;
    if (buffer != nullptr) {
        delete[] buffer;
        buffer = nullptr;
    }
    for (auto &pair : write_requests) {
        delete[] pair.second->base;
        delete pair.second;
        delete pair.first;
    }
}

uv_loop_t *SSLClient::getLoop() const {
    return uv_handle_get_loop((uv_handle_t *)client);
}

void SSLClient::listen() {
    uv_read_start((uv_stream_t *)client, cb_uv_alloc, cb_uv_read);
}

void SSLClient::stop_listening() {
    uv_read_stop((uv_stream_t *)client);
}

int SSLClient::read(void *buffer, int size) {
    return wolfSSL_read(ssl, buffer, size);
}

int SSLClient::write(void *data, int size) {
    return wolfSSL_write(ssl, data, size);
}

void SSLClient::close() {
    if (client) {
        uv_unref((uv_handle_t *)client);
        uv_close((uv_handle_t *)client, cb_uv_close);
    }
}

bool SSLClient::wants_read() const {
    return wolfSSL_want_read(ssl);
}

bool SSLClient::is_open() const {
    return client != nullptr;
}

int SSLClient::get_error() const {
    return wolfSSL_get_error(ssl, 0);
}

string SSLClient::get_error_string() const {
    char buf[80];
    wolfSSL_ERR_error_string(wolfSSL_get_error(ssl, 0), buf);
    return string(buf);
}

int SSLClient::_send(const char *buf, size_t size) {
    if (!is_open()) {
        return WOLFSSL_CBIO_ERR_CONN_CLOSE;
    }
    char *msg = new char[size];
    memcpy(msg, buf, size);

    // Make sure that the writing buffer has enough space
    uv_buf_t *write_buf = new uv_buf_t;
    write_buf->base = new char[size];
    memcpy(write_buf->base, buf, size);
    write_buf->len = size;

    uv_write_t *write_req = new uv_write_t;
    uv_write(write_req, (uv_stream_t *)client, write_buf, 1, cb_uv_write);
    write_requests.insert_or_assign(write_req, write_buf);

    return size;
}


int SSLClient::_recv(char *buf, size_t size) {
    if (buffer_len == 0) {
        if (!is_open()) {
            return WOLFSSL_CBIO_ERR_CONN_CLOSE;
        }
        return WOLFSSL_CBIO_ERR_WANT_READ;
    }
    int len = size;
    if (buffer_len < len) {
        len = buffer_len;
    }
    memcpy(buf, buffer, len);
    buffer_len = buffer_len - len;
    if (buffer_len > 0) {
        memcpy(buffer, buffer + len, buffer_len);
    }

    return len;
}

void SSLClient::_close() {
    delete client;
    client = nullptr;
    if (ccb) {
        ccb(this);
    }
}

void SSLClient::_receive(ssize_t read, const uv_buf_t *buf) {
    if (read < 0) {
        close();
        return;
    }
    if (read > 0) {
        if (read + buffer_len > buffer_size) {
            buffer_size = read + buffer_len;
            char *new_buf = new char[buffer_size];
            if (buffer != nullptr) {
                memcpy(new_buf, buffer, buffer_len);
                delete[] buffer;
            }
            buffer = new_buf;
        }
        memcpy(buffer + buffer_len, buf->base, read);
        buffer_len += read;
    }
    if (rrcb) {
        do {
            rrcb(this);
        } while (hasData() && !wolfSSL_want_read(ssl));
    }
}

void SSLClient::_write(uv_write_t *req, int status) {
    uv_buf_t *buf = write_requests.at(req);
    delete[] buf->base;
    delete buf;
    write_requests.erase(req);
    delete req;
    if (wrcb) {
        wrcb(this);
    }
    write_requests.erase(req);
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

void default_close_handler(SSLClient *client) {
    delete client;
}

void SSLServer::_notify_death(SSLClient *client) {
    clients.erase(client);
}

SSLServer::~SSLServer() {
    if (context != nullptr) {
        wolfSSL_CTX_free(context);
    }
    if (server != nullptr) {
        uv_close((uv_handle_t *)server, delete_handle);
    }
    for (SSLClient *client : clients) {
        delete client;
    }
}

bool SSLServer::load(uv_loop_t *loop, const string &host, int port, const string &cert, const string &key) {
    if (context != nullptr) {
        wolfSSL_CTX_free(context);
    }

    context = wolfSSL_CTX_new(wolfSSLv23_server_method());
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
    uv_tcp_bind(server, (const struct sockaddr *)&addr, 0);
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
    SSLClient *client = new SSLClient(conn, ssl);
    client->setCloseCallback(default_close_handler);
    accept_cb(client);
    client->listen();
}