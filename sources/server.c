#include "server.h"

#include <stdlib.h>

#include <benstools/logging.h>

#include <wolfssl/options.h>


WOLFSSL_CTX *server_bind(char *port, char *cert_file, char *key_file) {
    bt_log_buf_setup(1024);
    int port_num = atoi(port);

    WOLFSSL_METHOD *method;

    method = wolfTLSv1_3_server_method();

    WOLFSSL_CTX *ctx = wolfSSL_CTX_new(method);
    if (ctx == NULL) {
        bt_log_error("server_bind", "wolfSSL context error");
        return NULL;
    }

    // Load the certificates
    if (wolfSSL_CTX_use_certificate_file(ctx, cert_file, SSL_FILETYPE_PEM) != SSL_SUCCESS) {
        bt_log_errorf("server_bind", "error loading cert file '%s'", cert_file);
        wolfSSL_CTX_free(ctx);
        return NULL;
    }
    if (wolfSSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM) != SSL_SUCCESS) {
        bt_log_errorf("server_bind", "error loading key file '%s'", key_file);
        wolfSSL_CTX_free(ctx);
        return NULL;
    }

    return ctx;
}

WOLFSSL *server_accept(WOLFSSL_CTX *ctx, int socket) {
    WOLFSSL *ssl = wolfSSL_new(ctx);
    if (ssl == NULL) {
        bt_log_error("server_accept", "wolfSSL creation error");
        return NULL;
    }

    wolfSSL_set_fd(ssl, socket);

    return ssl;
}