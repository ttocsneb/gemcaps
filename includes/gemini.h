#ifndef __GEMCAPS_GEMINI__
#define __GEMCAPS_GEMINI__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char *host;
    char *port;
    char *path;
    char *query;
} gemini_request;

#ifdef __cplusplus
}
#endif
#endif