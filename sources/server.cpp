#include "server.hpp"

#include <iostream>
#include <sstream>
#include <string.h>

using std::string;
using std::shared_ptr;
using std::make_unique;
using std::make_shared;
using std::stringstream;

using std::cerr;
using std::endl;

struct WOLFSSL_deleter {
    void operator ()(WOLFSSL *p) {
        // ClientContext *context = static_cast<ClientContext *>(wolfSSL_GetIOReadCtx(p));
        // delete context;
        wolfSSL_free(p);
    }
};

struct WOLFSSL_CTX_deleter {
    void operator ()(WOLFSSL_CTX *p) {
        wolfSSL_CTX_free(p);
    }
};

int SSLClient::send(const char *buf, size_t size) {
    // TODO write data to the client
}

int SSLClient::recv(char *buf, size_t size) {
    string read = buffer.substr(0, size);
    buffer = buffer.substr(size);
    int size = read.length();
    strncpy(buf, read.c_str(), size);

    // TODO return errors
    return size;
}

void SSLClient::_receive(const char *buf, size_t size) {
    buffer.append(buf, size);
    rrcb(client, ssl);
}

int CIORecv(WOLFSSL *ssl, char *buf, int size, void *ctx) {
    // ClientContext *context = static_cast<ClientContext *>(ctx);
}

int CIOSend(WOLFSSL *ssl, char *buf, int size, void *ctx) {
    // ClientContext *context = static_cast<ClientContext *>(ctx);
}

bool SSLServer::load(const string &cert, const string &key) {
    WOLFSSL_METHOD *method;

    method = wolfTLSv1_3_server_method();

    context = make_unique<WOLFSSL_CTX>(wolfSSL_CTX_new(method), WOLFSSL_CTX_deleter());
    if (context == nullptr) {
        return false;
    }

    // Load the certificates
    if (wolfSSL_CTX_use_certificate_file(context.get(), cert.c_str(), SSL_FILETYPE_PEM) != SSL_SUCCESS) {
        cerr << "Error [SSLLoad] Could not load certificate file: " << cert << endl;
        return false;
    }
    if (wolfSSL_CTX_use_PrivateKey_file(context.get(), key.c_str(), SSL_FILETYPE_PEM) != SSL_SUCCESS) {
        cerr << "Error [SSLLoad] Could not load private key file: " << key << endl;
        return false;
    }

    wolfSSL_CTX_SetIOSend(context.get(), CIOSend);
    wolfSSL_CTX_SetIORecv(context.get(), CIORecv);

    return true;
}

bool SSLServer::isLoaded() const {
    return context != nullptr;
}

shared_ptr<WOLFSSL> SSLServer::accept(uv_tcp_t *client) {
    std::shared_ptr<WOLFSSL> ssl = make_shared<WOLFSSL>(wolfSSL_new(context.get()), WOLFSSL_deleter());
    if (ssl == nullptr) {
        return nullptr;
    }
    // ClientContext *context = new ClientContext;
    // context->client = client;

    // wolfSSL_SetIOReadCtx(ssl.get(), context);
    // wolfSSL_SetIOWriteCtx(ssl.get(), context);

    int ret = wolfSSL_accept_TLSv13(ssl.get());
    if (ret != SSL_SUCCESS) {
        cerr << "Error [SSLAccept] wolfSSL accept error: " << wolfSSL_get_error(ssl.get(), ret) << endl;
        return nullptr;
    }

    return ssl;
}
