#ifndef __GEMCAPS_WSGI__
#define __GEMCAPS_WSGI__

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int id;
    char ip[64];
    int port;
    char request[1025];
} wsgi_request;

typedef struct {
    int id;
    int response;
    char meta[1025];
    int cache_time;
    int size;
} wsgi_response;


#ifdef __cplusplus
}
#endif
#endif