#include "server.hpp"

#include <iostream>

using std::string;
using std::shared_ptr;
using std::make_unique;
using std::make_shared;

using std::cerr;
using std::endl;

struct WOLFSSL_deleter {
    void operator ()(WOLFSSL *p) {
        wolfSSL_free(p);
    }
};

struct WOLFSSL_CTX_deleter {
    void operator ()(WOLFSSL_CTX *p) {
        wolfSSL_CTX_free(p);
    }
};

bool SSLServer::load( const string &cert, const string &key) {
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

    return true;
}

bool SSLServer::isLoaded() const {
    return context != nullptr;
}

shared_ptr<WOLFSSL> SSLServer::accept(int socketfd) {
    std::shared_ptr<WOLFSSL> ssl = make_shared<WOLFSSL>(wolfSSL_new(context.get()), WOLFSSL_deleter());
    if (ssl == nullptr) {
        return nullptr;
    }

    wolfSSL_set_fd(ssl.get(), socketfd);

    int ret = wolfSSL_accept_TLSv13(ssl.get());
    if (ret != SSL_SUCCESS) {
        cerr << "Error [SSLAccept] wolfSSL accept error: " << wolfSSL_get_error(ssl.get(), ret) << endl;
        return nullptr;
    }

    return ssl;
}
