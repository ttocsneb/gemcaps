#include <string>
#include <set>
#include <memory>

#include <iostream>

#include <uv.h>

#include "server.hpp"
#include "manager.hpp"
#include "settings.hpp"

using std::string;
using std::set;
using std::shared_ptr;
using std::make_shared;

using std::cout;
using std::cerr;
using std::endl;

Manager manager;
SSLServer ssl;
set<shared_ptr<uv_tcp_t>> servers;

struct uv_tcp_t_deleter {
    void operator ()(uv_tcp_t *p) {
        uv_tcp_close_reset(p, nullptr);
    }
};

void on_new_connection(uv_stream_t *stream, int status) {
    shared_ptr<uv_tcp_t> server = make_shared<uv_tcp_t>((uv_tcp_t *)stream, uv_tcp_t_deleter());
    if (status < 0) {
        cerr << "Error [Accept] Error while connecting: " << uv_strerror(status) << endl;
        return;
    }
    uv_os_fd_t fd;
    uv_fileno((uv_handle_t *)server.get(), &fd);

    shared_ptr<WOLFSSL> client = ssl.accept(fd);
    if (client == nullptr) {
        return;
    }

    manager.handle(server, client);
}

int main(int argc, char *argv[]) {
    // TODO: deal with missing config file
    string config = "conf.yml";
    if (argc > 1) {
        config = argv[1];
    }

    GemCapSettings settings;
    settings.loadFile(config);
    if (!ssl.load(settings.getCert(), settings.getKey())) {
        return 1;
    }
    manager.load(settings.getCapsules());

    uv_loop_t *loop = uv_default_loop();

    for (int port : manager.getPorts()) {
        shared_ptr<uv_tcp_t> server = make_shared<uv_tcp_t>();
        uv_tcp_init(loop, server.get());

        sockaddr_in addr;
        uv_ip4_addr("0.0.0.0", port, &addr);

        uv_tcp_bind(server.get(), (const struct sockaddr *)&addr, 0);
        int r = uv_listen((uv_stream_t *) &server, 5, on_new_connection);
        if (r) {
            cout << "Listen error " << uv_strerror(r) << endl;
            return 2;
        }

        servers.insert(server);
    }

    wolfSSL_Init();

    int ret = uv_run(uv_default_loop(), UV_RUN_DEFAULT);

    servers.clear();

    uv_loop_close(uv_default_loop());

    wolfSSL_Cleanup();
    return ret;
}