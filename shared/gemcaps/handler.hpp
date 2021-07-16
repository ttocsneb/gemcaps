#ifndef __GEMCAPS_SHARED_HANDLER__
#define __GEMCAPS_SHARED_HANDLER__

#include <cassert>

#include <string>
#include <iostream>
#include <memory>

#include <yaml-cpp/yaml.h>

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
 * A connection to a client.
 *
 * This abstraction represents the connection to a client.
 *
 * Eventually, support for client certificates will be added to this abstraction
 */
class ClientConnection {
public:
	/**
	 * Get the client's request
	 *
	 * @return the client's request
	 */
	virtual const Request& getRequest() const = 0;

	/**
	 * Send data to the client
	 * 
	 * @param data data to send
	 * @param length length of the data
	 */
	virtual void send(const void *data, size_t length) = 0;
	/**
	 * Send text to the client
	 *
	 * @param message null terminated string
	 */
	void send(const char *message) noexcept {
		send(message, strlen(message));
	}

	/**
	 * Close the connection to the client
	 */
	virtual void close() = 0;
};

/**
 * The base pure virtual class for all Handlers.
 */
class Handler {
public:
    /**
     * Handle an incoming request asynchronously
     *
	 * To properly handle a request, you must first send a response header
	 * (you may use responseHeader() to generate the header) then if the
	 * response code calls for a body, send the body contents. After your
	 * response is finished, you must call close on the client.
	 *
     * @note This function must not perform any synchronous operations.
	 *
	 * @warning the client is only garenteed to exist prior to closing it.
	 *     After calling ClientConnection::close(), you should assume that
	 *     the client is no longer in memory.
     * 
     * @param client the connection to the client
     */
    virtual void handle(ClientConnection *client) = 0;
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