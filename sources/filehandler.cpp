#include "filehandler.hpp"
#include "main.hpp"

#include <filesystem>
#include <sstream>
#include <iostream>

namespace fs = std::filesystem;

using std::cout;
using std::cerr;
using std::endl;
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
    context->getHandler()->sendCache(client, context);
}

void sentData(SSLClient *client) {
    ClientContext *ctx = static_cast<ClientContext *>(client->getContext());
    FileContext *context = static_cast<FileContext *>(ctx->getContext());
    // Once the data has been sent, close the connection
    if (context->getState() == FileContext::READY) {
        delete client;
    }
}

void got_realpath(uv_fs_t *req) {
    SSLClient *client = static_cast<SSLClient *>(uv_req_get_data((uv_req_t *)req));
    if (client == nullptr) {
        uv_fs_req_cleanup(req);
        return;
    }
    ClientContext *ctx = static_cast<ClientContext *>(client->getContext());
    FileContext *context = static_cast<FileContext *>(ctx->getContext());
    if (req->ptr != nullptr) {
        string realpath = string((char *)uv_fs_get_ptr(req));
        uv_fs_req_cleanup(req);
        context->getHandler()->gotRealPath(client, context, realpath);
    } else {
        uv_fs_req_cleanup(req);
        context->getHandler()->gotInvalidPath(client, context);
    }
}

void got_access(uv_fs_t *req) {
    SSLClient *client = static_cast<SSLClient *>(uv_req_get_data((uv_req_t *)req));
    if (client == nullptr) {
        uv_fs_req_cleanup(req);
        return;
    }
    ClientContext *ctx = static_cast<ClientContext *>(client->getContext());
    FileContext *context = static_cast<FileContext *>(ctx->getContext());
    ssize_t res = uv_fs_get_result(req);
    uv_fs_req_cleanup(req);
    context->getHandler()->gotAccess(client, context, res);
}

void got_stat(uv_fs_t *req) {
    SSLClient *client = static_cast<SSLClient *>(uv_req_get_data((uv_req_t *)req));
    if (client == nullptr) {
        uv_fs_req_cleanup(req);
        return;
    }
    ClientContext *ctx = static_cast<ClientContext *>(client->getContext());
    FileContext *context = static_cast<FileContext *>(ctx->getContext());

    uv_stat_t statbuf = *uv_fs_get_statbuf(req);
    if (uv_fs_get_result(req) < 0) {
        uv_fs_req_cleanup(req);
        ERROR("stat error: " << uv_strerror(uv_fs_get_result(req)));
        context->getHandler()->internalError(client, context);
        return;
    }
    DEBUG("file type: " << statbuf.st_mode);
    uv_fs_req_cleanup(req);
    context->getHandler()->gotStat(client, context, &statbuf);
}

void FileHandler::internalError(SSLClient *client, FileContext *context) {
    CacheData error;
    error.lifetime = settings->getCacheTime();
    error.response = RES_ERROR_CGI;
    error.meta = "An internal error occurred";
    getCache()->add(context->getRequest().getRequest(), error);
    sendCache(client, context);
}

void FileHandler::sendCache(SSLClient *client, FileContext *context) {
    context->setState(FileContext::READY);
    DEBUG("Sending Cache");

    // Send cached response
    const CacheData &data = getCache()->get(context->getRequest().getRequest());
    ostringstream oss;
    oss << data.response << " " << data.meta << "\r\n";
    if (data.response >= 20 && data.response <= 29) {
        oss << data.body;
    }
    string content = oss.str();
    client->write(content.c_str(), content.length());
    return;
}

void FileHandler::gotRealPath(SSLClient *client, FileContext *context, string realPath) {
    // Check if the handler has permissions to read the file
    DEBUG("Real path: " << realPath);
    bool valid = true;
    if (!settings->getAllowedDirs().empty()) {
        valid = false;
        for (const auto &rule : settings->getAllowedDirs()) {
            if (rule.match(realPath)) {
                valid = true;
                break;
            }
        }
    }
    if (!valid) {
        // Make sure that the path ends with a / just in case the rules require a trailing backslash
        if (realPath.find("/", realPath.length() - 1) == string::npos) {
            gotRealPath(client, context, realPath + "/");
            return;
        }

        // The handler is not allowed to read this file
        CacheData error;
        error.lifetime = settings->getCacheTime();
        error.response = RES_NOT_FOUND;
        error.meta = "You are not allowed to access this file";
        getCache()->add(context->getRequest().getRequest(), error);
        sendCache(client, context);
        return;
    }
    context->setDisk(realPath);
    // Check if the file has read permissions
    uv_fs_access(client->getLoop(), context->getReq(), realPath.c_str(), R_OK, got_access);
}

void FileHandler::gotAccess(SSLClient *client, FileContext *context, ssize_t result) {
    if (result | R_OK) {
        // The file is readable!

        // Check if the file exists
        string disk = context->getDisk();
        DEBUG("disk: " << disk);
        uv_fs_stat(client->getLoop(), context->getReq(), disk.c_str(), got_stat);
        return;
    } else {
        CacheData error;
        error.lifetime = settings->getCacheTime();
        error.response = RES_NOT_FOUND;
        error.meta = "You are not allowed to access this file";
        getCache()->add(context->getRequest().getRequest(), error);
        sendCache(client, context);
        return;
    }
}

void FileHandler::gotStat(SSLClient *client, FileContext *context, uv_stat_t *statbuf) {
    const string &path = context->getRequest().getPath();
    size_t final_backslash = path.find('/', path.length() - 1);
    DEBUG("mode: " << statbuf->st_mode);
    if (statbuf->st_mode & S_IFDIR) {
        // The file is a directory
        if (final_backslash == string::npos) {
            // Make sure the path has a trailing backslash
            CacheData redirect;
            redirect.lifetime = settings->getCacheTime();
            redirect.meta = path + "/";
            redirect.response = RES_REDIRECT_PERM;
            getCache()->add(context->getRequest().getRequest(), redirect);
            sendCache(client, context);
            return;
        }
        CacheData ret;
        ret.lifetime = settings->getCacheTime();
        ret.meta = "text/gemini";
        ret.response = 20;
        ret.body = "Directory:\n" + context->getDisk() + "\n";
        getCache()->add(context->getRequest().getRequest(), ret);
        sendCache(client, context);
        // TODO: read the directory
        // TODO: find an index.xyz file
    } else {
        // The file is a file
        if (final_backslash != string::npos) {
            // Make sure the path does not have a trailing backslash
            CacheData redirect;
            redirect.lifetime = settings->getCacheTime();
            redirect.meta = path.substr(0, final_backslash);
            redirect.response = RES_REDIRECT_PERM;
            getCache()->add(context->getRequest().getRequest(), redirect);
            sendCache(client, context);
            return;
        }
        // TODO: read the file and create the cache
        CacheData ret;
        ret.lifetime = settings->getCacheTime();
        ret.meta = "text/gemini";
        ret.response = 20;
        ret.body = "File:\n" + context->getDisk() + "\n";
        getCache()->add(context->getRequest().getRequest(), ret);
        sendCache(client, context);
    }
}

void FileHandler::gotInvalidPath(SSLClient *client, FileContext *context) {
    CacheData error;
    error.lifetime = settings->getCacheTime();
    error.response = 51;
    error.meta = "File does not exist";
    getCache()->add(context->getRequest().getRequest(), error);
    sendCache(client, context);
}

void FileHandler::handle(SSLClient *client, const GeminiRequest &request) {
    client->setWriteReadyCallback(sentData);
    ClientContext *ctx = static_cast<ClientContext *>(client->getContext());
    FileContext *context = new FileContext(this, request, settings);
    ctx->setContext(context, fileContextDestructor);

    // Check if the data has been cached already
    Cache *c = getCache();
    if (c->isLoaded(request.getRequest())) {
        sendCache(client, context);
        return;
    }
    // Check if the data is being loaded
    if (c->isLoading(request.getRequest())) {
        c->getNotified(request.getRequest(), cacheReady, ctx);
        return;
    }
    // Tell the cache that the data is being loaded
    c->loading(request.getRequest());

    // Find the path on file
    fs::path path(settings->getRoot());
    string p = request.getPath();
    if (p.rfind('/', 0) == 0) {
        p = p.substr(1);
    }
    path /= p;
    path = path.make_preferred();
    string full = path;
    context->setPath(path);

    // find the realpath
    uv_fs_t *req = context->getReq();
    uv_req_set_data((uv_req_t *)req, client);
    int res = uv_fs_realpath(client->getLoop(), context->getReq(), path.c_str(), got_realpath);
    if (res == UV_ENOSYS) {
        cerr << "Error: Unable to read file due to unsupported operating system!" << endl;
        delete client;
        return;
    }
}
