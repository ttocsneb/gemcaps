#include "gsgi.h"

#include <benstools/string.h>
#include <benstools/set.h>
#include <semaphore.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

typedef struct {
    int id;         // Id of the request
    int state;      // Current state of the request
    bt_string buf;  // String buffer
    WOLFSSL *ssl;   // SSL connection
    gsgi *object;   // gsgi object this request belongs to
} gsgi_request;

void gsgi_request_destroy(gsgi_request *request) {
    bt_string_destroy(&request->buf);
    if (request->ssl != NULL) {
        wolfSSL_free(request->ssl);
        request->ssl = NULL;
    }
}

int gsgi_request_hash(gsgi_request *request) {
    int hash = request->object->pid;
    hash = hash * 31 + request->id;
    return hash;
}

/**
 * Send the request to a file
 * 
 * @param f file to send to
 * @param request request to send
 */
void send_request(FILE *f, int id, char *ip, int port, char *request) {
    fprintf(f, "id: %d\n", id);
    fprintf(f, "ip: %s\n", ip);
    fprintf(f, "port: %d\n", port);
    fprintf(f, "request: %s\n\n", request);
}

void _apply_header(gsgi_response *response, bt_string *key, bt_string *value) {
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
gsgi_response parse_response(FILE *f) {
    gsgi_response response;
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
bt_string read_response(FILE *f, gsgi_response *response) {

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

unsigned int is_setup = 0;
bt_set connections;
int tid;
bt_array gsgi_queue;
sem_t gsgi_queue_mutex;

void init_epoll_gsgi(int epfd, gsgi *object) {
    // epoll_ctl(epfd, EPOLL_CTL_ADD, object->resfd, &object);
}

void *gsgi_loop(void *args) {
    // int epfd = epoll_create1(0);

    // close(epfd);
    return NULL;
}

void gsgi_setup() {
    ++is_setup;
    if (is_setup == 1) {
        return;
    }

    connections = bt_set_create(31, sizeof(gsgi_request));
    // sem_init(&gsgi_queue_mutex, FALSE, 1);
    gsgi_queue = bt_array_create(sizeof(gsgi), 5);

    // TODO create the gsgi loop thread
}

void gsgi_cleanup() {
    --is_setup;
    if (is_setup > 0) {
        return;
    }
    is_setup = FALSE;

    bt_set_iter it = bt_set_iterate(&connections);
    gsgi_request *r;
    while ((r = bt_set_iter_next(&it)) != NULL) {
        gsgi_request_destroy(r);
    }
    bt_set_destroy(&connections);

    bt_array_destroy(&gsgi_queue);
    sem_destroy(&gsgi_queue_mutex);

    // TODO destroy the gsgi loop thread
}

gsgi gsgi_create(gsgi_settings *settings) {
    gsgi_setup();
    gsgi object;
    memset(&object, 0, sizeof(object));
    // TODO create process
    return object;
}

void gsgi_destroy(gsgi *object) {
    gsgi_cleanup();
    // TODO stop process
    close(object->resfd);
    close(object->reqfd);
}

void gsgi_process_request(gsgi *object, WOLFSSL *ssl, const char *request) {
    // Create the request
    gsgi_request r;
    r.id = ++object->rid;
    r.ssl = ssl;
    r.object = object;
    r.state = 0;
    r.buf = bt_string_new(1024);

    // Add the request to the request set
    // The gsgi receiver handler should process outgoing responses
    bt_set_add(&connections, gsgi_request_hash(&r), &r);

    gsgi_cleanup();
}