#include "filehandler.hpp"

#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

using std::ostringstream;
using std::string;

void fileContextDestructor(void *ctx) {
    FileContext *context = static_cast<FileContext *>(ctx);
    delete context;
}

void cacheReady(const CacheData &data, void *arg) {
    SSLClient *client = static_cast<SSLClient *>(arg);
    ClientContext *ctx = static_cast<ClientContext *>(client->getContext());
    FileContext *context = static_cast<FileContext *>(ctx->getContext());
    // TODO: send the cached data
}

void sentData(SSLClient *client) {
    // TODO: figure out if the data has been sent

    // If the data has been sent, close the connection
    delete client;
}


void FileHandler::handle(SSLClient *client, const GeminiRequest &request) {
    client->setWriteReadyCallback(sentData);
    ClientContext *ctx = static_cast<ClientContext *>(client->getContext());
    FileContext *context = new FileContext(request, settings);
    ctx->setContext(context, fileContextDestructor);
    fs::path path(settings->getRoot());
    path /= request.getPath();
    context->setPath(path);
    client->getLoop();

    Cache *c = getCache();
    if (c->isLoaded(path)) {
        // Send cached response
        const CacheData &data = c->get(path);
        ostringstream oss;
        oss << data.response << " " << data.meta << "\r\n";
        oss << data.body;
        string content = oss.str();
        client->write(content.c_str(), content.length());
        return;
    }
    if (c->isLoading(path)) {
        c->getNotified(path, cacheReady, ctx);
        return;
    }
    c->loading(path);
    // TODO: find the realpath
    // TODO: check if the handler has permissions to read the file
    // TODO: handle the request
    // TODO: create the cache
}
