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
phmap::flat_hash_map<string, string> programs;

size_t strfind(const char *text, char find) noexcept {
    for (size_t i = 0; text[i] != '\0'; ++i) {
        if (text[i] == find) {
            return i;
        }
    }
    return string::npos;
}


void Executor::__on_exit(uv_process_t *process, int64_t exit_status, int term_signal) noexcept {
    Executor *exc = static_cast<Executor *>(process->data);
    if (process->data == nullptr) {
        process_allocator.deallocate(process);
    }
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
        buf = buf + pos;
        this->env[i] = buf;
        pos = strfind(buf, '\r');
        if (pos == string::npos) {
            break;
        }
        buf[pos++] = '\0';
    }
    this->env[i] = nullptr;

    // Create the args array

    // find the executor program
    string file = args.front();
    pos = file.rfind('.');
    if (pos != string::npos) {
        string ext = file.substr(pos + 1);
        auto found = programs.find(ext);
        if (found != programs.end()) {
            // Insert the executor program to the front of the args
            args.insert(args.begin(), found->second);
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
        buf = buf + pos;
        this->args[i] = buf;
        pos = strfind(buf, '\r');
        if (pos == string::npos) {
            break;
        }
        buf[pos++] = '\0';
    }
    this->env[i] = nullptr;
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
        signal(SIGSTOP);
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

    uv_process_options_t options;
    options.args = args;
    options.env = env;
    options.cwd = cwd.c_str();
    options.gid = uv_os_getpid();
    options.flags = UV_PROCESS_SETGID | UV_PROCESS_WINDOWS_HIDE_CONSOLE | UV_PROCESS_WINDOWS_HIDE_GUI;
    options.exit_cb = __on_exit;
    // TODO: Figure out input/output io
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
    uv_disable_stdio_inheritance();

    programs.clear();
    if (!settings.IsMap()) {
        throw InvalidSettingsException(settings.Mark(), "Must be a map");
    }
    try {
        for (auto it : settings) {
            programs.insert({it.first.as<string>(), it.second.as<string>()});
        } 
    } catch (YAML::RepresentationException e) {
        throw InvalidSettingsException(e.mark, e.msg);
    }
}