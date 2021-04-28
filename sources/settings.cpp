#include "settings.hpp"

#include <iostream>

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
    rules.clear();
    type = settings["type"].as<string>();
    if (settings["path"].IsDefined()) {
        path = settings["path"].as<string>();
    }
    if (settings["rules"].IsDefined()) {
        rules = settings["rules"].as<vector<Glob>>();
    }
}

void FileSettings::load(YAML::Node &settings) {
    HandlerSettings::load(settings);
    if (settings["root"].IsDefined()) {
        root = settings["root"].as<string>();
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
        for (auto it = node.begin(); it != node.end(); ++it) {
            _load_handler(it->second);
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