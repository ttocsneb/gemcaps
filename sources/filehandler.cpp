#include "filehandler.hpp"

#include <filesystem>

namespace fs = std::filesystem;


void fileContextDestructor(void *ctx) {
    FileContext *context = static_cast<FileContext *>(ctx);
    delete context;
}


void FileHandler::handle(SSLClient *client, const GeminiRequest &request) {
    ClientContext *ctx = static_cast<ClientContext *>(client->getContext());
    FileContext *context = new FileContext(request);
    ctx->setContext(context, fileContextDestructor);
    // TODO: get relative base to base of the 
    fs::path path(settings->getRoot());
    path /= request.getPath();
    context->setPath(path);
    client->getLoop();
}
