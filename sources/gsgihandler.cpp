#include "gsgihander.hpp"

GeminiHandler::GeminiHandler(GSGISettings settings) 
        : settings(settings){
    gsgi_settings conf;
    object = gsgi_create(&conf);
}

GeminiHandler::~GeminiHandler() {
    gsgi_destroy(&object);
}

void GeminiHandler::handle(WOLFSSL *ssl, const GeminiRequest &request) {
    gsgi_process_request(&object, ssl, request.getRequest().c_str());
}