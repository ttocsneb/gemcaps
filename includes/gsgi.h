#ifndef __GEMCAPS_GSGI__
#define __GEMCAPS_GSGI__

#include <stdio.h>

#include "gemini.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    const char **command;
    const char **env_vars;
} gsgi_settings;

typedef struct {
    int pid;    // The process id of the gsgi instance
    int rid;    // The current request id
    int reqfd;  // The request pipe
    int resfd;  // The response pipe
} gsgi;
typedef struct {
    int id;
    int response;
    char meta[1025];
    int cache_time;
    int size;
} gsgi_response;

/**
 * Create a new gsgi object
 * 
 * @param settings settings for the object
 * 
 * @return the created gsgi object
 */
gsgi gsgi_create(gsgi_settings *settings);
/**
 * Destroy the gsgi object
 * 
 * @param object gsgi object
 */
void gsgi_destroy(gsgi *object);

/**
 * Process a gemini request
 * 
 * @param object gsgi object
 * @param ssl ssl instance
 * @param request gemini request
 */
void gsgi_process_request(gsgi *object, WOLFSSL *ssl, const char *request);

#ifdef __cplusplus
}
#endif
#endif