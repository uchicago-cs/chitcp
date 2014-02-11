#include <stdio.h>
#include <stdlib.h>
#include "chitcp/debug_api.h"
#include "chitcp/tester.h"

#define FAIL(msg) printf(msg "\n"); exit(-1)

enum chitcpd_debug_response client_handler_func(int sockfd, enum chitcpd_debug_event event_flag, debug_socket_state_t *state_info, debug_socket_state_t *saved_state_info, int new_sockfd)
{
    bool_t done = FALSE;

    if (event_flag == DBG_EVT_TCP_STATE_CHANGE)
    {
        tcp_state_t old_state, new_state;
        new_state = state_info->tcp_state;
        if (saved_state_info)
            old_state = saved_state_info->tcp_state;
        else
            old_state = CLOSED;

        printf ("Client debug event:\n sockfd: %d\n state: %s\n event_flag: %s\n", sockfd, tcp_str(new_state), dbg_evt_str(event_flag));

        if (old_state == CLOSED && new_state != SYN_SENT)
        {
            FAIL("Client: Bad transition from LISTEN");
        }
        else if (old_state == SYN_SENT && new_state != ESTABLISHED)
        {
            FAIL("Client: Bad transition from SYN_RCVD");
        }
        else if (old_state == SYN_SENT && new_state == ESTABLISHED)
        {
            printf ("Client socket passes! Final state:\n");
            /* TRUE indicates that the buffer contents should be dumped */
            dump_socket_state (state_info, TRUE);
            done = TRUE;
        }
        else if (old_state != CLOSED && old_state != SYN_SENT)
        {
            FAIL ("Client: encountered unexpected state");
        }
    }

    if (done)
    {
        return DBG_RESP_STOP;
    }
    else
    {
        chitcpd_debug_save_socket_state(state_info);
        return DBG_RESP_NONE;
    }
}


enum chitcpd_debug_response server_handler_func(int sockfd, enum chitcpd_debug_event event_flag, debug_socket_state_t *state_info, debug_socket_state_t *saved_state_info, int new_sockfd)
{
    bool_t done = FALSE;

    if (event_flag == DBG_EVT_PENDING_CONNECTION)
    {
        return DBG_RESP_ACCEPT_MONITOR; /* also debug the new socket */
    }

    if (event_flag == DBG_EVT_TCP_STATE_CHANGE)
    {
        tcp_state_t old_state, new_state;
        new_state = state_info->tcp_state;
        if (saved_state_info)
            old_state = saved_state_info->tcp_state;
        else
            old_state = LISTEN;

        printf ("Server debug event:\n sockfd: %d\n state: %s\n event_flag: %s\n", sockfd, tcp_str(new_state), dbg_evt_str(event_flag));

        if (old_state == LISTEN && new_state != SYN_RCVD)
        {
            FAIL("Server: Bad transition from LISTEN");
        }
        else if (old_state == SYN_RCVD && new_state != ESTABLISHED)
        {
            FAIL("Server: Bad transition from SYN_RCVD");
        }
        else if (old_state == SYN_RCVD && new_state == ESTABLISHED)
        {
            printf ("Server socket passes! Final state:\n");
            /* TRUE indicates that the buffer contents should be dumped */
            dump_socket_state (state_info, TRUE);
            done = TRUE;
        }
        else if (old_state != LISTEN && old_state != SYN_RCVD)
        {
            FAIL ("Server: encountered unexpected state");
        }
    }

    if (done)
    {
        return DBG_RESP_STOP;
    }
    else
    {
        chitcpd_debug_save_socket_state(state_info);
        return DBG_RESP_NONE;
    }
}

int main(int argc, char *argv[])
{
    chitcp_tester_t t;

    chitcp_tester_init(&t);
    chitcp_tester_server_set_debug(&t, server_handler_func,
            DBG_EVT_PENDING_CONNECTION| DBG_EVT_TCP_STATE_CHANGE);
    chitcp_tester_client_set_debug(&t, client_handler_func,
            DBG_EVT_TCP_STATE_CHANGE);

    chitcp_tester_start(&t);

    chitcp_tester_server_listen(&t);
    chitcp_tester_server_accept(&t);
    chitcp_tester_client_connect(&t);

    getchar();
    chitcp_tester_client_close(&t);
    chitcp_tester_server_close(&t);
}
