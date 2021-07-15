#ifndef __GEMCAPS_SHARED_HANDLER__
#define __GEMCAPS_SHARED_HANDLER__

#include <cassert>

#include <string>
#include <iostream>
#include <memory>

#include <yaml-cpp/yaml.h>

#include "gemcaps/server.hpp"
#include "gemcaps/util.hpp"

#define RES_INPUT 10
#define RES_SENSITIVE_INPUT 11
#define RES_SUCCESS 20
#define RES_REDIRECT_TEMP 30
#define RES_REDIRECT_PERM 31
#define RES_FAIL_TEMP 40
#define RES_SERVER_UNAVAIL 41
#define RES_ERROR_CGI 42
#define RES_ERROR_PROXY 43
#define RES_SLOW_DOWN 44
#define RES_FAIL_PERM 50
#define RES_NOT_FOUND 51
#define RES_GONE 52
#define RES_BAD_REQUEST 59
#define RES_CERT_REQUIRED 60
#define RES_CERT_NOT_AUTH 61
#define RES_CERT_NOT_VALID 62

constexpr size_t string_length(const char *str) {
    int length = 0;
    while (str[length] != '\0') ++length;
    return length;
}

constexpr void string_copy(char *dest, const char *src) {
    int i = 0;
    while (src[i] != '\0') {
        dest[i] = src[i];
        ++i;
    }
    dest[i] = '\0';
}

/**
 * Create a response header string
 * 
 * @param response response code
 * @param meta meta value
 * @param header the header buffer (this allows us to put the header outside the scope of the function)
 * 
 * @return the header string
 */
constexpr const char *responseHeader(int response, const char *meta = "", char header[1024] = 0) {
    size_t i = 0;
    header[i++] = ('0' + (response / 10) % 10);
    header[i++] = ('0' + response % 10);
    header[i++] = ' ';
    string_copy(header + i, meta);
    i += string_length(meta);
    header[i++] = '\r';
    header[i++] = '\n';
    header[i++] = '\0';
    return header;
}

/**
 * A gemini request.
 * 
 * Eventually, client certificates will be included in the Request object
 * 
 * @property header the full header of the request
 * @property host
 * @property port
 * @property path 
 * @property query
 */
struct Request {
    std::string header;
    std::string host;
    uint16_t port;
    std::string path;
    std::string query;
};

/**
 * The base pure virtual class for all Handlers.
 */
class Handler {
public:
    /**
     * Handle an incoming request asynchronously
     * 
     * @note This function must not perform any synchronous operations.
     * 
     * @param request The request header
     * @param body The body output buffer
     */
    virtual void handle(const Request *request, OBufferPipe *body) = 0;
    /**
     * Check if this handler should process a given request.
     * 
     * @note This function must only perform synchronous operations.
     * 
     * @param host host of the request
     * @param path path of the request
     * 
     * @return whether this handler should process the given request
     */
    virtual bool shouldHandle(std::string host, std::string path) = 0;
};

/**
 * A factory to create handlers from settings
 */
class HandlerFactory {
public:
    /**
     * Create a handler from the given settings
     * 
     * @param settings settings to use to create a new handler
     * 
     * @return the newly created handler
     */
    virtual std::shared_ptr<Handler> createHandler(YAML::Node settings) = 0;
};

#endif