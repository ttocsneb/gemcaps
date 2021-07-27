#ifndef __GEMCAPS_SHARED_EXECUTER__
#define __GEMCAPS_SHARED_EXECUTER__

#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>
#include <parallel_hashmap/phmap.h>
#include <uv.h>

#include "gemcaps/util.hpp"

class Executor;

class ExecutorContext {
public:
    virtual void onExit(Executor *executor, int64_t exit_status, int term_signal) = 0;

};

class Executor {
private:
    uv_process_t *process = nullptr;
    ExecutorContext *context = nullptr;

    std::string path;
    std::string cwd;
    char *env_buf = nullptr;
    char *args_buf = nullptr;

    char **env = nullptr;
    char **args = nullptr;

    bool alive = false;

    static void __on_exit(uv_process_t *process, int64_t exit_status, int term_signal) noexcept;
public:
    /**
     * Create an executor for the given file.
     * 
     * This will be able to execute a file either directly or using a secondary program specified by a configuration file.
     * 
     * @param path file to execute
     * @param env environment variables to give the process
     * @param args arguments to give the process
     */
    Executor(std::string path, const phmap::flat_hash_map<std::string, std::string> &env, std::vector<std::string> args);
    ~Executor();

    void setContext(ExecutorContext *context) noexcept { this->context = context; }

    /**
     * Create the process
     * 
     * @todo add options to redirect input/output to/from the created process
     * 
     * @param loop loop
     * @param input_fd file descriptor to redirect to the spawned process's stdin (fd 0)
     * @param output_fd file descriptor to redirect to the 
     * 
     * @return the result of spawning the process
     */
    int spawn(uv_loop_t *loop, int input_fd = -1, int output_fd = -1) noexcept;
    /**
     * Send a signal to the process
     * 
     * @param signal signal to send
     */
    void signal(int signal) noexcept;

    /**
     * Check if the process is alive
     * 
     * @return whether the process is alive
     */
    bool is_alive() const noexcept { return alive; }

    /**
     * Load the global settings for executors.
     * 
     * This should only need to be run once at startup
     * 
     * @param settings settings node
     */
    static void load(YAML::Node settings);

    /**
     * Find the path of an filename
     * 
     * @warning this is a synchronous function, and should not be called while processing a request
     * 
     * @param filename 
     * 
     * @return full path to the file
     * 
     * If the path could not be found, the filename is returned
     */
    static std::string findPath(std::string filename) noexcept;
};

#endif