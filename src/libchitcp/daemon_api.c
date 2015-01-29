/*
 *  chiTCP - A simple, testable TCP stack
 *
 *  Functions to connect to the local chiTCP daemon from an application.
 *
 *  See daemon_api.h for more details.
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
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "daemon_api.h"
#include "chitcp/chitcpd.h"

static pthread_once_t daemon_socket_key_init = PTHREAD_ONCE_INIT;
static pthread_key_t daemon_socket_key;

static void create_daemon_socket_key()
{
    pthread_key_create(&daemon_socket_key, NULL);
}

int chitcpd_get_socket()
{
    void *daemon_socket_ptr;
    int daemon_socket;
    int rc;
    ChitcpdMsg msg = CHITCPD_MSG__INIT;
    ChitcpdInitArgs ia = CHITCPD_INIT_ARGS__INIT;
    ChitcpdMsg *resp_p;

    pthread_once(&daemon_socket_key_init, create_daemon_socket_key);

    daemon_socket_ptr = pthread_getspecific(daemon_socket_key);

    if (daemon_socket_ptr)
    {
        daemon_socket = *((int*) daemon_socket_ptr);
    }
    else
    {
        daemon_socket_ptr = malloc(sizeof(int));
        daemon_socket = chitcpd_connect();

        /* If there was an error, don't store the socket value.
         * Return the error code immediately. */
        if(daemon_socket < 0)
            return daemon_socket;

        /* Send INIT message */
        msg.code = CHITCPD_MSG_CODE__INIT;
        msg.init_args = &ia;
        msg.init_args->connection_type = CHITCPD_CONNECTION_TYPE__COMMAND_CONNECTION;

        rc = chitcpd_send_command(daemon_socket, &msg, &resp_p);
        if (rc < 0)
            return rc;

        /* Unpack response */
        assert(resp_p->resp != NULL);
        rc = resp_p->resp->ret;
        errno = resp_p->resp->error_code;
        chitcpd_msg__free_unpacked(resp_p, NULL);

        if (rc < 0)
            return rc;

        *((int*)daemon_socket_ptr) = daemon_socket;

        pthread_setspecific(daemon_socket_key, daemon_socket_ptr);
    }

    return daemon_socket;
}

/* See daemon_api.h */
int chitcpd_connect()
{
    int clientSocket;
    int len;

    struct sockaddr_un serverAddr;

    if ((clientSocket = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        return CHITCP_ESOCKET;
    }

    // Create server address
    serverAddr.sun_family = AF_UNIX;
    strcpy(serverAddr.sun_path, GET_CHITCPD_SOCK);
    len = strlen(serverAddr.sun_path) + sizeof(serverAddr.sun_family);

    if (connect(clientSocket, (struct sockaddr *)&serverAddr, len) == -1)
    {
        return CHITCP_ESOCKET;
    }

    return clientSocket;
}

/* See daemon_api.h */
int chitcpd_send_command(int sockfd, const ChitcpdMsg *req, ChitcpdMsg **resp_p)
{
    int r; /* return value */

    r = chitcpd_send_and_recv_msg(sockfd, req, resp_p);
    if (r == -1)
        fprintf(stderr, "Daemon socket disconnected\n");

    return r;
}
