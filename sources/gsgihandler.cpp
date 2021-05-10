#include "gsgihander.hpp"

#include <vector>

using std::string;
using std::vector;
using std::shared_ptr;

GSGIHandler::~GSGIHandler() {
}

void GSGIHandler::create() {
}

void GSGIHandler::handle(SSLClient *client, const GeminiRequest &request, string path) {
    // Dynamically start the gsgi instance
    // gsgi_process_request(&instance, ssl, request.getRequest().c_str());
}