#include "gsgihander.hpp"

#include <vector>

using std::string;
using std::vector;
using std::shared_ptr;

GSGIHandler::~GSGIHandler() {
    if (instance.pid != 0) {
        gsgi_destroy(&instance);
    }
}

void GSGIHandler::create() {
    gsgi_settings conf;

    // Create the command arguments
    auto &command = settings->getCommand();
    int size = command.size() + 1;
    conf.command = new const char*[size];
    memset(&conf.command, 0, sizeof(char*) * size);
    for (int i = 0; i < command.size(); ++i) {
        conf.command[i] = command.at(i).c_str();
    }

    // Create the environment variables
    auto &environment = settings->getEnvironment();
    vector<string> env;
    size = environment.size() + 1;
    conf.env_vars = new const char*[size];
    memset(&conf.env_vars, 0, sizeof(char*) * size);
    int i = 0;
    for (auto &item : environment) {
        env.push_back(item.first + "=" + item.second);
        conf.env_vars[i++] = env.back().c_str();
    }

    // Create the gsgi instance
    instance = gsgi_create(&conf);

    // Free the config
    delete[] conf.command;
    delete[] conf.env_vars;
}

void GSGIHandler::handle(WOLFSSL *ssl, const GeminiRequest &request) {
    // Dynamically start the gsgi instance
    if (instance.pid == 0) {
        create();
    }
    gsgi_process_request(&instance, ssl, request.getRequest().c_str());
}