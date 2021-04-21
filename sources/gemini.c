#include "gemini.h"

#include <benstools/string.h>

gemini_request gemini_read_header(WOLFSSL *ssl) {
    char buf[1025];
    char buf2[1025];
    gemini_request request;
    memset(&request, 0, sizeof(request));
    request.request = bt_string_new(1024);
    int read = wolfSSL_read(ssl, request.request.str, 1024);
    if (read < 0) {
        return request;
    }
    request.request.str[read] = '\0';

    char host[128] = "";
    char path[1024] = "";
    char query[1024] = "";

    sscanf(request.request.str, "gemini://%127[^:/?\r\n]%1024[^\r\n]", host, buf);
    if (buf[0] == ':') {
        scanf(buf, ":%d%1024[^\r\n]", &request.port, buf2);
        strcpy(buf, buf2);
    } else {
        request.port = 1965;
    }
    if (buf[0] == '/') {
        scanf(buf, "%1023[^?\r\n]%1024[^\r\n]", path, buf2);
        strcpy(buf, buf2);
    }
    if (buf[0] == '?') {
        strcpy(query, buf);
    }

    request.host = bt_string_create(host);
    request.path = bt_string_create(path);
    request.query = bt_string_create(query);

    return request;
}

void gemini_request_destroy(gemini_request *request) {
    if (request->request.str != NULL) bt_string_destroy(&request->request);
    if (request->host.str != NULL) bt_string_destroy(&request->host);
    if (request->path.str != NULL) bt_string_destroy(&request->path);
    if (request->query.str != NULL) bt_string_destroy(&request->query);
}