#include "loader.hpp"

#include "gemcaps/settings.hpp"
#include "gemcaps/pathutils.hpp"
#include "filehandler.hpp"

using std::shared_ptr;
using std::make_shared;
using std::string;


shared_ptr<SSLServer> loadServer(YAML::Node settings, string dir, uv_loop_t *loop) {
    shared_ptr<SSLServer> server = make_shared<SSLServer>();

    if (loop == nullptr) {
        loop = uv_default_loop();
    }


    string host = getProperty<string>(settings, HOST, "0.0.0.0");
    int port = getProperty<int>(settings, PORT, 1965);
    string cert = getProperty<string>(settings, CERT);
    string key = getProperty<string>(settings, KEY);

    if (path::isrel(cert)) {
        cert = path::join(dir, cert);
    }
    if (path::isrel(key)) {
        key = path::join(dir, key);
    }

    server->load(loop, host, port, cert, key);

    return server;
}

void HandlerLoader::loadFactories() noexcept {
    factories.insert({"filehandler", make_shared<FileHandlerFactory>()});
}

shared_ptr<Handler> HandlerLoader::loadHandler(YAML::Node settings, string dir) {
    string handlerName = getProperty<string>(settings, HANDLER);

    auto factory = factories.find(handlerName);
    if (factory == factories.end()) {
        throw InvalidSettingsException(settings[HANDLER].Mark(), "'" + handlerName + "' is not a valid handler");
    }

    return factory->second->createHandler(settings, dir);
}