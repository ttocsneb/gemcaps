#ifndef __GEMCAPS_FILEHANDLER__
#define __GEMCAPS_FILEHANDLER__

#include <memory>
#include <vector>

#include "settings.hpp"
#include "manager.hpp"

class FileHandler;

class FileContext {
public:
    enum State {
        REALPATH,
        READFILE,
        READY
    };
private:
    uv_fs_t req;
    const GeminiRequest request;
    std::string path;
    std::string body;
    std::shared_ptr<FileSettings> settings;
    State state;
    FileHandler *handler;
    std::vector<std::string> files;
    int file;
    unsigned long offset;
    char rdbuf[1024];
	uv_buf_t buf;
public:
    FileContext(FileHandler *handler, const GeminiRequest &request, std::shared_ptr<FileSettings> settings)
        : handler(handler),
          request(request),
          settings(settings) {
		buf.base = rdbuf;
		buf.len = sizeof(rdbuf);
	}
    uv_fs_t *getReq() { return &req; }
    const GeminiRequest &getRequest() const { return request; }

    void setPath(const std::string &path) { this->path = path; }
    const std::string &getPath() const { return path; }

    void setState(State state) { this->state = state; } 
    State getState() const { return state; }

    FileHandler *getHandler() const { return handler; }

    std::vector<std::string> &getFiles() { return files; }

    void setFile(uv_file file) { this->file = file; }
    int getFile() const { return file; }

    uv_buf_t *getBuf() { return &buf; }
    std::string &getBody() { return body; }

    void setOffset(unsigned long offset) { this->offset = offset; }
    unsigned long getOffset() const { return offset; }
};

/**
 * The file request handler
 */
class FileHandler : public Handler {
private:
    std::shared_ptr<FileSettings> settings;
public:
    FileHandler(Cache *cache, std::shared_ptr<FileSettings> settings, Glob host, int port)
        : settings(settings),
          Handler(cache, host, port) {}
    void handle(SSLClient *client, const GeminiRequest &request);

    void internalError(SSLClient *client, FileContext *context);
    void sendCache(SSLClient *client, FileContext *context);
    void gotRealPath(SSLClient *client, FileContext *context, std::string realPath);
    void gotInvalidPath(SSLClient *client, FileContext *context);
    void gotAccess(SSLClient *client, FileContext *context, ssize_t result);
    void gotStat(SSLClient *client, FileContext *context, uv_stat_t *statbuf);
    void onScandir(SSLClient *client, FileContext *context, std::map<std::string, uv_dirent_type_t> &dirs);
    void readFile(SSLClient *client, FileContext *context);
    void onFileOpen(SSLClient *client, FileContext *context, uv_file file);
    void onFileRead(SSLClient *client, FileContext *context, unsigned int read);
};

#endif