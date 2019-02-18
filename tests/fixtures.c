#include <stdio.h>
#include <stdlib.h>
#include <criterion/criterion.h>
#include "serverinfo.h"
#include "server.h"
#include "chitcp/chitcpd.h"
#include "chitcp/debug_api.h"
#include "chitcp/tester.h"
#include "chitcp/log.h"

serverinfo_t *si;
chitcp_tester_t *tester;

void log_setup(void)
{
    char *loglevel;

    loglevel = getenv("LOG");

    if(loglevel)
    {
        if (!strcmp(loglevel, "CRITICAL"))
            chitcp_setloglevel(CRITICAL);
        else if (!strcmp(loglevel, "ERROR"))
            chitcp_setloglevel(ERROR);
        else if (!strcmp(loglevel, "WARNING"))
            chitcp_setloglevel(WARNING);
        else if (!strcmp(loglevel, "MINIMAL"))
            chitcp_setloglevel(MINIMAL);
        else if (!strcmp(loglevel, "INFO"))
            chitcp_setloglevel(INFO);
        else if (!strcmp(loglevel, "DEBUG"))
            chitcp_setloglevel(DEBUG);
        else if (!strcmp(loglevel, "TRACE"))
            chitcp_setloglevel(TRACE);
        else
            chitcp_setloglevel(CRITICAL);
    }
    else
        chitcp_setloglevel(CRITICAL);
}

void chitcpd_and_tester_setup(void)
{
    int rc;

    log_setup();

    si = calloc(1, sizeof(serverinfo_t));
    si->server_port = chitcp_htons(GET_CHITCPD_PORT);
    chitcp_unix_socket(si->server_socket_path, UNIX_PATH_MAX);

    si->libpcap_file_name = getenv("PCAP");

    rc = chitcpd_server_init(si);
    cr_assert(rc == 0, "Could not initialize chiTCP daemon.");

    rc = chitcpd_server_start(si);
    cr_assert(rc == 0, "Could not start chiTCP daemon.");

    tester = calloc(1, sizeof(chitcp_tester_t));
    rc = chitcp_tester_init(tester);
    cr_assert(rc == 0, "Could not initialize tester.");
}

void chitcpd_set_latency(double latency)
{
    si->latency = latency;
}

void chitcpd_and_tester_teardown(void)
{
    int rc;

    rc = chitcpd_server_stop(si);
    cr_assert(rc == 0, "Could not stop chiTCP daemon.");

    rc = chitcpd_server_wait(si);
    cr_assert(rc == 0, "Waiting for chiTCP daemon failed.");

    chitcpd_server_free(si);
    chitcp_tester_free(tester);
}

void tester_connect(void)
{
    int rc;

    rc = chitcp_tester_start(tester);
    cr_assert(rc == 0, "Could not start tester.");

    rc = chitcp_tester_server_listen(tester);
    cr_assert(rc == 0, "Tester did not listen()");

    rc = chitcp_tester_server_accept(tester);
    cr_assert(rc == 0, "Tester did not accept()");

    rc = chitcp_tester_client_connect(tester);
    cr_assert(rc == 0, "Tester did not connect()");
}

void tester_run(void)
{
    int rc;

    rc = chitcp_tester_server_run(tester);
    cr_assert(rc == 0, "Tester server did not run");

    rc = chitcp_tester_client_run(tester);
    cr_assert(rc == 0, "Tester client did not run");
}

void tester_close(void)
{
    int rc;

    rc = chitcp_tester_client_close(tester);
    cr_assert(rc == 0, "Tester client did not close()");

    rc = chitcp_tester_server_close(tester);
    cr_assert(rc == 0, "Tester server did not close()");
}

void tester_done(void)
{
    int rc;

    rc = chitcp_tester_client_exit(tester);
    cr_assert(rc == 0, "Tester client did not exit");

    rc = chitcp_tester_server_exit(tester);
    cr_assert(rc == 0, "Tester server did not exit");
}
