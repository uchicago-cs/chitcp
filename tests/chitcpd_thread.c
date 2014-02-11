/*
 * chitcpd_thread.c
 *
 *  Created on: Jan 11, 2014
 *      Author: borja
 */

#include <stdlib.h>
#include <stdio.h>

#include "chitcp/chitcpd.h"
#include "chitcp/log.h"
#include "server.h"

typedef struct chitcpd_thread
{
    serverinfo_t *si;
    pthread_t t;
} chitcpd_thread_t;


void* chitcpd_thread_func(void *args)
{
    int rc;
    chitcpd_thread_t *chitcpd;

    chitcpd = (chitcpd_thread_t*) args;



    /* Run the daemon */
    //rc = chitcpd_server_init(si);
    if(rc != 0)
    {
        fprintf(stderr, "Could not start server.\n");
        return rc;
    }

    //chilog(INFO, "chitcpd running. UNIX socket: %s. TCP socket: %i", si->server_socket_path, ntohs(si->server_port));

    /* Wait for daemon to be done */
    //rc = chitcpd_server_wait(si);


    return 0;
}

int chitcpd_thread_init(chitcpd_thread_t *chitcpd)
{
    /* Allocate the serverinfo struct. It contains all of the daemon's state */
    chitcpd->si = calloc(1, sizeof(serverinfo_t));

    /* Set values in serverinfo */
   // chitcpd->si->server_port = chitcp_htons(atoi(port));
   // chitcpd->si->server_socket_path = usocket;

}

int chitcpd_thread_start(chitcpd_thread_t *chitcpd)
{

    if (pthread_create(&chitcpd->t, NULL, chitcpd_thread_func, chitcpd) < 0)
    {
        perror("Could not create chitcpd thread");
        free(chitcpd);
        return CHITCP_ETHREAD;
    }

}

int chitcpd_thread_stop()
{

}
