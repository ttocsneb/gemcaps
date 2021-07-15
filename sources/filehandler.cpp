#include "filehandler.hpp"

#include <sstream>

#include "gemcaps/util.hpp"
#include "gemcaps/uvutils.hpp"
#include "gemcaps/pathutils.hpp"
#include "gemcaps/MimeTypes.h"


using std::shared_ptr;
using std::make_shared;
using std::string;
using std::vector;
using std::regex;
using std::ostringstream;

namespace re_consts = std::regex_constants;

const char *ILLEGAL_FILE = responseHeader(RES_NOT_FOUND, "Illegal File");
const char *DOES_NOT_EXIST = responseHeader(RES_NOT_FOUND, "File does not exist");
const char *FILE_NOT_OPEN = responseHeader(RES_GONE, "File could not be opened");

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
    OBufferPipe *body;
    const Request *request;
    const FileHandler *handler;
};
ReusableAllocator<RequestContext> request_allocator;

void read_file(RequestContext *ctx);
void read_dir(RequestContext *ctx);

void handle_on_stat(uv_fs_t *req);
void FileHandler::handle(const Request *request, OBufferPipe *body) noexcept {
    // Get the absolute path of the requested file
    string file = path::delUps(request->path);
    if (file != request->path) {
        // TODO Check if works properly with paths that end in '/'
        const char *header = responseHeader(RES_REDIRECT_PERM, file.c_str());
        body->write(header, strlen(header));
        body->close();
        return;
    }
    if (!this->base.empty()) {
        file = path::relpath(file, this->base);
    }
    file = path::join(this->folder, path::delUps(file));

    // Assert that the file matches the rules
    if (!validateFile(file)) {
        body->write(ILLEGAL_FILE, strlen(ILLEGAL_FILE));
        body->close();
        return;
    }

    // Check if the path exists, and if it is a file or not
    RequestContext *ctx = request_allocator.allocate();
    ctx->file = file;
    ctx->req.data = ctx;
    ctx->req.loop = uv_default_loop();
    ctx->body = body;
    ctx->request = request;
    ctx->handler = this;
    uv_fs_stat(ctx->req.loop, &ctx->req, ctx->file.c_str(), handle_on_stat);
}
void handle_on_stat(uv_fs_t *req) {
    RequestContext *ctx = static_cast<RequestContext *>(req->data);

    if (req->result != 0) {
        // The file does not exist or does not have read permissions
        ctx->body->write(DOES_NOT_EXIST, strlen(DOES_NOT_EXIST));
        ctx->body->close();
        uv_fs_req_cleanup(req);
        request_allocator.deallocate(ctx);
        return;
    }

    if (S_ISDIR(req->statbuf.st_mode)) {
        // The path is a directory
        uv_fs_req_cleanup(req);
        read_dir(ctx);
        return;
    }
    if (S_ISREG(req->statbuf.st_mode)) {
        // The path is a file
        uv_fs_req_cleanup(req);
        read_file(ctx);
        return;
    }

    ctx->body->write(DOES_NOT_EXIST, strlen(DOES_NOT_EXIST));
    ctx->body->close();
    uv_fs_req_cleanup(req);
    request_allocator.deallocate(ctx);
}

void file_on_close(uv_fs_t *req);
void file_on_read(uv_fs_t *req);
void file_on_open(uv_fs_t *req);
void read_file(RequestContext *ctx) {
    uv_fs_open(ctx->req.loop, &ctx->req, ctx->file.c_str(), 0, UV_FS_O_RDONLY, file_on_open);
}
void file_on_open(uv_fs_t *req) {
    RequestContext *ctx = static_cast<RequestContext *>(req->data);

    if (req->result != 0) {
        // The file couldn't be opened
        ctx->body->write(FILE_NOT_OPEN, strlen(FILE_NOT_OPEN));
        ctx->body->close();
        uv_fs_req_cleanup(req);
        request_allocator.deallocate(ctx);
        return;
    }
    ctx->fd = req->file;
    ctx->buf = buffer_allocate();
    ctx->offset = 0;

    const char *header = responseHeader(RES_SUCCESS, mimeTypes.getType(ctx->file.c_str()));
    ctx->body->write(header, strlen(header));

    uv_fs_read(req->loop, req, ctx->fd, &ctx->buf, 1, ctx->offset, file_on_read);
}
void file_on_read(uv_fs_t *req) {
    RequestContext *ctx = static_cast<RequestContext *>(req->data);

    if (req->result <= 0) {
        ctx->body->close();
        uv_fs_req_cleanup(req);
        buffer_deallocate(ctx->buf);
        uv_fs_close(req->loop, req, ctx->fd, file_on_close);
        return;
    }

    ctx->body->write(ctx->buf.base, req->result);
    ctx->offset += req->result;

    uv_fs_req_cleanup(req);
    uv_fs_read(req->loop, req, ctx->fd, &ctx->buf, 1, ctx->offset, file_on_read);
}
void file_on_close(uv_fs_t *req) {
    RequestContext *ctx = static_cast<RequestContext *>(req->data);

    uv_fs_req_cleanup(req);
    request_allocator.deallocate(ctx);
}


void dir_on_scan(uv_fs_t *req);
void read_dir(RequestContext *ctx) {
    uv_fs_scandir(ctx->req.loop, &ctx->req, ctx->file.c_str(), 0, dir_on_scan);
}
void dir_on_scan(uv_fs_t *req) {
    RequestContext *ctx = static_cast<RequestContext *>(req->data);

    if (req->result < 0) {
        ctx->body->write(FILE_NOT_OPEN, strlen(FILE_NOT_OPEN));
        ctx->body->close();
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
        uv_fs_req_cleanup(req);
        read_file(ctx);
    }
    uv_fs_req_cleanup(req);

    if (!ctx->handler->canReadDirs()) {
        ctx->body->write(DOES_NOT_EXIST, strlen(DOES_NOT_EXIST));
        ctx->body->close();
        request_allocator.deallocate(ctx);
        return;
    }

    // Read the contents of the directory

    const char *header = responseHeader(RES_SUCCESS, "text/gemini");
    ctx->body->write(header, strlen(header));

    ostringstream oss;
    oss << "# DirectoryContents\n\n## " << ctx->request->path << "\n\n";

    oss << "=> " << path::dirname(ctx->request->path) << " back\n\n";

    for (string folder : folders) {
        oss << "=> " << path::join(ctx->request->path, folder) << "/ " << folder << "/\n";
    }

    oss << "\n";

    for (string file : children) {
        oss << "=> " << path::join(ctx->request->path, file) << " " << file << "\n";
    }

    string response = oss.str();
    ctx->body->write(response.c_str(), response.length());
    ctx->body->close();
    request_allocator.deallocate(ctx);
}


shared_ptr<Handler> FileHandlerFactory::createHandler(YAML::Node settings) {
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

    return make_shared<FileHandler>(
        host,
        folder,
        base,
        read_dirs,
        rules,
        cgi_types
    );
}