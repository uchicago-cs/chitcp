/*
 * check_chitcpd.c
 *
 *  Created on: Jan 11, 2014
 *      Author: borja
 */

#include <stdio.h>
#include <stdlib.h>
#include <criterion/criterion.h>
#include "serverinfo.h"
#include "server.h"
#include "chitcp/chitcpd.h"
#include "chitcp/debug_api.h"
#include "chitcp/tester.h"
#include "fixtures.h"

static enum chitcpd_debug_response check_states(int sockfd, enum chitcpd_debug_event event_flag, debug_socket_state_t *state_info, debug_socket_state_t *saved_state_info, int new_sockfd)
{
    if (event_flag == DBG_EVT_PENDING_CONNECTION)
    {
        return DBG_RESP_ACCEPT_MONITOR;
    }

    if (event_flag == DBG_EVT_TCP_STATE_CHANGE)
    {
        tcp_state_t curs = state_info->tcp_state;

        cr_assert(IS_VALID_TCP_STATE(curs), "Unknown TCP state.");

        if(!saved_state_info)
        {
            cr_assert(curs == SYN_SENT || curs == SYN_RCVD,
                          "%s is not a valid initial state", tcp_str(state_info->tcp_state));
        }
        else
        {
            cr_assert(IS_VALID_TCP_STATE(saved_state_info->tcp_state), "Unknown (previous) TCP state.");
            tcp_state_t prevs = saved_state_info->tcp_state;

            if ( (prevs == SYN_SENT && curs != ESTABLISHED) ||
                 (prevs == SYN_RCVD && curs != ESTABLISHED)  )
                cr_assert_fail("Invalid transition: %s -> %s", tcp_str(prevs), tcp_str(curs));
        }

        chitcpd_debug_save_socket_state(state_info);

        if (curs == ESTABLISHED)
            return DBG_RESP_STOP;
        else
            return DBG_RESP_NONE;
    }

    return DBG_RESP_NONE;
}


static enum chitcpd_debug_response check_vars(int sockfd, enum chitcpd_debug_event event_flag, debug_socket_state_t *state_info, debug_socket_state_t *saved_state_info, int new_sockfd)
{
    if (event_flag == DBG_EVT_PENDING_CONNECTION)
    {
        return DBG_RESP_ACCEPT_MONITOR;
    }

    if (event_flag == DBG_EVT_TCP_STATE_CHANGE)
    {
        tcp_state_t curs = state_info->tcp_state;
        cr_assert(IS_VALID_TCP_STATE(curs), "Unknown TCP state.");

        if(!saved_state_info)
        {
            cr_assert(curs == SYN_SENT || curs == SYN_RCVD,
                          "%s is not a valid initial state", tcp_str(curs));
            cr_assert(state_info->SND_UNA + 1 == state_info->SND_NXT,
                          "In state %s, SND.UNA + 1 != SND.NXT (got SND.UNA=%i, SND.NXT=%i)",
                          tcp_str(curs), state_info->SND_UNA, state_info->SND_NXT);
        }
        else
        {
            cr_assert(IS_VALID_TCP_STATE(saved_state_info->tcp_state), "Unknown (previous) TCP state.");
            tcp_state_t prevs = saved_state_info->tcp_state;

            if ( (prevs == SYN_SENT && curs != ESTABLISHED) ||
                 (prevs == SYN_RCVD && curs != ESTABLISHED)  )
                cr_assert_fail("Invalid transition: %s -> %s", tcp_str(prevs), tcp_str(curs));

            if (prevs == SYN_SENT)
                cr_assert(state_info->SND_UNA == state_info->SND_NXT,
                              "In state %s, SND.UNA != SND.NXT (got SND.UNA=%i, SND.NXT=%i)",
                              tcp_str(curs), state_info->SND_UNA, state_info->SND_NXT);

            if (prevs == SYN_RCVD)
                cr_assert(state_info->SND_UNA == state_info->SND_NXT-1,
                          "In state %s, SND.UNA != SND.NXT-1 (got SND.UNA=%i, SND.NXT=%i). "
                          "Careful: in the transition from SYN_RCVD to ESTABLISHED, the value of "
                          "SND.UNA is updated *after* the transition to ESTABLISHED. The tests "
                          "check for the values at the moment the transition happens.",
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

Test(conn_init, 3way_states, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 0.5)
{
    int rc;

    rc = chitcp_tester_server_set_debug(tester, check_states,
            DBG_EVT_PENDING_CONNECTION| DBG_EVT_TCP_STATE_CHANGE);
    cr_assert(rc == 0, "Error setting debug handler (server)");

    rc = chitcp_tester_client_set_debug(tester, check_states,
            DBG_EVT_TCP_STATE_CHANGE);
    cr_assert(rc == 0, "Error setting debug handler (client)");

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_done();
}


Test(conn_init, 3way_vars, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 0.5)
{
    int rc;

    rc = chitcp_tester_server_set_debug(tester, check_vars,
            DBG_EVT_PENDING_CONNECTION| DBG_EVT_TCP_STATE_CHANGE);
    cr_assert(rc == 0, "Error setting debug handler (server)");

    rc = chitcp_tester_client_set_debug(tester, check_vars,
            DBG_EVT_TCP_STATE_CHANGE);
    cr_assert(rc == 0, "Error setting debug handler (client)");

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_done();
}


