#include <string>
#include <map>
#include <memory>

#include <iostream>

#include <uv.h>

#include "server.hpp"
#include "manager.hpp"
#include "settings.hpp"

using std::string;
using std::map;
using std::shared_ptr;
using std::make_shared;

using std::cout;
using std::cerr;
using std::endl;

typedef struct {
    SSLServer ssl;
    uv_tcp_t *client;
};

Manager manager;
map<uv_tcp_t*, SSLServer> servers;

void on_new_connection(uv_stream_t *server, int status) {
    if (status < 0) {
        cerr << "Error [Accept] Error while connecting: " << uv_strerror(status) << endl;
        return;
    }
    uv_tcp_t *client = new uv_tcp_t;
    uv_tcp_init(uv_default_loop(), client);
    if (uv_accept(server, (uv_stream_t *)client) != 0) {
        uv_close((uv_handle_t *)client, delete_handle);
        return;
    }
    
    uv_os_fd_t fd;
    uv_fileno((uv_handle_t *)client, &fd);

    shared_ptr<WOLFSSL> conn = servers[(uv_tcp_t *)server].accept(fd);
    if (conn == nullptr) {
        uv_tcp_close_reset(client, delete_handle);
        return;
    }

    wolfSSL_SetIOReadCtx(conn.get(), client);
    wolfSSL_SetIOWriteCtx(conn.get(), client);

    // TODO: get the request header
    // uv_read_start((uv_stream_t *)client.get())

    // manager.handle(client, conn);
}

void delete_handle(uv_handle_t *handle) {
    delete handle;
}

int CIORecv(WOLFSSL *ssl, char *buf, int size, void *ctx) {

}

int CIOSend(WOLFSSL *ssl, char *buf, int size, void *ctx) {

}

int main(int argc, char *argv[]) {
    // TODO: deal with missing config file
    string config = "conf.yml";
    if (argc > 1) {
        config = argv[1];
    }

    GemCapSettings settings;
    settings.loadFile(config);
    manager.load(settings.getCapsules());

    uv_loop_t *loop = uv_default_loop();

    for (auto conf : manager.getServers()) {
        uv_tcp_t *server = new uv_tcp_t;
        uv_tcp_init(loop, server);

        sockaddr_in addr;
        uv_ip4_addr(conf.host.c_str(), conf.port, &addr);

        uv_tcp_bind(server, (const struct sockaddr *)&addr, 0);
        int r = uv_listen((uv_stream_t *) &server, 5, on_new_connection);
        if (r) {
            cout << "Listen error " << uv_strerror(r) << endl;
            continue;
        }

        if (!servers[server].load(conf.cert, conf.key)) {
           servers.erase(server);
           uv_close((uv_handle_t *)server, delete_handle);
           continue;
        }
        WOLFSSL_CTX *ctx = servers[server].getContext();
        wolfSSL_CTX_SetIORecv(ctx, CIORecv);
        wolfSSL_CTX_SetIOSend(ctx, CIOSend);
    }

    wolfSSL_Init();

    int ret = uv_run(uv_default_loop(), UV_RUN_DEFAULT);

    for (auto it = servers.begin(); it != servers.end(); ++it) {
        uv_close((uv_handle_t *)it->first, delete_handle);
    }
    servers.clear();

    uv_loop_close(uv_default_loop());

    wolfSSL_Cleanup();
    return ret;
}