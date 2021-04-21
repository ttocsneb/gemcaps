#include "settings.hpp"

using std::map;
using std::vector;
using std::string;

void Settings::load(const string &path) {
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
        rules = settings["rules"].as<vector<string>>();
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
    if (type == "file") {
        FileSettings conf;
        conf.load(settings);
        files.push_back(conf);
        return;
    }
    if (type == "gsgi") {
        GSGISettings conf;
        conf.load(settings);
        gsgi.push_back(conf);
        return;
    }
    // TODO Error
}


void CapsuleSettings::load(YAML::Node &settings) {
    files.clear();
    gsgi.clear();
    if (settings["host"].IsDefined()) {
        host = settings["host"].as<string>();
    }
    if (settings["port"].IsDefined()) {
        port = settings["port"].as<int>();
    }
    if (settings["handlers"].IsDefined()) {
        YAML::Node node = settings["handlers"];
        for (auto it = node.begin(); it != node.end(); ++it) {
            _load_handler(it->second);
        }
    }
}

void GemCapSettings::load(YAML::Node &settings) {
    cert = settings["cert"].as<string>();
    key = settings["key"].as<string>();
    if (settings["capsules"].IsDefined()) {
        capsules = settings["capsules"].as<string>();
    }
}