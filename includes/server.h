#ifndef __GEMCAPS_SERVER__
#define __GEMCAPS_SERVER__

#include <wolfssl/options.h>
#include <wolfssl/ssl.h>

#include <benstools.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Bind the server to a port and load the certificate and private key.
 * 
 * Dont forget to call wolfSSL_CTX_free when done.
 * 
 * @param port port to use
 * @param cert_file certificate file
 * @param key_file private key file
 * 
 * @return the created context or NULL
 */
WOLFSSL_CTX *server_bind(char *port, char *cert_file, char *key_file);

/**
 * Accept an incoming request.
 * 
 * Dont forget to call wolfSSL_free when done.
 * 
 * @param ctx ssl context to use
 * @param socket incoming client file descriptor
 * 
 * @return the created SSL connection
 */
WOLFSSL *server_accept(WOLFSSL_CTX *ctx, int socket);



#ifdef __cplusplus
}
#endif
#endif