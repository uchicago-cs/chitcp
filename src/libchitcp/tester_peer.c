#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "daemon_api.h"
#include "tester_peer.h"
#include "chitcp/types.h"
#include "chitcp/addr.h"
#include "chitcp/log.h"
#include "chitcp/debug_api.h"
#include "chitcp/socket.h"
#include "chitcp/tester.h"


void chitcp_tester_peer_init(chitcp_tester_t *tester, chitcp_tester_peer_t *peer)
{
    peer->sockfd = chisocket_socket(PF_INET,       // Family: IPv4
                                    SOCK_STREAM,   // Type: Full-duplex stream (reliable)
                                    IPPROTO_TCP);  // Protocol: TCP;

    printf("The socket is %i\n", peer->sockfd);

    if(peer->sockfd == -1)
    {
        perror("Could not open socket");
        exit(-1);
    }

    if(peer->debug_handler_func)
    {
        /* Register the debug event handler */
        if (chitcpd_debug(peer->sockfd,
                          peer->debug_event_flags,
                          peer->debug_handler_func)
                != CHITCP_OK)
        {
            perror("Couldn't open a debug connection");
            chisocket_close(peer->sockfd);
            exit(-1);
        }
    }

}

void chitcp_tester_peer_listen(chitcp_tester_t *tester, chitcp_tester_peer_t *peer)
{
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(7); // The echo protocol
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(chisocket_bind(peer->sockfd, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) == -1)
    {
        perror("Socket bind() failed");
        chisocket_close(peer->sockfd);
        exit(-1);
    }


    if(chisocket_listen(peer->sockfd, 5) == -1)
    {
        perror("Socket listen() failed");
        chisocket_close(peer->sockfd);
        exit(-1);
    }

}

void chitcp_tester_peer_connect(chitcp_tester_t *tester, chitcp_tester_peer_t *peer)
{
    struct sockaddr_in serverAddr;

    if(chitcp_addr_construct("localhost", "7", &serverAddr))
    {
        perror("Could not construct address");
        exit(-1);
    }

    if (chisocket_connect(peer->sockfd, (struct sockaddr *) &serverAddr, sizeof(struct sockaddr_in)) == -1)
    {
        chisocket_close(peer->sockfd);
        perror("Could not connect to socket");
        exit(-1);
    }
}


void* chitcp_tester_peer_thread_func(void *args)
{
    test_peer_thread_args_t *tpta;
    chitcp_tester_t *tester;
    chitcp_tester_peer_t *peer;
    bool_t done = FALSE;

    tpta = (test_peer_thread_args_t*) args;
    tester = tpta->tester;
    peer = tpta->peer;

    int clientSocket = -1;

    printf("Started peer thread\n");

    while(!done)
    {
        /* Wait for event */
        pthread_mutex_lock(&peer->lock_event);
        while(peer->event == TEST_EVENT_NONE)
            pthread_cond_wait(&peer->cv_event, &peer->lock_event);

        switch(peer->event)
        {
        case TEST_EVENT_NONE:
            /* Skip */
            break;
        case TEST_EVENT_INIT:
            chitcp_tester_peer_init(tester, peer);
            break;
        case TEST_EVENT_LISTEN:
            chitcp_tester_peer_listen(tester, peer);
            break;
        case TEST_EVENT_ACCEPT:
            /* Do nothing. Accept is special (see below) */
            break;
        case TEST_EVENT_CONNECT:
            chitcp_tester_peer_connect(tester, peer);
            break;
        case TEST_EVENT_RUN:
            printf("Running peer\n");

            break;
        case TEST_EVENT_CLOSE:
            done = TRUE;
            break;
        }

        if(peer->event == TEST_EVENT_ACCEPT)
        {
            peer->event = TEST_EVENT_NONE;
            pthread_cond_signal(&peer->cv_event);
            pthread_mutex_unlock(&peer->lock_event);

            socklen_t sinSize = sizeof(struct sockaddr_in);
            struct sockaddr_in clientAddr;

            sinSize = sizeof(struct sockaddr_in);
            if( (clientSocket = chisocket_accept(peer->sockfd, (struct sockaddr *) &clientAddr, &sinSize)) == -1)
            {
                perror("Socket accept() failed");
                chisocket_close(peer->sockfd);
                exit(-1);
            }
        }
        else
        {
            peer->event = TEST_EVENT_NONE;
            pthread_cond_signal(&peer->cv_event);
            pthread_mutex_unlock(&peer->lock_event);
        }

    }

    return NULL;
}


int chitcp_tester_peer_event(chitcp_tester_peer_t* peer, test_event_t event)
{
    RET_ON_ERROR(pthread_mutex_lock(&peer->lock_event),
            CHITCP_ESYNC);

    peer->event = event;

    RET_ON_ERROR(pthread_cond_signal(&peer->cv_event),
            CHITCP_ESYNC);

    while(peer->event != TEST_EVENT_NONE)
    {
        RET_ON_ERROR(pthread_cond_wait(&peer->cv_event, &peer->lock_event),
                CHITCP_ESYNC);
    }
    RET_ON_ERROR(pthread_mutex_unlock(&peer->lock_event),
            CHITCP_ESYNC);

    return CHITCP_OK;
}

