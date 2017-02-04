/*
 *  chiTCP - A simple, testable TCP stack
 *
 *  See connection.c
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


#ifndef CONNECTION_H_
#define CONNECTION_H_

#include "serverinfo.h"
#include "chitcp/packet.h"

typedef struct connection_thread_args
{
    serverinfo_t *si;
    tcpconnentry_t *connection;
    char thread_name[16];
} connection_thread_args_t;

void* chitcpd_connection_thread_func(void *args);


typedef struct packet_delivery_thread_args
{
    serverinfo_t *si;
} packet_delivery_thread_args_t;

void* chitcpd_packet_delivery_thread_func(void *args);


tcpconnentry_t* chitcpd_get_connection(serverinfo_t *si, struct sockaddr* addr);
tcpconnentry_t* chitcpd_create_connection(serverinfo_t *si, struct sockaddr* addr);
tcpconnentry_t* chitcpd_add_connection(serverinfo_t *si, socket_t realsocket_send, socket_t realsocket_recv, struct sockaddr* addr);
int chitcpd_create_connection_thread(serverinfo_t *si, tcpconnentry_t* connection);

int chitcpd_send_tcp_packet(serverinfo_t *si, chisocketentry_t *sock, tcp_packet_t* tcp_packet);
int chitcpd_recv_tcp_packet(serverinfo_t *si, tcp_packet_t* tcp_packet, struct sockaddr *local_realaddr, struct sockaddr *peer_realaddr);

#endif /* CONNECTION_H_ */
