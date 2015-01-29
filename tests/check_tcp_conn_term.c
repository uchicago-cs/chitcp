/*
 * check_tcp_conn_term.c
 *
 *  Created on: December 15, 2014
 *      Author: Tristan Rasmussen
 */

#include <stdio.h>
#include <stdlib.h>
#include <check.h>

#include "serverinfo.h"
#include "server.h"
#include "chitcp/chitcpd.h"
#include "chitcp/debug_api.h"
#include "chitcp/tester.h"
#include "fixtures.h"

START_TEST (client_term)
{
    tester_connect();

    chitcp_tester_client_close(tester);
    chitcp_tester_client_wait_for_state(tester, FIN_WAIT_2);
    chitcp_tester_server_close(tester);

    chitcp_tester_server_wait_for_state(tester, CLOSED);
    chitcp_tester_client_wait_for_state(tester, CLOSED);

    tester_done();
}
END_TEST

START_TEST (server_term)
{
    tester_connect();

    chitcp_tester_server_close(tester);
    chitcp_tester_server_wait_for_state(tester, FIN_WAIT_2);
    chitcp_tester_client_close(tester);

    chitcp_tester_server_wait_for_state(tester, CLOSED);
    chitcp_tester_client_wait_for_state(tester, CLOSED);

    tester_done();
}
END_TEST

START_TEST (simultaneous_term)
{
    tester_connect();

    chitcp_tester_server_close(tester);
    chitcp_tester_client_close(tester);

    chitcp_tester_server_wait_for_state(tester, CLOSED);
    chitcp_tester_client_wait_for_state(tester, CLOSED);

    tester_done();
}
END_TEST

Suite* make_connection_term_suite (void)
{
    Suite *s = suite_create ("TCP: Connection Termination");

    /* Core test case */
    TCase *tc_term = tcase_create ("Connection termination");
    tcase_add_checked_fixture (tc_term, chitcpd_and_tester_setup, chitcpd_and_tester_teardown);
    tcase_add_test (tc_term, client_term);
    tcase_add_test (tc_term, server_term);
    tcase_add_test (tc_term, simultaneous_term);
    suite_add_tcase (s, tc_term);

    return s;
}
