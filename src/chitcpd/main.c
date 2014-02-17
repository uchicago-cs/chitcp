/*
 *  chiTCP - A simple, testable TCP stack
 *
 *  main() function for chiTCP daemon (chitcpd)
 *
 *  The chitcpd executable accepts three command-line arguments:
 *
 *  -p PORT: The daemon's TCP port (default: 23300)
 *  -s FILE: The UNIX socket where local applications will be able to
 *           send requests to the chiTCP daemon
 *  -v: Be verbose. Can be repeated up to three times for extra verbosity.
 *
 *  The main() function mostly takes care of processing the command-line
 *  arguments. Next, server.c takes care of setting up the various threads
 *  of the daemon.
 *
 */

/*
 *  Copyright (c) 2013-2014, The University of Chicago
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 *  - Neither the name of The University of Chicago nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <arpa/inet.h>

#include "chitcp/chitcpd.h"
#include "chitcp/log.h"
#include "server.h"


int main(int argc, char *argv[])
{
    int rc;
    serverinfo_t *si;
    sigset_t new;
    int opt;
    char *port = NULL;
    char *usocket = NULL;
    int verbosity = 0;

    /* Stop SIGPIPE from messing with our sockets */
    sigemptyset (&new);
    sigaddset(&new, SIGPIPE);
    if (pthread_sigmask(SIG_BLOCK, &new, NULL) != 0)
    {
        perror("Unable to mask SIGPIPE");
        exit(-1);
    }

    /* Process command-line arguments */
    while ((opt = getopt(argc, argv, "p:s:vh")) != -1)
        switch (opt)
        {
        case 'p':
            port = strdup(optarg);
            break;
        case 's':
            usocket = strdup(optarg);
            break;
        case 'v':
            verbosity++;
            break;
        case 'h':
            printf("Usage: chitcpd [-p PORT] [-s UNIX_SOCKET] [(-v|-vv|-vvv)]\n");
            exit(0);
        default:
            printf("ERROR: Unknown option -%c\n", opt);
            exit(-1);
        }

    if(!port)
        port = GET_CHITCPD_PORT_STRING;

    if(!usocket)
        usocket = GET_CHITCPD_SOCK;

    /* Set logging level based on verbosity */
    switch(verbosity)
    {
    case 0:
        chitcp_setloglevel(ERROR);
        break;
    case 1:
        chitcp_setloglevel(INFO);
        break;
    case 2:
        chitcp_setloglevel(DEBUG);
        break;
    case 3:
        chitcp_setloglevel(TRACE);
        break;
    default:
        chitcp_setloglevel(TRACE);
        break;
    }


    /* Allocate the serverinfo struct. It contains all of the daemon's state */
    si = calloc(1, sizeof(serverinfo_t));

    /* Set values in serverinfo */
    si->server_port = chitcp_htons(atoi(port));
    si->server_socket_path = usocket;

    /* Run the daemon */
    rc = chitcpd_server_init(si);
    if(rc != 0)
    {
        fprintf(stderr, "Could not initialize server.\n");
        return rc;
    }

    rc = chitcpd_server_start(si);
    if(rc != 0)
    {
        fprintf(stderr, "Could not start server.\n");
        return rc;
    }

    chilog(INFO, "chitcpd running. UNIX socket: %s. TCP socket: %i", si->server_socket_path, ntohs(si->server_port));

    /* Wait for daemon to be done */
    rc = chitcpd_server_wait(si);
    if(rc != 0)
    {
        fprintf(stderr, "Server stopped unexpectedly.\n");
        return rc;
    }

    chitcpd_server_free(si);
    free(si);

    return CHITCP_OK;
}
