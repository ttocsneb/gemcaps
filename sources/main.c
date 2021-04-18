#include <benstools/string.h>
#include <benstools/logging.h>
#include "server.h"

int main() {
    bt_log_setup();

    wolfSSL_Init();

    WOLFSSL_CTX *ctx = server_bind("1965", ".test/server.crt", ".test/server.key");
    bt_log_cleanup();

    if (ctx == NULL) {
        wolfSSL_Cleanup();
        return 1;
    }
    wolfSSL_CTX_free(ctx);
    wolfSSL_Cleanup();
}