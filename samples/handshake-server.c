/*
 *  chiTCP - A simple, testable TCP stack
 *
 *  Echo server using chisockets.
 *
 *  Not complete yet: can accept connections, but can't send/receive yet.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "chitcp/socket.h"

/* Include the debug functions! */
#include "chitcp/debug_api.h"
#include "chitcp/types.h"

#define FAIL(msg) printf(msg "\n"); exit(-1)
#define PASS printf("pass\n"); exit(0)

int server_event_handler_func(int sockfd, int event_flag, int new_sockfd)
{
    static int sockfd_to_check = -1;
    static tcp_state_t old_state;
    tcp_state_t new_state;

    if (event_flag == DBG_EVT_PENDING_CONNECTION)
    {
        if (sockfd_to_check == -1)
        {
            sockfd_to_check = new_sockfd;
            old_state = LISTEN;
            printf ("Debug event:\n sockfd: %d\n event_flag: %s\n", sockfd, dbg_evt_str(event_flag));
            return DBG_RESP_ACCEPT_MONITOR; /* also debug the new socket */
        }
        else
        {
            return DBG_RESP_NONE;
        }
    }
    else if (event_flag == DBG_EVT_TCP_STATE_CHANGE)
    {
        debug_socket_state_t *state_info;
        state_info = chitcpd_get_socket_state (sockfd, FALSE);

        if (state_info == NULL)
        {
            printf ("Debug event: chitcpd_get_socket_state failed!\n");
        }
        else
        {
            new_state = state_info->tcp_state;
            free (state_info);
            printf ("Debug event:\n sockfd: %d\n state: %s\n event_flag: %s\n", sockfd, tcp_str(new_state), dbg_evt_str(event_flag));

            switch (old_state)
            {
            case LISTEN:
                if (new_state != SYN_RCVD)
                {
                    FAIL("Bad transition from LISTEN");
                }
                break;
            case SYN_RCVD:
                if (new_state != ESTABLISHED)
                {
                    FAIL("Bad transition from SYN_RCVD");
                }
                else PASS;
                break;
            default:
                FAIL ("How did we get here??");
            }

            old_state = new_state;
        }
    }
    return DBG_RESP_NONE;
}

int main(int argc, char *argv[])
{
    int serverSocket;
    int clientSocket;
    debug_event_handler server_event_handler = &server_event_handler_func;

    struct sockaddr_in serverAddr, clientAddr;
    socklen_t sinSize = sizeof(struct sockaddr_in);
    memset(&serverAddr, 0, sizeof(serverAddr));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(7); // The echo protocol
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    serverSocket = chisocket_socket(PF_INET,       // Family: IPv4
                                    SOCK_STREAM,   // Type: Full-duplex stream (reliable)
                                    IPPROTO_TCP);  // Protocol: TCP;

    if(serverSocket == -1)
    {
        perror("Could not open socket");
        exit(-1);
    }

    printf("The socket is %i\n", serverSocket);

    /* Register the debug event handler */
    if (chitcpd_debug(serverSocket, DBG_EVT_PENDING_CONNECTION
                      | DBG_EVT_TCP_STATE_CHANGE,
                      server_event_handler)
            != CHITCP_OK)
    {
        perror("Couldn't open a debug connection");
        chisocket_close(serverSocket);
        exit(-1);
    }


    if(chisocket_bind(serverSocket, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) == -1)
    {
        perror("Socket bind() failed");
        chisocket_close(serverSocket);
        exit(-1);
    }


    if(chisocket_listen(serverSocket, 5) == -1)
    {
        perror("Socket listen() failed");
        chisocket_close(serverSocket);
        exit(-1);
    }

    printf("Waiting for a connection on port %u ...\n", ntohs(serverAddr.sin_port));


    sinSize = sizeof(struct sockaddr_in);
    if( (clientSocket = chisocket_accept(serverSocket, (struct sockaddr *) &clientAddr, &sinSize)) == -1)
    {
        perror("Socket accept() failed");
        chisocket_close(serverSocket);
        exit(-1);
    }

    printf("Got a connection!\n");
    printf("Press any key to close connection... ");
    getchar();
    printf("closing connection... ");
    //chisocket_close(clientSocket);
    printf("Closed!");

    //chisocket_close(serverSocket);

    return 0;
}

