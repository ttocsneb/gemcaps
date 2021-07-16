#ifndef __GEMCAPS_SHARED_HANDLER__
#define __GEMCAPS_SHARED_HANDLER__

#include <cassert>

#include <string>
#include <iostream>
#include <memory>

#include <yaml-cpp/yaml.h>

#include "gemcaps/server.hpp"
#include "gemcaps/util.hpp"
#include "gemcaps/stringutil.hpp"


constexpr const int RES_INPUT = 10;
constexpr const int RES_SENSITIVE_INPUT = 11;
constexpr const int RES_SUCCESS = 20;
constexpr const int RES_REDIRECT_TEMP = 30;
constexpr const int RES_REDIRECT_PERM = 31;
constexpr const int RES_FAIL_TEMP = 40;
constexpr const int RES_SERVER_UNAVAIL = 41;
constexpr const int RES_ERROR_CGI = 42;
constexpr const int RES_ERROR_PROXY = 43;
constexpr const int RES_SLOW_DOWN = 44;
constexpr const int RES_FAIL_PERM = 50;
constexpr const int RES_NOT_FOUND = 51;
constexpr const int RES_GONE = 52;
constexpr const int RES_BAD_REQUEST = 59;
constexpr const int RES_CERT_REQUIRED = 60;
constexpr const int RES_CERT_NOT_AUTH = 61;
constexpr const int RES_CERT_NOT_VALID = 62;

/**
 * Create a response header string
 * 
 * @param response response code
 * @param meta meta string
 * 
 * @return string literal
 */
template<size_t Size = 1024>
constexpr const StringLiteral<Size> responseHeader(int response, const char *meta) {
    StringLiteral<Size> builder;
    builder.append(response);
    builder.append(" ");
    builder.append(meta);
    builder.append("\r\n");
    return builder;
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