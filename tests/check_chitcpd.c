/*
 * check_chitcpd.c
 *
 *  Created on: Jan 11, 2014
 *      Author: borja
 */

#include <stdlib.h>
#include <check.h>
#include "chitcp/chitcpd.h"
#include "serverinfo.h"
#include "server.h"

START_TEST (test_chitcpd_startstop)
{
    int rc;
    serverinfo_t *si;

    si = calloc(1, sizeof(serverinfo_t));
    si->server_port = chitcp_htons(GET_CHITCPD_PORT);
    si->server_socket_path = GET_CHITCPD_SOCK;

    rc = chitcpd_server_init(si);
    ck_assert_msg(rc == 0, "Could not initialize chiTCP daemon.");

    rc = chitcpd_server_start(si);
    ck_assert_msg(rc == 0, "Could not start chiTCP daemon.");

    rc = chitcpd_server_stop(si);
    ck_assert_msg(rc == 0, "Could not stop chiTCP daemon.");

    rc = chitcpd_server_wait(si);
    ck_assert_msg(rc == 0, "Waiting for chiTCP daemon failed.");

    chitcpd_server_free(si);
}
END_TEST

int main (void)
{
    SRunner *sr;
    int number_failed;
    Suite *s;

    s = suite_create ("chiTCP daemon");

    /* Core test case */
    TCase *tc_startstop= tcase_create ("Start and stop");
    tcase_add_test (tc_startstop, test_chitcpd_startstop);
    suite_add_tcase (s, tc_startstop);

    sr = srunner_create (s);

    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
