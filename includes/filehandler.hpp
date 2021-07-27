#ifndef __GEMCAPS_FILEHANDLER__
#define __GEMCAPS_FILEHANDLER__

#include <memory>
#include <vector>
#include <regex>

#include <parallel_hashmap/phmap.h>

#include "gemcaps/settings.hpp"
#include "gemcaps/handler.hpp"

class FileHandler : public Handler {
private:
    const std::string host;
    const std::string folder;
    const std::string base;
    const bool read_dirs;
    const std::vector<std::regex> rules;
    const std::vector<std::string> cgi_types;
    const std::string cgi_lang;
    const phmap::flat_hash_map<std::string, std::string> cgi_vars;
public:
    FileHandler(
            std::string host,
            std::string folder,
            std::string base,
            bool read_dirs,
            std::vector<std::regex> rules,
            std::vector<std::string> cgi_types,
            std::string cgi_lang,
            phmap::flat_hash_map<std::string, std::string> cgi_vars)
        : host(host),
          folder(folder),
          base(base),
          read_dirs(read_dirs),
          rules(rules),
          cgi_types(cgi_types),
          cgi_lang(cgi_lang),
          cgi_vars(cgi_vars) {}

    /**
     * Check if this handler is allowed to display directory contents
     * 
     * @param file file
     * 
     * @return whether the handler is allowed to read directories to the client
     */
    bool canReadDirs() const noexcept { return read_dirs; }
    /**
     * Check if the given file is allowed to be sent to clients
     * 
     * @param file file
     * 
     * @return whether the file is allowed to be sent to clients
     */
    bool validateFile(std::string file) const noexcept;
    /**
     * Check if the file is allowed to be executed
     * 
     * @note this does not check if the file itself is executable
     * 
     * @param file file
     * 
     * @return whether the file is allowed to be executed
     */
    bool isExecutable(std::string file) const noexcept;

    /**
     * Generate the environment variables for a cgi script
     * 
     * @param file script file
     * @param client client connection
     * 
     * @return the generated environment variables
     */
    phmap::flat_hash_map<std::string, std::string> generateEnvironment(const std::string &file, const ClientConnection *client) const noexcept;

    // Override Handler
    bool shouldHandle(std::string host, std::string path) noexcept;
    void handle(ClientConnection *client) noexcept;
};


class FileHandlerFactory : public HandlerFactory {
public:
    inline static const std::string HOST = "host";
    inline static const std::string FOLDER = "folder";
    inline static const std::string BASE = "base";
    inline static const std::string READ_DIRS = "readDirs";
    inline static const std::string RULES = "rules";
    inline static const std::string CGI_TYPES = "cgiFiletypes";
    inline static const std::string CGI_LANG = "cgiLang";
    inline static const std::string CGI_VARS = "cgiVars";

    // Override HandlerFactory
    std::shared_ptr<Handler> createHandler(YAML::Node settings, std::string dir);
};

#endif