#include "settings.hpp"

#include "main.hpp"

#include <iostream>

#include <filesystem>

#include <uv.h>

namespace fs = std::filesystem;

using std::map;
using std::vector;
using std::string;
using std::shared_ptr;
using std::make_shared;

using std::cerr;
using std::endl;


namespace YAML {
    template<>
    struct convert<Glob> {
        static Node encode(const Glob &rhs) {
            return Node(rhs.str());
        }
        static bool decode(const Node &node, Glob &rhs) {
            if (!node.IsScalar()) {
                return false;
            }
            rhs = node.as<string>();
            return true;
        }
    };
}

void Settings::loadFile(const string &path) {
    YAML::Node settings = YAML::LoadFile(path);
    load(settings);
}


void HandlerSettings::load(YAML::Node &settings) {
    type = settings["type"].as<string>();
    if (settings["path"].IsDefined()) {
        path = settings["path"].as<string>();
    }
    if (settings["rules"].IsDefined()) {
        rules = settings["rules"].as<vector<Glob>>();
    }
}

void FileSettings::load(YAML::Node &settings) {
    allowedDirs.clear();
    HandlerSettings::load(settings);
    root = settings["root"].as<string>();
    if (settings["cacheTime"].IsDefined()) {
        cacheTime = settings["cacheTime"].as<float>();
    } else {
        cacheTime = 0;
    }
    if (settings["readDirs"].IsDefined()) {
        readDirs = settings["readDirs"].as<bool>();
    } else {
        readDirs = false;
    }
    vector<string> items;
    if (settings["allowedDirs"].IsDefined()) {
        vector<string> items = settings["allowedDirs"].as<vector<string>>();
    } else {
        items.push_back(root);
    }

    // Add the allowed directories
    for (string item : items) {
        fs::path p = item;
        p = p.make_preferred();
        uv_fs_t req;
        int res = uv_fs_realpath(uv_default_loop(), &req, p.c_str(), nullptr);
        if (res == UV_ENOSYS) {
            ERROR("Unsupported operating system");
            return;
        }
        if (req.ptr) {
            item = (char*)req.ptr;
        } else {
            WARN("The path \"" << item << "\" does not exist");
        }

        if (item.find('/', item.length() - 1) != string::npos) {
            item += '*';
        } else {
            item += "/*";
        }
        DEBUG("Allowed Dir: " << item);
        allowedDirs.push_back(item);
        uv_fs_req_cleanup(&req);
    }
}

void GSGISettings::load(YAML::Node &settings) {
    HandlerSettings::load(settings);
    command = settings["command"].as<vector<string>>();
    if (settings["environment"].IsDefined()) {
        environment = settings["environment"].as<map<string, string>>();
    }
}


void CapsuleSettings::_load_handler(YAML::Node &settings) {
    string type = settings["type"].as<string>();
    shared_ptr<HandlerSettings> conf;
    if (type == TYPE_FILE) {
        conf = make_shared<FileSettings>();
    } else if (type == TYPE_GSGI) {
        conf = make_shared<GSGISettings>();
    } else {
        // TODO Error
        return;
    }
    conf->load(settings);
    handlers.push_back(conf);
}


void CapsuleSettings::load(YAML::Node &settings) {
    handlers.clear();
    if (settings["host"].IsDefined()) {
        host = settings["host"].as<Glob>();
    } else {
        host = "*";
    }
    if (settings["port"].IsDefined()) {
        port = settings["port"].as<int>();
    } else {
        port = 0;
    }
    if (settings["listen"].IsDefined()) {
        listen = settings["listen"].as<string>();
    } else {
        listen = "";
    }
    if (settings["cert"].IsDefined()) {
        cert = settings["cert"].as<string>();
    } else {
        listen = "";
    }
    if (settings["key"].IsDefined()) {
        key = settings["key"].as<string>();
    } else {
        key = "";
    }
    if (settings["handlers"].IsDefined()) {
        YAML::Node node = settings["handlers"];
        for (YAML::iterator it = node.begin(); it != node.end(); ++it) {
            YAML::Node n = *it;
            _load_handler(n);
        }
    }
}

void GemCapSettings::load(YAML::Node &settings) {
    try {
        cert = settings["cert"].as<string>();
    } catch (std::exception &err) {
        cerr << "Error: cert is a required option!" << endl;
        throw err;
    }
    try {
        key = settings["key"].as<string>();
    } catch (std::exception &err) {
        cerr << "Error: key is a required option!" << endl;
        throw err;
    }
    if (settings["capsules"].IsDefined()) {
        capsules = settings["capsules"].as<string>();
    } else {
        capsules = ".";
    }
    if (settings["listen"].IsDefined()) {
        listen = settings["listen"].as<string>();
    } else {
        listen = "0.0.0.0";
    }
    if (settings["port"].IsDefined()) {
        port = settings["port"].as<int>();
    } else {
        port = 1965;
    }
}