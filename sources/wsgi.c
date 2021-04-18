#include "wsgi.h"

#include <benstools/string.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/**
 * Send the request to a file
 * 
 * @param f file to send to
 * @param request request to send
 */
void send_request(FILE *f, wsgi_request *request) {
    fprintf(f, "id: %d\n", request->id);
    fprintf(f, "ip: %s\n", request->ip);
    fprintf(f, "port: %d\n", request->port);
    fprintf(f, "request: %s\n\n", request->request);
}

void _apply_header(wsgi_response *response, bt_string *key, bt_string *value) {
    if (strcmp(key->str, "id") == 0) {
        response->id = atoi(value->str);
        return;
    }
    if (strcmp(key->str, "response") == 0) {
        response->response = atoi(value->str);
        return;
    }
    if (strcmp(key->str, "meta") == 0) {
        strcpy(response->meta, value->str);
        return;
    }
    if (strcmp(key->str, "cache-time") == 0) {
        response->cache_time = atoi(value->str);
        return;
    }
    if (strcmp(key->str, "size") == 0) {
        response->size = atoi(value->str);
        return;
    }
}

/**
 * Parse the response headers from the file
 * 
 * @param f file to read from
 * 
 * @return the parsed response headers
 */
wsgi_response parse_response(FILE *f) {
    wsgi_response response;
    memset(&response, 0, sizeof(response));

    char buf[2048];
    while (TRUE) {
        fscanf(f, "%2048[^\n]\n", buf);
        if (strlen(buf) == 0) {
            break;
        }

        int pos = bt_string_find(buf, ":");

        if (pos > 0) {
            bt_string key = bt_string_sub(buf, 0, pos);
            bt_string value = bt_string_sub(buf, pos + 1, -1);
            bt_string_strip(&key);
            bt_string_strip(&value);
            bt_string_lower(key.str);

            _apply_header(&response, &key, &value);

            bt_string_destroy(&key);
            bt_string_destroy(&value);
        }
    } 

    return response;
}


/**
 * Read the response body from the file
 * 
 * @param f file to read from
 * @param response settings to use
 * 
 * @return the created response body
 */
bt_string read_response(FILE *f, wsgi_response *response) {

    // Initialize the body string
    int len = response->size;
    if (len <= 0) {
        len = 2048;
    }
    bt_string body = bt_string_new(len);

    if (response->size <= 0) {
        // Read until the body has read the declared number of characters
        while (bt_string_len(&body) < response->size) {
            int c = fgetc(f);
            if (c == EOF) {
                return body;
            }
            bt_string_addc(&body, c);
        }
        // Dump any excess characters
        int c;
        do {
            c = fgetc(f);
        } while (c != EOF && c != '\0');
        return body;
    }
    // The size is not given
    // Read until a null character is read
    int c;
    while (TRUE) {
        c = fgetc(f);
        if (c == EOF || c == '\0') {
            return body;
        }
        bt_string_addc(&body, c);
    }

    return body;
}