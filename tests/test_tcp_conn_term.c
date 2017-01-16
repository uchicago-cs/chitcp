#include <stdio.h>
#include <stdlib.h>
#include <criterion/criterion.h>
#include "serverinfo.h"
#include "server.h"
#include "chitcp/chitcpd.h"
#include "chitcp/debug_api.h"
#include "chitcp/tester.h"
#include "fixtures.h"

static enum chitcpd_debug_response active_close_checker(int sockfd, enum chitcpd_debug_event event_flag, debug_socket_state_t *state_info, debug_socket_state_t *saved_state_info, int new_sockfd)
{
    if (event_flag == DBG_EVT_PENDING_CONNECTION)
    {
        return DBG_RESP_ACCEPT_MONITOR;
    }

    if (event_flag == DBG_EVT_TCP_STATE_CHANGE)
    {
        tcp_state_t curs = state_info->tcp_state;
        cr_assert(IS_VALID_TCP_STATE(curs), "Unknown TCP state.");

        if(saved_state_info)
        {
            cr_assert(IS_VALID_TCP_STATE(saved_state_info->tcp_state), "Unknown (previous) TCP state.");
            tcp_state_t prevs = saved_state_info->tcp_state;

            if ( (prevs == ESTABLISHED && curs != FIN_WAIT_1) ||
                 (prevs == FIN_WAIT_1 && curs != FIN_WAIT_2)  ||
                 (prevs == FIN_WAIT_2 && curs != TIME_WAIT)   ||
                 (prevs == TIME_WAIT && curs != CLOSED)
            )
                cr_assert_fail("Invalid transition: %s -> %s", tcp_str(prevs), tcp_str(curs));
        }

        chitcpd_debug_save_socket_state(state_info);

        if (curs == CLOSED)
            return DBG_RESP_STOP;
        else
            return DBG_RESP_NONE;
    }

    return DBG_RESP_NONE;
}


static enum chitcpd_debug_response passive_close_checker(int sockfd, enum chitcpd_debug_event event_flag, debug_socket_state_t *state_info, debug_socket_state_t *saved_state_info, int new_sockfd)
{
    if (event_flag == DBG_EVT_PENDING_CONNECTION)
    {
        return DBG_RESP_ACCEPT_MONITOR;
    }

    if (event_flag == DBG_EVT_TCP_STATE_CHANGE)
    {
        tcp_state_t curs = state_info->tcp_state;
        cr_assert(IS_VALID_TCP_STATE(curs), "Unknown TCP state.");

        if(saved_state_info)
        {
            cr_assert(IS_VALID_TCP_STATE(saved_state_info->tcp_state), "Unknown (previous) TCP state.");
            tcp_state_t prevs = saved_state_info->tcp_state;

            if ( (prevs == ESTABLISHED && curs != CLOSE_WAIT) ||
                 (prevs == CLOSE_WAIT && curs != LAST_ACK)  ||
                 (prevs == LAST_ACK && curs != CLOSED)
            )
                cr_assert_fail("Invalid transition: %s -> %s", tcp_str(prevs), tcp_str(curs));
        }

        chitcpd_debug_save_socket_state(state_info);

        if (curs == CLOSED)
            return DBG_RESP_STOP;
        else
            return DBG_RESP_NONE;
    }

    return DBG_RESP_NONE;
}


static enum chitcpd_debug_response simultaneous_close_checker(int sockfd, enum chitcpd_debug_event event_flag, debug_socket_state_t *state_info, debug_socket_state_t *saved_state_info, int new_sockfd)
{
    if (event_flag == DBG_EVT_PENDING_CONNECTION)
    {
        return DBG_RESP_ACCEPT_MONITOR;
    }

    /* Ensure that close is truly simultaneous. This prevents
     * a race condition where one peer send its FIN message
     * before the other peer has transitioned to FIN_WAIT_1.
     * The following code prevents the FIN packet from being
     * delivered if the socket is still in an ESTABLISHED state.
     */
    if (event_flag == DBG_EVT_INCOMING_PACKET)
    {
        tcp_state_t curs = state_info->tcp_state;

        if (curs == ESTABLISHED)
        {
            return DBG_RESP_WITHHOLD;
        }
    }

    if (event_flag == DBG_EVT_TCP_STATE_CHANGE)
    {
        tcp_state_t curs = state_info->tcp_state;
        cr_assert(IS_VALID_TCP_STATE(curs), "Unknown TCP state.");

        if(saved_state_info)
        {
            cr_assert(IS_VALID_TCP_STATE(saved_state_info->tcp_state), "Unknown (previous) TCP state.");
            tcp_state_t prevs = saved_state_info->tcp_state;

            if ( (prevs == ESTABLISHED && curs != FIN_WAIT_1) ||
                 (prevs == FIN_WAIT_1 && curs != CLOSING) ||
                 (prevs == CLOSING && curs != TIME_WAIT)  ||
                 (prevs == TIME_WAIT && curs != CLOSED)
            )
                cr_assert_fail("Invalid transition: %s -> %s", tcp_str(prevs), tcp_str(curs));

            /* If there was a FIN packet waiting to be delivered,
             * release it now. */
            if (prevs == ESTABLISHED && curs == FIN_WAIT_1)
            {
                chitcpd_debug_save_socket_state(state_info);
                return DBG_RESP_DRAW_WITHHELD;
            }
        }

        chitcpd_debug_save_socket_state(state_info);

        if (curs == CLOSED)
            return DBG_RESP_STOP;
        else
            return DBG_RESP_NONE;
    }

    return DBG_RESP_NONE;
}


Test(conn_term, client_closes_first, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 0.5)
{
    int rc;

    rc = chitcp_tester_server_set_debug(tester, passive_close_checker,
                                        DBG_EVT_PENDING_CONNECTION| DBG_EVT_TCP_STATE_CHANGE);
    cr_assert(rc == 0, "Error setting debug handler (server)");

    rc = chitcp_tester_client_set_debug(tester, active_close_checker, DBG_EVT_TCP_STATE_CHANGE);
    cr_assert(rc == 0, "Error setting debug handler (client)");

    tester_connect();

    chitcp_tester_client_close(tester);
    chitcp_tester_client_wait_for_state(tester, FIN_WAIT_2);
    chitcp_tester_server_close(tester);

    chitcp_tester_server_wait_for_state(tester, CLOSED);
    chitcp_tester_client_wait_for_state(tester, CLOSED);

    tester_done();
}

Test(conn_term, server_closes_first, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 0.5)
{
    int rc;

    rc = chitcp_tester_server_set_debug(tester, active_close_checker,
                                        DBG_EVT_PENDING_CONNECTION| DBG_EVT_TCP_STATE_CHANGE);
    cr_assert(rc == 0, "Error setting debug handler (server)");

    rc = chitcp_tester_client_set_debug(tester, passive_close_checker, DBG_EVT_TCP_STATE_CHANGE);
    cr_assert(rc == 0, "Error setting debug handler (client)");

    tester_connect();

    chitcp_tester_server_close(tester);
    chitcp_tester_server_wait_for_state(tester, FIN_WAIT_2);
    chitcp_tester_client_close(tester);
    chitcp_tester_server_wait_for_state(tester, CLOSED);
    chitcp_tester_client_wait_for_state(tester, CLOSED);

    tester_done();
}

Test(conn_term, simultaneous_close, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 0.5)
{
    int rc;

    rc = chitcp_tester_server_set_debug(tester, simultaneous_close_checker,
                                        DBG_EVT_PENDING_CONNECTION| DBG_EVT_TCP_STATE_CHANGE);
    cr_assert(rc == 0, "Error setting debug handler (server)");

    rc = chitcp_tester_client_set_debug(tester, simultaneous_close_checker, DBG_EVT_TCP_STATE_CHANGE);
    cr_assert(rc == 0, "Error setting debug handler (client)");

    tester_connect();

    chitcp_tester_server_close(tester);
    chitcp_tester_client_close(tester);

    chitcp_tester_server_wait_for_state(tester, CLOSED);
    chitcp_tester_client_wait_for_state(tester, CLOSED);

    tester_done();
}

