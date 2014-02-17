#include <stdio.h>
#include <stdlib.h>
#include "chitcp/debug_api.h"
#include "chitcp/tester.h"

enum chitcpd_debug_response print_state_updates(int sockfd, enum chitcpd_debug_event event_flag, debug_socket_state_t *state_info, debug_socket_state_t *saved_state_info, int new_sockfd)
{
    /* If this is a passive socket, we also monitor any active sockets it spawns */
    if (event_flag == DBG_EVT_PENDING_CONNECTION)
    {
        return DBG_RESP_ACCEPT_MONITOR;
    }

    if (event_flag == DBG_EVT_TCP_STATE_CHANGE)
    {
        printf("Socket %i: ", sockfd);
        printf("[SND.UNA = %5i  SND.NXT = %5i  RCV.NXT = %5i]",
                state_info->SND_UNA, state_info->SND_NXT, state_info->RCV_NXT);


        if (saved_state_info)
            printf(" %12s -> %12s\n",
                    tcp_str(saved_state_info->tcp_state),
                    tcp_str(state_info->tcp_state));
        else
            printf("              -> %12s\n", tcp_str(state_info->tcp_state));

        chitcpd_debug_save_socket_state(state_info);
    }

    return DBG_RESP_NONE;
}

int main(int argc, char *argv[])
{
    chitcp_tester_t t;

    EXIT_ON_ERROR(chitcp_tester_init(&t),
            "Could not initialize tester");

    EXIT_ON_ERROR(chitcp_tester_server_set_debug(&t, print_state_updates,
            DBG_EVT_PENDING_CONNECTION| DBG_EVT_TCP_STATE_CHANGE),
            "Could not set debug handler for client");

    EXIT_ON_ERROR(chitcp_tester_client_set_debug(&t, print_state_updates,
            DBG_EVT_TCP_STATE_CHANGE),
            "Could not set debug handler for client");

    EXIT_ON_ERROR(chitcp_tester_start(&t),
            "Could not start tester");

    EXIT_ON_ERROR(chitcp_tester_server_listen(&t),
            "Tester server won't listen()");

    EXIT_ON_ERROR(chitcp_tester_server_accept(&t),
            "Tester server won't accept()");

    EXIT_ON_ERROR(chitcp_tester_client_connect(&t),
            "Tester client won't connect()");

    chitcp_tester_client_close(&t);
    chitcp_tester_server_close(&t);

    chitcp_tester_client_wait_for_state(&t, CLOSED);
    chitcp_tester_server_wait_for_state(&t, CLOSED);
}
