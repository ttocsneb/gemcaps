#include "filehandler.hpp"

#include <sstream>

#include <uv.h>

#include "gemcaps/util.hpp"
#include "gemcaps/uvutils.hpp"
#include "gemcaps/pathutils.hpp"
#include "gemcaps/MimeTypes.h"
#include "gemcaps/log.hpp"


using std::shared_ptr;
using std::make_shared;
using std::string;
using std::vector;
using std::regex;
using std::ostringstream;

namespace re_consts = std::regex_constants;

constexpr const auto ILLEGAL_FILE = responseHeader<32>(RES_NOT_FOUND, "Illegal File");
constexpr const auto DOES_NOT_EXIST = responseHeader<32>(RES_NOT_FOUND, "File does not exist");
constexpr const auto FILE_NOT_OPEN = responseHeader<32>(RES_GONE, "File could not be opened");

#define HEADER(x) x.buf, x.length()

////////////////////////////////////////////////////////////////////////////////
//
// FileHandler
//
////////////////////////////////////////////////////////////////////////////////

bool FileHandler::validateFile(string file) const noexcept {
    for (regex pattern : rules) {
        if (!std::regex_search(file, pattern, re_consts::match_not_null | re_consts::match_any)) {
            return false;
        }
    }
    return true;
}

bool FileHandler::isExecutable(string file) const noexcept {
    for (string cgi_type : cgi_types) {
        if (file.find(cgi_type, file.length() - cgi_type.length()) != string::npos) {
            return true;
        }
    }
    return false;
}


bool FileHandler::shouldHandle(string host, string path) noexcept {
    if (!this->host.empty()) {
        if (this->host != host) {
            return false;
        }
    }

    if (!this->base.empty()) {
        if (!path::isSubpath(base, path)) {
            return false;
        }
    }
    return true;
}

struct RequestContext {
    uv_fs_t req;
    uv_file fd;
    uv_buf_t buf;
    size_t offset;
    string file;
	ClientConnection *client;
    const FileHandler *handler;
};
ReusableAllocator<RequestContext> request_allocator;

////////////////////////////////////////////////////////////////////////////////
//
// FileHandler::handle()
//
// Handle the file request and send the response to the client.
//
////////////////////////////////////////////////////////////////////////////////

void read_file(RequestContext *ctx);
void read_dir(RequestContext *ctx);

void handle_on_stat(uv_fs_t *req);
void FileHandler::handle(ClientConnection *client) noexcept {
    // Get the absolute path of the requested file
	const Request &request = client->getRequest();
    string file = path::delUps(request.path);
    if (!request.path.empty() && request.path.back() == '/') {
        file = file + '/';
    }
    if (file != request.path) {
        // Check if the path contains up dirs
        const auto header = responseHeader(RES_REDIRECT_PERM, file.c_str());
        client->send(HEADER(header));
        client->close();
        return;
    }
    if (!this->base.empty()) {
        file = path::relpath(file, this->base);
    }
    file = path::join(this->folder, path::delUps(file));

    // Assert that the file matches the rules
    if (!validateFile(file)) {
        client->send(HEADER(ILLEGAL_FILE));
        client->close();
        return;
    }

    // Check if the path exists, and if it is a file or not
    RequestContext *ctx = request_allocator.allocate();
    ctx->file = file;
    ctx->req.data = ctx;
    ctx->req.loop = uv_default_loop();
	ctx->client = client;
    ctx->handler = this;
    uv_fs_stat(ctx->req.loop, &ctx->req, ctx->file.c_str(), handle_on_stat);
}
void handle_on_stat(uv_fs_t *req) {
    RequestContext *ctx = static_cast<RequestContext *>(req->data);

    if (req->result != 0) {
        // The file does not exist or does not have read permissions
        ctx->client->send(HEADER(DOES_NOT_EXIST));
        ctx->client->close();
        uv_fs_req_cleanup(req);
        request_allocator.deallocate(ctx);
        return;
    }
    string path = ctx->client->getRequest().path;

    if (S_IFDIR & req->statbuf.st_mode) {
        // The path is a directory
        uv_fs_req_cleanup(req);

        if (path.empty() || path.back() != '/') {
            // Make sure that the path ends with a forward slash for directories
            path += '/';
            const auto header = responseHeader(RES_REDIRECT_PERM, path.c_str());
            ctx->client->send(header.buf);
            ctx->client->close();
            request_allocator.deallocate(ctx);
            return;
        }

        read_dir(ctx);
        return;
    }
    if (S_IFREG & req->statbuf.st_mode) {
        // The path is a file
        uv_fs_req_cleanup(req);

        if (!path.empty() && path.back() == '/') {
            // Make sure that the path ends with a forward slash for directories
            const auto header = responseHeader(RES_REDIRECT_PERM, path.substr(0, path.length() - 1).c_str());
            ctx->client->send(header.buf);
            ctx->client->close();
            request_allocator.deallocate(ctx);
            return;
        }

        read_file(ctx);
        return;
    }

    ctx->client->send(HEADER(DOES_NOT_EXIST));
    ctx->client->close();
    uv_fs_req_cleanup(req);
    request_allocator.deallocate(ctx);
}


////////////////////////////////////////////////////////////////////////////////
//
// read_file()
//
// Read the file in `ctx->file` to the client
//
////////////////////////////////////////////////////////////////////////////////


void file_on_close(uv_fs_t *req);
void file_on_read(uv_fs_t *req);
void file_on_open(uv_fs_t *req);
void read_file(RequestContext *ctx) {
    uv_fs_open(ctx->req.loop, &ctx->req, ctx->file.c_str(), 0, UV_FS_O_RDONLY, file_on_open);
}
void file_on_open(uv_fs_t *req) {
    RequestContext *ctx = static_cast<RequestContext *>(req->data);

    if (req->result < 0) {
        // The file couldn't be opened
        ctx->client->send(HEADER(FILE_NOT_OPEN));
        ctx->client->close();
        uv_fs_req_cleanup(req);
        request_allocator.deallocate(ctx);
        return;
    }
    ctx->fd = req->result;
    ctx->buf = buffer_allocate();
    ctx->offset = 0;

    const auto header = responseHeader(RES_SUCCESS, mimeTypes.getType(ctx->file.c_str()));
    ctx->client->send(HEADER(header));

    uv_fs_read(req->loop, req, ctx->fd, &ctx->buf, 1, ctx->offset, file_on_read);
}
void file_on_read(uv_fs_t *req) {
    RequestContext *ctx = static_cast<RequestContext *>(req->data);

    if (req->result <= 0) {
        ctx->client->close();
        uv_fs_req_cleanup(req);
        buffer_deallocate(ctx->buf);
        uv_fs_close(req->loop, req, ctx->fd, file_on_close);
        return;
    }

    ctx->client->send(ctx->buf.base, req->result);
    ctx->offset += req->result;

    uv_fs_req_cleanup(req);
    uv_fs_read(req->loop, req, ctx->fd, &ctx->buf, 1, ctx->offset, file_on_read);
}
void file_on_close(uv_fs_t *req) {
    RequestContext *ctx = static_cast<RequestContext *>(req->data);

    uv_fs_req_cleanup(req);
    request_allocator.deallocate(ctx);
}

////////////////////////////////////////////////////////////////////////////////
//
// read_dir()
//
// Read the directory in `ctx->file` to the client. If the directory contains
// an index.* file, and that file is allowed to be viewed, then that file will
// be read to the client instead.
//
// If there is no index.* file, and ctx->handler->canReadDirs() is true, then
// the contents of the directory will be read to the client.
//
////////////////////////////////////////////////////////////////////////////////

void dir_on_scan(uv_fs_t *req);
void read_dir(RequestContext *ctx) {
    uv_fs_scandir(ctx->req.loop, &ctx->req, ctx->file.c_str(), 0, dir_on_scan);
}
void dir_on_scan(uv_fs_t *req) {
    RequestContext *ctx = static_cast<RequestContext *>(req->data);

    if (req->result < 0) {
        ctx->client->send(HEADER(FILE_NOT_OPEN));
        ctx->client->close();
        uv_fs_req_cleanup(req);
        request_allocator.deallocate(ctx);
        return;
    }

    vector<string> folders;
    vector<string> children;

    uv_dirent_t entry;
    while (uv_fs_scandir_next(req, &entry) != UV_EOF) {
        string name = entry.name;
        if (entry.type == UV_DIRENT_DIR) {
            folders.push_back(name);
        } else {
            children.push_back(name);
        }
        // Assert that the filename starts with index
        if (name.rfind("index.", 0) == string::npos) {
            continue;
        }
        string new_file = path::join(ctx->file, name);

        // Assert that the file is allowed
        if (!ctx->handler->validateFile(new_file)) {
            continue;
        }
        ctx->file = new_file;
        uv_fs_req_cleanup(req);
        read_file(ctx);
        return;
    }
    uv_fs_req_cleanup(req);

    if (!ctx->handler->canReadDirs()) {
        ctx->client->send(HEADER(DOES_NOT_EXIST));
        ctx->client->close();
        request_allocator.deallocate(ctx);
        return;
    }

    // Read the contents of the directory

    constexpr const auto header = responseHeader(RES_SUCCESS, "text/gemini");
    ctx->client->send(HEADER(header));
	
	const Request &request = ctx->client->getRequest();

    ostringstream oss;
    oss << "# DirectoryContents\n\n## " << request.path << "\n\n";

    oss << "=> " << path::dirname(request.path) << " back\n\n";

    for (string folder : folders) {
        oss << "=> " << path::join(request.path, folder) << "/ " << folder << "/\n";
    }

    oss << "\n";

    for (string file : children) {
        oss << "=> " << path::join(request.path, file) << " " << file << "\n";
    }

    string response = oss.str();
    ctx->client->send(response.c_str(), response.length());
    ctx->client->close();
    request_allocator.deallocate(ctx);
}

////////////////////////////////////////////////////////////////////////////////
//
// FileHandlerFactory
//
////////////////////////////////////////////////////////////////////////////////

shared_ptr<Handler> FileHandlerFactory::createHandler(YAML::Node settings, string dir) {
    string host = getProperty<string>(settings, HOST, "");
    string folder = getProperty<string>(settings, FOLDER);
    string base = getProperty<string>(settings, BASE, "");
    bool read_dirs = getProperty<bool>(settings, READ_DIRS, true);

    vector<regex> rules;
    if (settings[RULES].IsDefined()) {
        if (!settings[RULES].IsSequence()) {
            throw InvalidSettingsException(settings[RULES].Mark(), "'" + RULES + "' must be a sequence");
        }
        try {
            for (auto rule : settings[RULES]) {
                string rule_value = rule.as<string>();
                rules.push_back(regex(rule_value, re_consts::ECMAScript | re_consts::icase | re_consts::optimize));
            }
        } catch (YAML::RepresentationException e) {
            throw InvalidSettingsException(e.mark, e.msg);
        }
    }

    std::vector<string> cgi_types;
    if (settings[CGI_TYPES].IsDefined()) {
        if (!settings[CGI_TYPES].IsSequence()) {
            throw InvalidSettingsException(settings[CGI_TYPES].Mark(), "'" + CGI_TYPES + "' must be a sequence");
        }
        try {
            for (auto cgi_type : settings[CGI_TYPES]) {
                cgi_types.push_back(cgi_type.as<string>());
            }
        } catch (YAML::RepresentationException e) {
            throw InvalidSettingsException(e.mark, e.msg);
        }
    }

    if (path::isrel(folder)) {
        folder = path::join(dir, folder);
    }
    folder = path::delUps(folder);

    return make_shared<FileHandler>(
        host,
        folder,
        base,
        read_dirs,
        rules,
        cgi_types
    );
}