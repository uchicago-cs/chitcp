/*
 *  chiTCP - A simple, testable TCP stack
 *
 *  Functions for sending and receiving messages to and from chitcpd.
 *
 *  See protobuf-wrapper.h for more details.
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include "chitcp/types.h"
#include "protobuf-wrapper.h"


int chitcpd_send_msg(int sockfd, const ChitcpdMsg *msg)
{
    int nbytes;
    size_t size;
    uint8_t *packed;

    size = chitcpd_msg__get_packed_size(msg);
    packed = malloc(sizeof(size_t) + size); /* message length, then message */
    /* We use malloc rather than stack space because message length
     * can be arbitrarily long.*/
    if (!packed)
    {
        perror("chitcpd_send_msg: malloc failed");
        return -2;
    }
    *((size_t *) packed) = size; /* copy over the length (host byte order) */
    chitcpd_msg__pack(msg, packed + sizeof(size_t));

    /* Send request */
    nbytes = send(sockfd, packed, sizeof(size_t) + size, 0);
    free(packed);
    if (nbytes == -1 && (errno == ECONNRESET || errno == EPIPE))
    {
        /* Peer disconnected */
        close(sockfd);
        return -1;
    }
    else if (nbytes == -1)
    {
        perror("chitcpd_send_msg: Unexpected error in send()");
        return -2;
    }

    return CHITCP_OK;
}

int chitcpd_recv_msg(int sockfd, ChitcpdMsg **msg_p)
{
    int nbytes;
    size_t size;
    uint8_t *packed;

    /* Get the message length */
    nbytes = recv(sockfd, &size, sizeof(size_t), 0);
    if (nbytes == 0)
    {
        /* Peer disconnected */
        errno = ECONNRESET;
        close(sockfd);
        return -1;
    }
    else if (nbytes == -1)
    {
        perror("chitcpd_recv_msg: Unexpected error in recv()");
        return -2;
    }

    packed = malloc(size);
    if (!packed)
    {
        perror("chitcpd_recv_msg: malloc failed");
        return -2;
    }

    /* Get the message itself */
    nbytes = recv(sockfd, packed, size, 0);
    if (nbytes == 0)
    {
        /* Peer disconnected */
        errno = ECONNRESET;
        free(packed);
        close(sockfd);
        return -1;
    }
    else if (nbytes == -1)
    {
        perror("chitcpd_recv_msg: Unexpected error in recv()");
        free(packed);
        return -2;
    }

    *msg_p = chitcpd_msg__unpack(NULL, size, packed);
    free(packed);
    if (!*msg_p)
    {
        errno = EPROTO;
        perror("chitcpd_recv_msg: Error unpacking chitcpd msg");
        return -2;
    }

    return CHITCP_OK;
}

/* Note: if two threads call this function on the same socket, each thread
 * may get the response intended for the other. Therefore be careful not
 * to do that. */
int chitcpd_send_and_recv_msg(int sockfd, const ChitcpdMsg *req, ChitcpdMsg **resp)
{
    int r;

    if ((r = chitcpd_send_msg(sockfd, req)) < 0)
        return r;

    if ((r = chitcpd_recv_msg(sockfd, resp)) < 0)
        return r;

    return CHITCP_OK;
}
