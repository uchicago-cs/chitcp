/*
 * check_chitcpd.c
 *
 *  Created on: Jan 11, 2014
 *      Author: borja
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

enum chitcpd_debug_response check_states(int sockfd, enum chitcpd_debug_event event_flag, debug_socket_state_t *state_info, debug_socket_state_t *saved_state_info, int new_sockfd)
{
    if (event_flag == DBG_EVT_PENDING_CONNECTION)
    {
        return DBG_RESP_ACCEPT_MONITOR;
    }

    if (event_flag == DBG_EVT_TCP_STATE_CHANGE)
    {
        tcp_state_t curs = state_info->tcp_state;
        ck_assert_msg(IS_VALID_TCP_STATE(curs), "Unknown TCP state.");

        if(!saved_state_info)
        {
            ck_assert_msg(curs == SYN_SENT || curs == SYN_RCVD,
                          "%s is not a valid initial state", tcp_str(state_info->tcp_state));
        }
        else
        {
            ck_assert_msg(IS_VALID_TCP_STATE(saved_state_info->tcp_state), "Unknown (previous) TCP state.");
            tcp_state_t prevs = saved_state_info->tcp_state;

            if ( (prevs == SYN_SENT && curs != ESTABLISHED) ||
                 (prevs == SYN_RCVD && curs != ESTABLISHED)  )
                ck_abort_msg("Invalid transition: %s -> %s", tcp_str(prevs), tcp_str(curs));
        }

        chitcpd_debug_save_socket_state(state_info);

        if (curs == ESTABLISHED)
            return DBG_RESP_STOP;
        else
            return DBG_RESP_NONE;
    }

    return DBG_RESP_NONE;
}


enum chitcpd_debug_response check_vars(int sockfd, enum chitcpd_debug_event event_flag, debug_socket_state_t *state_info, debug_socket_state_t *saved_state_info, int new_sockfd)
{
    if (event_flag == DBG_EVT_PENDING_CONNECTION)
    {
        return DBG_RESP_ACCEPT_MONITOR;
    }

    if (event_flag == DBG_EVT_TCP_STATE_CHANGE)
    {
        tcp_state_t curs = state_info->tcp_state;
        ck_assert_msg(IS_VALID_TCP_STATE(curs), "Unknown TCP state.");

        if(!saved_state_info)
        {
            ck_assert_msg(curs == SYN_SENT || curs == SYN_RCVD,
                          "%s is not a valid initial state", tcp_str(curs));
            ck_assert_msg(state_info->SND_UNA + 1 == state_info->SND_NXT,
                          "In state %s, SND.UNA + 1 != SND.NXT (got SND.UNA=%i, SND.NXT=%i",
                          tcp_str(curs), state_info->SND_UNA, state_info->SND_NXT);
        }
        else
        {
            ck_assert_msg(IS_VALID_TCP_STATE(saved_state_info->tcp_state), "Unknown (previous) TCP state.");
            tcp_state_t prevs = saved_state_info->tcp_state;

            if ( (prevs == SYN_SENT && curs != ESTABLISHED) ||
                 (prevs == SYN_RCVD && curs != ESTABLISHED)  )
                ck_abort_msg("Invalid transition: %s -> %s", tcp_str(prevs), tcp_str(curs));

            if (prevs == SYN_SENT || prevs == SYN_RCVD)
                ck_assert_msg(state_info->SND_UNA  == state_info->SND_NXT,
                              "In state %s, SND.UNA != SND.NXT (got SND.UNA=%i, SND.NXT=%i",
                              tcp_str(curs), state_info->SND_UNA, state_info->SND_NXT);
        }

        chitcpd_debug_save_socket_state(state_info);


        if (curs == ESTABLISHED)
            return DBG_RESP_STOP;
        else
            return DBG_RESP_NONE;
    }

    return DBG_RESP_NONE;
}

START_TEST (test_3way_states)
{
    int rc;

    rc = chitcp_tester_server_set_debug(tester, check_states,
            DBG_EVT_PENDING_CONNECTION| DBG_EVT_TCP_STATE_CHANGE);

    rc = chitcp_tester_client_set_debug(tester, check_states,
            DBG_EVT_TCP_STATE_CHANGE);

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_done();
}
END_TEST

START_TEST (test_3way_vars)
{
    int rc;

    rc = chitcp_tester_server_set_debug(tester, check_vars,
            DBG_EVT_PENDING_CONNECTION| DBG_EVT_TCP_STATE_CHANGE);
    ck_assert_msg(rc == 0, "Error setting debug handler (server)");

    rc = chitcp_tester_client_set_debug(tester, check_vars,
            DBG_EVT_TCP_STATE_CHANGE);
    ck_assert_msg(rc == 0, "Error setting debug handler (client)");

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_done();
}
END_TEST

Suite* make_connection_init_suite (void)
{
  Suite *s = suite_create ("TCP: Connection Establishment");

  /* Core test case */
  TCase *tc_3way = tcase_create ("3-way handshake");
  tcase_add_checked_fixture (tc_3way, chitcpd_and_tester_setup, chitcpd_and_tester_teardown);
  tcase_add_test (tc_3way, test_3way_states);
  tcase_add_test (tc_3way, test_3way_vars);
  suite_add_tcase (s, tc_3way);

  return s;
}
