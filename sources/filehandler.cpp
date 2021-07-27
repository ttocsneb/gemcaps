#include "filehandler.hpp"

#include <sstream>

#include <uv.h>

#include <stdio.h>

#include "gemcaps/util.hpp"
#include "gemcaps/uvutils.hpp"
#include "gemcaps/pathutils.hpp"
#include "gemcaps/MimeTypes.h"
#include "gemcaps/log.hpp"
#include "gemcaps/executor.hpp"


using std::shared_ptr;
using std::make_shared;
using std::string;
using std::vector;
using std::regex;
using std::ostringstream;

namespace re_consts = std::regex_constants;

constexpr const auto CGI_ERROR = responseHeader<32>(RES_ERROR_CGI, "Could not run script");
constexpr const auto ILLEGAL_FILE = responseHeader<32>(RES_NOT_FOUND, "Illegal File");
constexpr const auto DOES_NOT_EXIST = responseHeader<32>(RES_NOT_FOUND, "File does not exist");
constexpr const auto FILE_NOT_OPEN = responseHeader<32>(RES_GONE, "File could not be opened");

ReusableAllocator<uv_pipe_t> pipe_allocator;

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

phmap::flat_hash_map<string, string> FileHandler::generateEnvironment(const string &file, const ClientConnection *client) const noexcept {
    const Request &request = client->getRequest();
    phmap::flat_hash_map<string, string> env = cgi_vars;

    size_t pos = request.header.find('\r');
    string url = request.header.substr(0, pos);

    env["GATEWAY_INTERFACE"] = "CGI/1.1";
    env["GEMINI_DOCUMENT_ROOT"] = folder;
    env["GEMINI_SCRIPT_FILENAME"] = file;
    env["GEMINI_URL"] = url;
    env["GEMINI_URL_PATH"] = request.path;
    env["LANG"] = cgi_lang;
    env["LC_COLLATE"] = "C";
    env["PATH"] = Executor::getPath();
    env["QUERY_STRING"] = request.query;
    env["REMOTE_ADDR"] = ""; // TODO
    env["REMOTE_HOST"] = ""; // TODO
    env["REQUEST_METHOD"] = "";
    env["SCRIPT_NAME"] = "/" + path::relpath(file, folder);
    env["SERVER_NAME"] = request.host;
    char port[6];
    sprintf(port, "%d", request.port);
    env["SERVER_PORT"] = port;
    env["SERVER_PROTOCOL"] = "GEMINI";
    env["SERVER_SOFTWARE"] = SOFTWARE;

    return env;
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

void on_client_closed(ClientConnection *client, void *ctx) {
    RequestContext *request = static_cast<RequestContext *>(ctx);
    request_allocator.deallocate(request);
    LOG_DEBUG("Connection Closed");
}


////////////////////////////////////////////////////////////////////////////////
//
// FileHandler::handle()
//
// Handle the file request and send the response to the client.
//
////////////////////////////////////////////////////////////////////////////////

void read_file(RequestContext *ctx);
void read_dir(RequestContext *ctx);
void run_cgi(RequestContext *ctx);

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
    client->setClientCloseCallback(on_client_closed, ctx);
    uv_fs_stat(ctx->req.loop, &ctx->req, ctx->file.c_str(), handle_on_stat);
}
void handle_on_stat(uv_fs_t *req) {
    RequestContext *ctx = static_cast<RequestContext *>(req->data);

    if (req->result != 0) {
        // The file does not exist or does not have read permissions
        ctx->client->send(HEADER(DOES_NOT_EXIST));
        ctx->client->close();
        uv_fs_req_cleanup(req);
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
            return;
        }

        read_file(ctx);
        return;
    }

    ctx->client->send(HEADER(DOES_NOT_EXIST));
    ctx->client->close();
    uv_fs_req_cleanup(req);
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
    if (ctx->handler->isExecutable(ctx->file)) {
        // Run the file if it is a cgi script
        run_cgi(ctx);
        return;
    }

    uv_fs_open(ctx->req.loop, &ctx->req, ctx->file.c_str(), 0, UV_FS_O_RDONLY, file_on_open);
}
void file_on_open(uv_fs_t *req) {
    RequestContext *ctx = static_cast<RequestContext *>(req->data);

    if (req->result < 0) {
        // The file couldn't be opened
        ctx->client->send(HEADER(FILE_NOT_OPEN));
        ctx->client->close();
        uv_fs_req_cleanup(req);
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
        uv_fs_close(req->loop, req, ctx->fd, file_on_close);
        return;
    }

    ctx->client->send(ctx->buf.base, req->result);
    ctx->offset += req->result;

    uv_fs_req_cleanup(req);
    uv_fs_read(req->loop, req, ctx->fd, &ctx->buf, 1, ctx->offset, file_on_read);
}
void file_on_close(uv_fs_t *req) {
    uv_fs_req_cleanup(req);
}

////////////////////////////////////////////////////////////////////////////////
//
// run_cgi()
//
// Run the cgi script in `ctx->file` and redirect the output to the client.
//
////////////////////////////////////////////////////////////////////////////////

class CGIRunner;

typedef void (*onCGIRunnerClose)(CGIRunner *runner);

class CGIRunner : public ExecutorContext {
private:
    RequestContext *ctx;
    uv_pipe_t *response;
    Executor executor;
    bool closing = false;
    const onCGIRunnerClose close_cb;

    static void __on_pipe_closed(uv_handle_t *handle) {
        if (handle->data != nullptr) {
            LOG_DEBUG("Pipe Closed");
            CGIRunner *runner = static_cast<CGIRunner *>(handle->data);
            runner->response = nullptr;
            runner->closing = false;
        }
        pipe_allocator.deallocate((uv_pipe_t *)handle);
    }

    static void __on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) noexcept {
        CGIRunner *runner = static_cast<CGIRunner*>(stream->data);
        if (stream->data == nullptr) {
            buffer_deallocate(*buf);
            return;
        }
        if (runner == nullptr) {
            LOG_WARN("Runner is not a CGIRunner");
            buffer_deallocate(*buf);
            return;
        }
        if (nread < 0) {
            LOG_ERROR("Could not read data from pipe: '" << uv_strerror(nread) << "'");
            buffer_deallocate(*buf);
            runner->close();
            return;
        }

        runner->ctx->client->send(buf->base, nread);

        buffer_deallocate(*buf);
    }

    static void __on_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) noexcept {
        uv_buf_t alloc = buffer_allocate();
        buf->base = alloc.base;
        buf->len = alloc.len;
    }

    static void __on_client_closed(ClientConnection *client, void *ctx) {
        LOG_DEBUG("Client Closed");
        CGIRunner *runner = static_cast<CGIRunner *>(ctx);
        runner->close_cb(runner);
    }

public:
    CGIRunner(RequestContext *ctx, phmap::flat_hash_map<string, string> env, vector<string> args, onCGIRunnerClose on_close)
            : ctx(ctx),
              executor(ctx->file, env, args),
              response(pipe_allocator.allocate()),
              close_cb(on_close) {
        ctx->client->setClientCloseCallback(__on_client_closed, this);
        executor.setContext(this);

        uv_pipe_init(ctx->req.loop, response, false);
        response->data = this;
    }
    
    ~CGIRunner() {
        request_allocator.deallocate(ctx);

        if (response != nullptr) {
            response->data = nullptr;
            if (!closing) {
                close();
            }
        }
    }

    /**
     * Run the cgi script
     */
    void run() noexcept {
        uv_file pipe[2];
        uv_pipe(pipe, UV_NONBLOCK_PIPE, 0);

        uv_pipe_open(response, pipe[0]);
        uv_read_start((uv_stream_t *)response, __on_alloc, __on_read);

        int error = executor.spawn(ctx->req.loop, -1, pipe[1]);
        if (error != 0) {
            LOG_ERROR("Could not start CGI Script '" << ctx->file << "': " << uv_strerror(error));
            ctx->client->send(CGI_ERROR.buf, CGI_ERROR.length());
            close();
            return;
        }
        return;
    }

    /**
     * Close the client connection and make sure the cgi script stops
     */
    void close() {
        closing = true;
        if (executor.is_alive()) {
            executor.signal(SIGSTOP);
        }
        ctx->client->close();
        uv_close((uv_handle_t *)response, __on_pipe_closed);
    }

    // override ExecutorContext
    void onExit(Executor *executor, int64_t exit_status, int term_signal) {
        LOG_DEBUG("Executor Exited");
        close();
    }
};


void on_cgi_close(CGIRunner *runner);
void run_cgi(RequestContext *ctx) {
    phmap::flat_hash_map<string, string> env = ctx->handler->generateEnvironment(ctx->file, ctx->client);
    vector<string> args;

    CGIRunner *runner = new CGIRunner(ctx, env, args, on_cgi_close);
    runner->run();
}
void on_cgi_close(CGIRunner *runner) {
    delete runner;
    LOG_DEBUG("Runner deleted");
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
    string cgi_lang = getProperty<string>(settings, CGI_LANG, "en_US.UTF-8");

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

    phmap::flat_hash_map<string, string> cgi_vars;
    if (settings[CGI_VARS].IsDefined()) {
        if (!settings[CGI_VARS].IsMap()) {
            throw InvalidSettingsException(settings[CGI_VARS].Mark(), "'" + CGI_VARS + "' must be a map");
        }
        try {
            for (auto var : settings[CGI_VARS]) {
                cgi_vars.insert({var.first.as<string>(), var.second.as<string>()});
            }
        } catch(YAML::RepresentationException &e) { 
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
        cgi_types,
        cgi_lang,
        cgi_vars
    );
}