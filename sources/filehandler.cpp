#include "filehandler.hpp"
#include "main.hpp"

#include "MimeTypes.h"

#include <filesystem>
#include <sstream>
#include <iostream>
#include <functional>

namespace fs = std::filesystem;

using std::cout;
using std::cerr;
using std::endl;
using std::ostringstream;
using std::string;
using std::map;
using std::hash;

MimeTypes mimetypes;

////////// FileContext handlers //////////

void got_realpath(uv_fs_t *req);
void got_access(uv_fs_t *req);
void got_stat(uv_fs_t *req);
void on_scandir(uv_fs_t *req);
void on_open(uv_fs_t *req);
void on_read(uv_fs_t *req);
void on_close(uv_fs_t *req);

void onCacheReady(const CachedData &data, Cache *cache, void *arg) {
    FileContext *context = static_cast<FileContext *>(arg);
    if (cache->isLoaded(context->getCacheKey())) {
        context->send(data);
        return;
    }
    context->handle();
}

void got_realpath(uv_fs_t *req) {
    FileContext *context = static_cast<FileContext *>(uv_req_get_data((uv_req_t *)req));
    if (context == nullptr) {
        uv_fs_req_cleanup(req);
        return;
    }
    if (req->ptr != nullptr) {
        string realpath = string((char *)uv_fs_get_ptr(req));
        uv_fs_req_cleanup(req);
        // TODO: Real path
        // Check if the handler has permissions to read the file
        LOG_DEBUG("Real path: " << realpath);
        bool valid = true;
        if (!context->settings->getAllowedDirs().empty()) {
            valid = false;
            for (const auto &rule : context->settings->getAllowedDirs()) {
                if (rule.match(realpath)) {
                    valid = true;
                    break;
                }
            }
        }
        if (!valid) {
            // The handler is not allowed to read this file
            CachedData error = context->createCache(RES_NOT_FOUND, "You are not allowed to access this file");
            context->send(error);
            context->getCache()->add(context->getCacheKey(), error);
            return;
        }
        context->file = realpath;
        // Check if the file has read permissions
        uv_fs_access(context->getClient()->getLoop(), req, realpath.c_str(), R_OK, got_access);
        return;
    } 
    uv_fs_req_cleanup(req);
    // Invalid path
    CachedData data = context->createCache(RES_NOT_FOUND, "File does not exist");
    context->send(data);
    context->getCache()->add(context->getCacheKey(), data);
}

void got_access(uv_fs_t *req) {
    FileContext *context = static_cast<FileContext *>(uv_req_get_data((uv_req_t *)req));
    if (context == nullptr) {
        uv_fs_req_cleanup(req);
        return;
    }
    ssize_t result = uv_fs_get_result(req);
    if (result | R_OK) {
        // Check if the file exists
        uv_fs_req_cleanup(req);
        uv_fs_stat(context->getClient()->getLoop(), req, context->file.c_str(), got_stat);
        return;
    } 
    CachedData error = context->createCache(RES_NOT_FOUND, "You are not allowed to access this file");
    context->send(error);
    context->getCache()->add(context->getCacheKey(), error);
    uv_fs_req_cleanup(req);
}

void got_stat(uv_fs_t *req) {
    FileContext *context = static_cast<FileContext *>(uv_req_get_data((uv_req_t *)req));
    if (context == nullptr) {
        uv_fs_req_cleanup(req);
        return;
    }
    uv_stat_t statbuf = *uv_fs_get_statbuf(req);
    if (uv_fs_get_result(req) < 0) {
        uv_fs_req_cleanup(req);
        LOG_ERROR("stat error: " << uv_strerror(uv_fs_get_result(req)));
        CachedData error = context->createCache(RES_ERROR_CGI, "Internal Server Error");
        context->send(error);
        context->getCache()->add(context->getCacheKey(), error);
        return;
    }
    LOG_DEBUG("file type: " << statbuf.st_mode);

    const string &path = context->getRequest().getPath();
    size_t final_backslash = path.find('/', path.length() - 1);
    if (statbuf.st_mode & S_IFDIR) {
        // The file is a directory
        if (final_backslash == string::npos) {
            // Make sure the path has a trailing backslash
            CachedData redirect = context->createCache(RES_REDIRECT_PERM, path + "/");
            context->send(redirect);
            context->getCache()->add(context->getCacheKey(), redirect);
            uv_fs_req_cleanup(req);
            return;
        }
        uv_fs_req_cleanup(req);
        uv_fs_scandir(context->getClient()->getLoop(), req, context->file.c_str(), 0, on_scandir);
        return;
    } 
    // The file is a file
    if (final_backslash != string::npos) {
        // Make sure the path does not have a trailing backslash
        CachedData redirect = context->createCache(RES_REDIRECT_PERM, path.substr(0, final_backslash));
        context->send(redirect);
        context->getCache()->add(context->getCacheKey(), redirect);
        uv_fs_req_cleanup(req);
        return;
    }
    uv_fs_req_cleanup(req);
    uv_fs_open(context->getClient()->getLoop(), req, context->file.c_str(), 0, UV_FS_O_RDONLY, on_open);
}

void on_scandir(uv_fs_t *req) {
    FileContext *context = static_cast<FileContext *>(uv_req_get_data((uv_req_t *)req));
    if (context == nullptr) {
        uv_fs_req_cleanup(req);
        return;
    }

    uv_dirent_t entry;
    if (uv_fs_get_result(req) < 0) {
        uv_fs_req_cleanup(req);
        LOG_ERROR("scandir error: " << uv_strerror(uv_fs_get_result(req)));
        CachedData error = context->createCache(RES_ERROR_CGI, "Internal Server Error");
        context->send(error);
        context->getCache()->add(context->getCacheKey(), error);
        return;
    }
    // Convert the scandir object into a map
    map<string, uv_dirent_type_t> dirs;
    while (uv_fs_scandir_next(req, &entry) != UV_EOF) {
        dirs[entry.name] = entry.type;
    }
    uv_fs_req_cleanup(req);

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
        fs::path p = context->file;
        p /= index;
        context->file = p.string();
        uv_fs_open(context->getClient()->getLoop(), req, context->file.c_str(), 0, UV_FS_O_RDONLY, on_open);
        return;
    }

    if (!context->settings->getReadDirs()) {
        // If the handler is not allowed to read directories, send an error message
        CachedData error = context->createCache(RES_NOT_FOUND, "You are not allowed to access this file");
        context->send(error);
        context->getCache()->add(context->getCacheKey(), error);
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
        string type = mimetypes.getType(pair.first.c_str());
        if (pair.second == 2) {
            oss << " 📁";
        } else if (type.find("gemini") != string::npos) {
            oss << " ♊";
        } else if (type.find("markdown") != string::npos) {
            oss << " 🔽";
        } else if (type.find("audio") != string::npos) {
            oss << " 🎶";
        } else if (type.find("image") != string::npos) {
            oss << " 🖼️";
        } else if (type.find("video") != string::npos) {
            oss << " 📺";
        } else if (type.find("pdf") != string::npos) {
            oss << " 📖";
        } else if (type.find("zip") != string::npos || type.find("compressed") != string::npos) {
            oss << " 📦";
        } else if (type.find("calendar") != string::npos) {
            oss << " 📅";
        } else {
            oss << " 📄";
        }
        oss << " ./" << pair.first << "\n";
    }
    oss << "\n";

    CachedData dir = context->createCache(RES_SUCCESS, "text/gemini");
    dir.body = oss.str();
    context->send(dir);
    context->getCache()->add(context->getCacheKey(), dir);
}

void on_open(uv_fs_t *req) {
    FileContext *context = static_cast<FileContext *>(uv_req_get_data((uv_req_t *)req));
    if (context == nullptr) {
        uv_fs_req_cleanup(req);
        return;
    }

    int fd = uv_fs_get_result(req);
    uv_fs_req_cleanup(req);
    if (fd < 0) {
        LOG_ERROR("open error: " << uv_strerror(uv_fs_get_result(req)));
        CachedData error = context->createCache(RES_ERROR_CGI, "Internal Server Error");
        context->send(error);
        context->getCache()->add(context->getCacheKey(), error);
        return;
    }
    context->file_fd = fd;
    context->offset = 0;
    uv_fs_read(context->getClient()->getLoop(), req, context->file_fd, &context->buf, 1, context->offset, on_read);
}

void on_read(uv_fs_t *req) {
    FileContext *context = static_cast<FileContext *>(uv_req_get_data((uv_req_t *)req));
    if (context == nullptr) {
        uv_fs_req_cleanup(req);
        return;
    }

    int read = uv_fs_get_result(req);
    if (read < 0) {
        LOG_ERROR("read error: " << uv_strerror(uv_fs_get_result(req)));
        CachedData error = context->createCache(RES_ERROR_CGI, "Internal Server Error");
        context->send(error);
        context->getCache()->add(context->getCacheKey(), error);
        return;
    }

    if (read > 0) {
        context->buffer += string(context->filebuf, read);
        context->offset += read;
        uv_fs_read(context->getClient()->getLoop(), req, context->file_fd, &context->buf, 1, context->offset, on_read);
        return;
    }

    context->closing = true;
    uv_fs_close(context->getClient()->getLoop(), req, context->file_fd, on_close);
    context->file_fd = 0;

    CachedData response = context->createCache(RES_SUCCESS, mimetypes.getType(context->file.c_str()));
    response.body = context->buffer;
    context->send(response);
    context->getCache()->add(context->getCacheKey(), response);
}

void on_close(uv_fs_t *req) {
    FileContext *context = static_cast<FileContext *>(uv_req_get_data((uv_req_t *)req));
    if (context) {
        if (uv_fs_get_result(req) < 0) {
            LOG_ERROR("close error: " << uv_strerror(uv_fs_get_result(req)));
            return;
        }
        context->_closed();
    }
}

////////// FileContext //////////

FileContext::FileContext(FileHandler *handler, SSLClient *client, Cache *cache, GeminiRequest request, string path, std::shared_ptr<FileSettings> settings)
        : ClientContext(handler, client, cache),
          handler(handler),
          settings(settings),
          request(request),
          path(path) {
    buf.base = filebuf;
    buf.len = sizeof(filebuf);
    ostringstream oss;
    int port = request.getPort();
    if (port == 0) {
        port = 1965;
    }
    key.name = path;
    hash<string> str_hash;
    hash<FileHandler*> ctx_hash;
    key.hash = ctx_hash(handler);
    key.hash = key.hash * 31 + str_hash(path);
    key.hash = key.hash * 31 + str_hash(request.getPath());
    key.hash = key.hash * 31 + str_hash(request.getQuery());

    uv_req_set_data((uv_req_t *)&req, this);
}

FileContext::~FileContext() {
}

void FileContext::onDestroy() {
    destroying = true;
    if (processing_cache) {
        getCache()->cancel(key);
    }
    if (file_fd) {
        uv_fs_close(getClient()->getLoop(), &req, file_fd, on_close);
        file_fd = 0;
        return;
    } else if (!closing) {
        _closed();
    }
}

void FileContext::_closed() {
    closing = false;
    file_fd = 0;
    if (destroying) {
        destroying = false;
        destroy_done();
    }
}

void FileContext::onRead() {

}

void FileContext::onWrite() {

}

void FileContext::handle() {
    Cache *c = getCache();
    // Check if the data has been cached already
    if (c->isLoaded(getCacheKey())) {
        send(c->get(getCacheKey()));
        return;
    }
    // Check if the data is being loaded
    if (c->isLoading(getCacheKey())) {
        c->getNotified(getCacheKey(), onCacheReady, this);
        return;
    }
    // Tell the cache that the data is being loaded
    c->loading(getCacheKey());
    processing_cache = true;

    // Find the path on file
    fs::path p(settings->getRoot());
    if (path.rfind('/', 0) == 0) {
        path = path.substr(1);
    }
    p /= path;
    p = p.make_preferred();
    file = p.string();

    // find the realpath
    int res = uv_fs_realpath(getClient()->getLoop(), &req, p.string().c_str(), got_realpath);
    if (res == UV_ENOSYS) {
        cerr << "Error: Unable to read file due to unsupported operating system!" << endl;
        getClient()->crash();
        return;
    }
}

void FileContext::send(const CachedData &data) {
    processing_cache = false;
    string content = data.generateResponse();
    LOG_INFO(getRequest().getRequestName() << " [" << data.response << "]");
    getClient()->write(content.c_str(), content.length());
    getClient()->close();

}

CachedData FileContext::createCache(int response, string meta) {
    CachedData data;
    data.lifetime = settings->getCacheTime();
    data.response = response;
    data.meta = meta;
    return data;
}

////////// FileHandler //////////

void FileHandler::handle(SSLClient *client, const GeminiRequest &request, string path) {
    FileContext *context = new FileContext(this, client, getCache(), request, path, settings);
    _add_context(context);
    client->setContext(context);

    context->handle();
}
