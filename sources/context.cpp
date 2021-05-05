#include "context.hpp"

using std::string;

void ClientContext::onTimeout() {
    const static string response("42 Request couldn't be processed in time\r\n");
    getClient()->write(response.c_str(), response.length());
    getClient()->close();
}