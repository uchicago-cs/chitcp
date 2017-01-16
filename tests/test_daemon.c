#include <stdlib.h>
#include <criterion/criterion.h>
#include "chitcp/chitcpd.h"
#include "serverinfo.h"
#include "server.h"

Test(daemon, startstop)
{
    int rc;
    serverinfo_t *si;

    si = calloc(1, sizeof(serverinfo_t));
    si->server_port = chitcp_htons(GET_CHITCPD_PORT);
    chitcp_unix_socket(si->server_socket_path, UNIX_PATH_MAX);

    rc = chitcpd_server_init(si);
    cr_assert(rc == 0, "Could not initialize chiTCP daemon.");

    rc = chitcpd_server_start(si);
    cr_assert(rc == 0, "Could not start chiTCP daemon.");

    rc = chitcpd_server_stop(si);
    cr_assert(rc == 0, "Could not stop chiTCP daemon.");

    rc = chitcpd_server_wait(si);
    cr_assert(rc == 0, "Waiting for chiTCP daemon failed.");

    chitcpd_server_free(si);
}
