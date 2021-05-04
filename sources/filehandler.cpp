#include "filehandler.hpp"
#include "main.hpp"

#include "MimeTypes.h"

#include <filesystem>
#include <sstream>
#include <iostream>

namespace fs = std::filesystem;

using std::cout;
using std::cerr;
using std::endl;
using std::ostringstream;
using std::string;
using std::map;

MimeTypes mimetypes;

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

void on_scandir(uv_fs_t *req) {
    SSLClient *client = static_cast<SSLClient *>(uv_req_get_data((uv_req_t *)req));
    if (client == nullptr) {
        uv_fs_req_cleanup(req);
        return;
    }
    ClientContext *ctx = static_cast<ClientContext *>(client->getContext());
    FileContext *context = static_cast<FileContext *>(ctx->getContext());

    uv_dirent_t entry;
    if (uv_fs_get_result(req) < 0) {
        uv_fs_req_cleanup(req);
        ERROR("scandir error: " << uv_strerror(uv_fs_get_result(req)));
        context->getHandler()->internalError(client, context);
        return;
    }
    // Convert the scandir object into a map
    map<string, uv_dirent_type_t> dirs;
    while (uv_fs_scandir_next(req, &entry) != UV_EOF) {
        dirs[entry.name] = entry.type;
    }

    uv_fs_req_cleanup(req);
    context->getHandler()->onScandir(client, context, dirs);
}

void on_open(uv_fs_t *req) {
    SSLClient *client = static_cast<SSLClient *>(uv_req_get_data((uv_req_t *)req));
    if (client == nullptr) {
        uv_fs_req_cleanup(req);
        return;
    }
    ClientContext *ctx = static_cast<ClientContext *>(client->getContext());
    FileContext *context = static_cast<FileContext *>(ctx->getContext());

    int fd = uv_fs_get_result(req);
    if (fd < 0) {
        ERROR("open error: " << uv_strerror(uv_fs_get_result(req)));
        context->getHandler()->internalError(client, context);
        return;
    }
    context->getHandler()->onFileOpen(client, context, fd);
}

void on_read(uv_fs_t *req) {
    SSLClient *client = static_cast<SSLClient *>(uv_req_get_data((uv_req_t *)req));
    if (client == nullptr) {
        uv_fs_req_cleanup(req);
        return;
    }
    ClientContext *ctx = static_cast<ClientContext *>(client->getContext());
    FileContext *context = static_cast<FileContext *>(ctx->getContext());

    int res = uv_fs_get_result(req);
    if (res < 0) {
        ERROR("read error: " << uv_strerror(uv_fs_get_result(req)));
        context->getHandler()->internalError(client, context);
        return;
    }

    context->getHandler()->onFileRead(client, context, res);
}

void on_close(uv_fs_t *req) {
    if (uv_fs_get_result(req) < 0) {
        ERROR("close error: " << uv_strerror(uv_fs_get_result(req)));
        return;
    }
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
    context->setPath(realPath);
    // Check if the file has read permissions
    uv_fs_access(client->getLoop(), context->getReq(), realPath.c_str(), R_OK, got_access);
}

void FileHandler::gotAccess(SSLClient *client, FileContext *context, ssize_t result) {
    if (result | R_OK) {
        // The file is readable!

        // Check if the file exists
        string disk = context->getPath();
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
        uv_fs_scandir(client->getLoop(), context->getReq(), context->getPath().c_str(), 0, on_scandir);
        return;
    } 
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
    readFile(client, context);
}

void FileHandler::onScandir(SSLClient *client, FileContext *context, map<string, uv_dirent_type_t> &dirs) {
    // find an index.xyz file
    string index;
    bool hasIndex = false;

    for (auto pair : dirs) {
        string name = pair.first;
        if (name.rfind("index.", 0) == 0) {
            hasIndex = true;
            index = name;
            break;
        }
    }

    if (hasIndex) {
        fs::path p = context->getPath();
        p /= index;
        context->setPath(p);
        readFile(client, context);
        return;
    }

    if (!settings->getReadDirs()) {
        // If the handler is not allowed to read directories, send an error message
        CacheData error;
        error.lifetime = settings->getCacheTime();
        error.response = RES_NOT_FOUND;
        error.meta = "You are not allowed to access this file";
        getCache()->add(context->getRequest().getRequest(), error);
        sendCache(client, context);
        return;
    }

    // Read the directory

    ostringstream oss;
    fs::path p = context->getRequest().getPath();
    oss << "# " << context->getRequest().getPath() << "\n\n";
    if (p.parent_path() != p) {
        oss << "=> " << p.parent_path().parent_path().string() << " 📁 ../\n";
    }
    for (auto pair : dirs) {
        oss << "=> " << (p / pair.first).string();
        if (pair.second == 2) {
            oss << " 📁";
        } else {
            oss << " 📄";
        }
        oss << " ./" << pair.first << "\n";
    }
    oss << "\n";

    CacheData dir;
    dir.lifetime = settings->getCacheTime();
    dir.meta = "text/gemini";
    dir.response = 20;
    dir.body = oss.str();
    getCache()->add(context->getRequest().getRequest(), dir);
    sendCache(client, context);
}

void FileHandler::readFile(SSLClient *client, FileContext *context) {
    DEBUG("Opening file..");
    uv_fs_open(client->getLoop(), context->getReq(), context->getPath().c_str(), 0, UV_FS_O_RDONLY, on_open);
}

void FileHandler::onFileOpen(SSLClient *client, FileContext *context, uv_file file) {
    context->setFile(file);
    uv_fs_read(client->getLoop(), context->getReq(), file, context->getBuf(), 1, context->getOffset(), on_read);
}

void FileHandler::onFileRead(SSLClient *client, FileContext *context, unsigned int read) {
    DEBUG("read " << read << " bytes of data");
    if (read > 0) {
        context->getBody() += context->getBuf()->base;
        context->setOffset(context->getOffset() + read);
        uv_fs_read(client->getLoop(), context->getReq(), context->getFile(), context->getBuf(), 1, context->getOffset(), on_read);
        return;
    }

    uv_fs_close(client->getLoop(), context->getReq(), context->getFile(), on_close);

    // TODO: get mimetype

    CacheData response;
    response.lifetime = settings->getCacheTime();
    response.body = context->getBody();
    response.response = 20;
    response.meta = mimetypes.getType(context->getPath().c_str());
    getCache()->add(context->getRequest().getRequest(), response);
    sendCache(client, context);
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
