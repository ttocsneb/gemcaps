#ifndef __GEMCAPS_SETTINGS__
#define __GEMCAPS_SETTINGS__

#include <string>
#include <vector>
#include <map>
#include <memory>

#include <yaml-cpp/yaml.h>

#include "glob.hpp"

/**
 * The base class for all settings objects
 */
class Settings {
public:
    /**
     * Load the settings from the yaml node
     * 
     * @param settings the yaml node to load from
     */
    virtual void load(YAML::Node &settings) = 0;
    /**
     * Load the settings from a yaml file
     * 
     * @param path yaml file to load
     */
    virtual void loadFile(const std::string &path);
};

/**
 * The base class for all handler settings
 * 
 * <code>
 * type: string
 * path: string
 * rules: list<string>
 * </code>
 */
class HandlerSettings : public Settings {
private:
    std::string type;
    Glob path;
    std::vector<Glob> rules;
public:
    virtual void load(YAML::Node &settings);

    /**
     * The type is the type of handler
     * 
     * @return the type of handler
     */
    const std::string &getType() const { return type; }
    /**
     * Get the allowed path for the handler
     * 
     * @return the path
     */
    const Glob &getPath() const { return path; }
    /**
     * Get the rules for the handler
     * 
     * @return the rules
     */
    const std::vector<Glob> &getRules() const { return rules; }
};

/**
 * A file handler which serves files from disk
 * 
 * <code>
 * type: string
 * path: string
 * rules: list<string>
 * root: string
 * </code>
 */
class FileSettings : public HandlerSettings {
private:
    std::string root;
public:
    virtual void load(YAML::Node &settings);

    /**
     * Get the root folder for the files to read from
     * 
     * @return the root folder of the files
     */
    const std::string &getRoot() const { return root; }
};

/**
 * A GSGI handler for a gsgi process
 * 
 * <code>
 * type: string
 * path: string
 * rules: list<string>
 * command: list<string>
 * environment: map<string, string>
 * </code>
 */
class GSGISettings : public HandlerSettings {
private:
    std::vector<std::string> command;
    std::map<std::string, std::string> environment;
public:
    virtual void load(YAML::Node &settings);

    /**
     * Get the command that starts the gsgi server
     * 
     * @return the gsgi command
     */
    const std::vector<std::string> &getCommand() const { return command; }
    /**
     * Get the environment variables for the gsgi server
     * 
     * @return the environment variables
     */
    const std::map<std::string, std::string> &getEnvironment() const { return environment; }
};

/**
 * The settings for a gemini capsule
 * 
 * <code>
 * host: string
 * port: int
 * handlers: list<HandlerSettings>
 * </code>
 */
class CapsuleSettings : public Settings {
private:
    Glob host;
    int port;
    std::string listen;
    std::string cert;
    std::string key;

    std::vector<std::shared_ptr<HandlerSettings>> handlers;

    void _load_handler(YAML::Node &settings);
public:
    static inline const std::string TYPE_FILE = "file";
    static inline const std::string TYPE_GSGI = "gsgi";

    virtual void load(YAML::Node &settings);

    /**
     * Get the host for the capsule
     * 
     * @return the host
     */
    const Glob &getHost() const { return host; }
    /**
     * Get the port for the capsule
     * 
     * @return the port
     */
    int getPort() const { return port; }
    /**
     * Get the listen host for the capsule
     * 
     * @return listen
     */
    const std::string &getListen() const { return listen; }
    /**
     * Get the certificate file for the capsule
     * 
     * This is only used if the host or port is unique!
     * 
     * @return certificate file
     */
    const std::string &getCert() const { return cert; }
    /**
     * Get the key file for the capsule
     * 
     * This is only used if the host or port is unique!
     * 
     * @return key file
     */
    const std::string &getKey() const { return key; }
    /**
     * Get the list of handlers for the capsule
     * 
     * @return file handlers
     */
    const std::vector<std::shared_ptr<HandlerSettings>> &getHandlers() { return handlers; }
};

/**
 * The settings for GemCaps
 * 
 * <code>
 * cert: string
 * key: string
 * capsules: string
 * </code>
 */
class GemCapSettings : public Settings {
private:
    std::string cert;
    std::string key;
    std::string listen;
    int port;
    std::string capsules;
public:
    virtual void load(YAML::Node &settings);

    /**
     * Get the default certificate file
     * 
     * @return certificate file
     */
    const std::string &getCert() const { return cert; }
    /**
     * Get the default key file
     * 
     * @return key file
     */
    const std::string &getKey() const { return key; }
    /**
     * Get the directory of capsule settings
     * 
     * @return capsules directory
     */
    const std::string &getCapsules() const { return capsules; }

    /**
     * Get the host that the server will listen on
     * 
     * The default value is "0.0.0.0"
     * 
     * @return listen
     */
    const std::string &getListen() const { return listen; }

    /**
     * Get the port for the server
     * 
     * The default value is 1965
     * 
     * @return port
     */
    int getPort() const { return port; }
};

#endif