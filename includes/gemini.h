#ifndef __GEMCAPS_GEMINI__
#define __GEMCAPS_GEMINI__

#include <wolfssl/options.h>
#include <wolfssl/ssl.h>
#include <benstools/string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bt_string host;
    int port;
    bt_string path;
    bt_string query;
    bt_string request;
} gemini_request;

/**
 * read the gemini header into a gemini_request object
 * 
 * @param ssl SSL Connection
 * 
 * @return the gemini_request object
 */
gemini_request gemini_read_header(WOLFSSL *ssl);

/**
 * Destroy the gemini request
 * 
 * @param request
 */
void gemini_request_destroy(gemini_request *request);


#ifdef __cplusplus
}
#endif
#endif