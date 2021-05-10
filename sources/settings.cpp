#include "settings.hpp"

#include "main.hpp"

#include <iostream>

#include <uv.h>

namespace fs = std::filesystem;

using std::map;
using std::vector;
using std::string;
using std::shared_ptr;
using std::make_shared;

using std::cerr;
using std::endl;
using std::regex;


namespace YAML {
    template<>
    struct convert<Regex> {
        static Node encode(const Regex &rhs) {
            return Node(rhs.getPattern());
        }
        static bool decode(const Node &node, Regex &rhs) {
            if (!node.IsScalar()) {
                return false;
            }
            rhs = Regex(node.as<string>(), regex::ECMAScript | regex::optimize);
            return true;
        }
    };
}

string Settings::to_absolute_path(const string& path) {
	fs::path p = fs::path(path).make_preferred();
	if (p.is_relative()) {
		LOG_DEBUG("path " << p << " is relative");
		LOG_DEBUG("Adding path " << directory << " to " << p << ": " << (directory / p));
		return (directory / p).string();
	}
	LOG_DEBUG("path " << p << " is not relative");
	return p.string();
}

void Settings::loadFile(const string &path) {
	fs::path p = path;
	setDirectory(p.parent_path());
    YAML::Node settings = YAML::LoadFile(path);
    load(settings);
}


void HandlerSettings::load(YAML::Node &settings) {
    type = settings["type"].as<string>();
    if (settings["path"].IsDefined()) {
        path = settings["path"].as<string>();
        if (path.find('/', path.length() - 1) != string::npos) {
            path = path.substr(0, path.length() - 1);
        } else {
            path += '/';
        }
        string p = path + "|" + path + "/.*";
        path_regex = Regex(p, regex::ECMAScript | regex::optimize);
    }
    if (settings["rules"].IsDefined()) {
        rules = settings["rules"].as<vector<Regex>>();
    }
}

void FileSettings::load(YAML::Node &settings) {
    allowedDirs.clear();
    HandlerSettings::load(settings);
	root = settings["root"].as<string>();
	LOG_DEBUG("root: " << root);
	root = to_absolute_path(root);
	LOG_DEBUG("root abspath: " << root);
    if (settings["cacheTime"].IsDefined()) {
        cacheTime = settings["cacheTime"].as<float>() * 1000;
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
		if (p.is_relative()) {
			p = directory / p;
		}
        p = p.make_preferred();
        uv_fs_t req;
        int res = uv_fs_realpath(uv_default_loop(), &req, p.string().c_str(), nullptr);
        if (res == UV_ENOSYS) {
            LOG_ERROR("Unsupported operating system");
            return;
        }
        if (req.ptr) {
            item = (char*)req.ptr;
        } else {
            LOG_WARN("The path \"" << item << "\" does not exist");
        }

        if (item.find(p.preferred_separator, item.length() - 1) == string::npos) {
			item += p.preferred_separator;
        }
		int n = 1;
		if (p.preferred_separator == '\\') {
			n = 2;
			// Replace all backslashes with a double backslash to prevent regex from catching it
			size_t pos = 0;
			while ((pos = item.find("\\", pos)) != string::npos) {
				item.replace(pos, 1, "\\\\");
				pos += 2;
			}
		}
        item = item.substr(0, item.length() - n) + "|" + item + ".*";
        LOG_DEBUG("Allowed Dir: " << item);
        allowedDirs.push_back(Regex(item, regex::ECMAScript | regex::optimize));
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
	conf->setDirectory(directory);
    conf->load(settings);
    handlers.push_back(conf);
}


void CapsuleSettings::load(YAML::Node &settings) {
    handlers.clear();
    if (settings["host"].IsDefined()) {
        host = settings["host"].as<Regex>();
    } else {
        host = Regex(".*", regex::ECMAScript | regex::optimize);
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
		cert = to_absolute_path(cert);
    } else {
        cert = "";
    }
    if (settings["key"].IsDefined()) {
        key = settings["key"].as<string>();
		key = to_absolute_path(key);
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
		cert = to_absolute_path(cert);
    } catch (std::exception &err) {
        cerr << "Error: cert is a required option!" << endl;
        throw err;
    }
    try {
        key = settings["key"].as<string>();
		key = to_absolute_path(key);
    } catch (std::exception &err) {
        cerr << "Error: key is a required option!" << endl;
        throw err;
    }
    if (settings["capsules"].IsDefined()) {
        capsules = settings["capsules"].as<string>();
		capsules = to_absolute_path(capsules);
    } else {
        capsules = directory.string();
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
    if (settings["cacheSize"].IsDefined()) {
        cacheSize = settings["cacheSize"].as<float>() * 1000;
    } else {
        cacheSize = 50000;
    }
    if (settings["timeout"].IsDefined()) {
        timeout = settings["timeout"].as<float>() * 1000;
    } else {
        timeout = 5000;
    }
}