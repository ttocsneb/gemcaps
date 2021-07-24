#ifndef __GEMCAPS_LOADER__
#define __GEMCAPS_LOADER__

#include <memory>
#include <vector>

#include <yaml-cpp/yaml.h>

#include <uv.h>

#include "server.hpp"
#include "gemcaps/handler.hpp"

inline const std::string HOST = "host";
inline const std::string PORT = "port";
inline const std::string CERT = "cert";
inline const std::string KEY = "key";

inline const std::string HANDLER = "handler";

/**
 * Load a server from a YAML File
 * 
 * @param settings settings to use
 * 
 * @return the loaded server
 */
std::shared_ptr<SSLServer> loadServer(YAML::Node settings, std::string dir, uv_loop_t *loop = nullptr);

class HandlerLoader {
private:
    phmap::flat_hash_map<std::string, std::shared_ptr<HandlerFactory>> factories;
public:
    /**
     * Load the factories into memory
     */
    void loadFactories() noexcept;

    /**
     * Load a handler from a YAML File
     * 
     * @param settings settings to use
     * 
     * @return the loaded handler
     */
    std::shared_ptr<Handler> loadHandler(YAML::Node settings, std::string dir);
};

#endif