#include "gemcaps/executor.hpp"

#include "gemcaps/log.hpp"
#include "gemcaps/pathutils.hpp"
#include "gemcaps/settings.hpp"

#include <sstream>


using std::string;
using std::vector;
using std::ostringstream;

ReusableAllocator<uv_process_t> process_allocator;

// This is a map of extensions -> program executables that will launch the process
phmap::flat_hash_map<string, vector<string>> programs;

size_t strfind(const char *text, char find) noexcept {
    for (size_t i = 0; text[i] != '\0'; ++i) {
        if (text[i] == find) {
            return i;
        }
    }
    return string::npos;
}


void Executor::__on_exit(uv_process_t *process, int64_t exit_status, int term_signal) noexcept {
    if (process->data == nullptr) {
        process_allocator.deallocate(process);
        return;
    }
    Executor *exc = static_cast<Executor *>(process->data);
    if (exc == nullptr) {
        LOG_ERROR("data is not executor as expected");
        return;
    }

    exc->alive = false;

    if (exc->context != nullptr) {
        exc->context->onExit(exc, exit_status, term_signal);
    }
}

Executor::Executor(string path, const phmap::flat_hash_map<string, string> &env, vector<string> args)
        : path(path) {

    cwd = path::dirname(path);

    // Create the environment array
    ostringstream env_oss;
    for (auto var : env) {
        env_oss << var.first << '=' << var.second << '\r';
    }
    string env_str = env_oss.str();
    env_buf = new char[env_str.length()];
    strcpy(env_buf, env_str.c_str());
    this->env = new char*[env.size() + 1];

    size_t pos = 0;
    char *buf = env_buf;
    int i;
    for (i = 0; i < env.size(); ++i) {
        this->env[i] = buf;
        pos = strfind(buf, '\r');
        if (pos == string::npos) {
            break;
        }
        buf[pos++] = '\0';
        buf = buf + pos;
    }
    this->env[i] = nullptr;

    // Create the args array

    // find the executor program
    args.insert(args.begin(), path);
    pos = path.rfind('.');
    if (pos != string::npos) {
        string ext = path.substr(pos + 1);
        auto found = programs.find(ext);
        if (found != programs.end()) {
            // Insert the executor program to the front of the args
            args.insert(args.begin(), found->second.begin(), found->second.end());
        }
    }

    ostringstream args_oss;
    for (auto arg : args) {
        args_oss << arg << '\r';
    }
    string args_str = args_oss.str();
    args_buf = new char[args_str.length()];
    strcpy(args_buf, args_str.c_str());
    this->args = new char*[args.size() + 1];

    pos = 0;
    buf = args_buf;
    for (i = 0; i < args.size(); ++i) {
        this->args[i] = buf;
        pos = strfind(buf, '\r');
        if (pos == string::npos) {
            break;
        }
        buf[pos++] = '\0';
        buf = buf + pos;
    }
    this->args[i] = nullptr;
}

Executor::~Executor() {
    if (env_buf != nullptr) {
        delete[] env_buf;
    }
    if (args_buf != nullptr) {
        delete[] args_buf;
    }
    if (env != nullptr) {
        delete[] env;
    }
    if (args != nullptr) {
        delete[] args;
    }

    if (alive) {
        process->data = nullptr;
        signal(SIGKILL);
    } else if (process != nullptr) {
        process_allocator.deallocate(process);
    }
}

int Executor::spawn(uv_loop_t *loop, int input_fd, int output_fd) noexcept {
    if (process == nullptr) {
        process = process_allocator.allocate();
    } else if (alive) {
        signal(SIGSTOP);
        alive = false;
    }

    uv_stdio_container_t stdio[3];
    if (input_fd >= 0) {
        stdio[0].flags = UV_INHERIT_FD;
        stdio[0].data.fd = input_fd;
    } else {
        stdio[0].flags = UV_IGNORE;
    }
    if (output_fd >= 0) {
        stdio[1].flags = UV_INHERIT_FD;
        stdio[1].data.fd = output_fd;
    } else {
        stdio[1].flags = UV_INHERIT_FD;
        stdio[1].data.fd = 1;
    }
    stdio[2].flags = UV_INHERIT_FD;
    stdio[2].data.fd = 2;

    uv_process_options_t options;
    options.file = args[0];
    options.args = args;
    options.env = env;
    options.cwd = cwd.c_str();
    options.flags = UV_PROCESS_WINDOWS_HIDE_CONSOLE | UV_PROCESS_WINDOWS_HIDE_GUI;
    options.exit_cb = __on_exit;
    options.stdio_count = 3;
    options.stdio = stdio;

    process->data = this;
    int success = uv_spawn(loop, process, &options);
    if (success == 0) {
        alive = true;
    }
    return success;
}

void Executor::signal(int signal) noexcept {
    if (alive) {
        uv_process_kill(process, signal);
    }
}

void Executor::load(YAML::Node settings) {
    programs.clear();
    if (!settings.IsMap()) {
        throw InvalidSettingsException(settings.Mark(), "Must be a map");
    }
    try {
        for (auto it : settings) {
            vector<string> program;
            if (it.second.IsSequence()) {
                program = it.second.as<vector<string>>();
            } else {
                program.push_back(it.second.as<string>());
            }
            if (program.empty()) {
                throw InvalidSettingsException(it.second.Mark(), "Must have at least one element in the sequence");
            }
            string exe = program.front();
            string ext = it.first.as<string>();
            if (exe.find('/') == string::npos && exe.find('\\') == string::npos) {
                program[0] = findPath(exe);
            }
            LOG_DEBUG("Adding program for '" << ext << "' files: '" << program.front() << "'");
            programs.insert({it.first.as<string>(), program});
        } 
    } catch (YAML::RepresentationException e) {
        throw InvalidSettingsException(e.mark, e.msg);
    }
}

string Executor::findPath(string filename) noexcept {
    static vector<string> paths;
    if (paths.empty()) {
        string path = getPath();
#ifdef WIN32
        constexpr const char SEP = ';';
#else
        constexpr const char SEP = ':';
#endif
        while (true) {
            size_t pos = path.find(SEP);
            if (pos == string::npos) {
                paths.push_back(path);
                break;
            }
            paths.push_back(path.substr(0, pos));
            path = path.substr(pos + 1);
        }
    }
    uv_fs_t req;
    for (string p : paths) {
        int result = uv_fs_scandir(uv_default_loop(), &req, p.c_str(), 0, nullptr);
        if (result < 0) {
            continue;
        }
        uv_dirent_t entry;
        while (uv_fs_scandir_next(&req, &entry) != UV_EOF) {
            if (entry.type != UV_DIRENT_FILE) {
                continue;
            }
            if (filename == entry.name
                    #ifdef WIN32
                    || filename + ".exe" == entry.name
                    #endif
            ) {
                return path::join(p, entry.name);
            }
        }
    }
    return filename;
}

const string &Executor::getPath() noexcept {
    static string path;
    if (path.empty()) {
        char path_buf[2048];
        size_t len = sizeof(path_buf);
        uv_os_getenv("PATH", path_buf, &len);
        path = path_buf;
    }
    return path;
}