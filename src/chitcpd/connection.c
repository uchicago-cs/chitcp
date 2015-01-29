/*
 *  chiTCP - A simple, testable TCP stack
 *
 *  Functions to handle (real) TCP connections, including
 *  sending/receiving TCP packets.
 *
 *  Here, "connection" refers to a TCP connection between two
 *  chiTCP daemons, *not* to a chiTCP connection (between two
 *  chisockets).
 *
 *  chiTCP is a TCP-over-TCP protocol, where two peers will both
 *  be running a chiTCP daemon (chitcpd). These two daemons exchange
 *  chiTCP packets through a (real) TCP connection. The code
 *  in this file handles these connections, including sending
 *  chiTCP packets to a peer, and handling the reception of
 *  a chiTCP packet through that connection.
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
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "handlers.h"
#include "connection.h"
#include "chitcp/chitcpd.h"
#include "chitcp/addr.h"
#include "chitcp/log.h"
#include "breakpoint.h"

/*
 * chitcpd_connection_thread_func - Connection thread function
 *
 * This thread is spawned from chitcpd_server_network_thread_func
 * (in server.c) each time a peer connects to the chiTCP daemon.
 *
 * args: arguments (connection_thread_args_t)
 *
 * Returns: Nothing.
 *
 */
void* chitcpd_connection_thread_func(void *args)
{
    connection_thread_args_t *cta = (connection_thread_args_t *) args;

    ssize_t nbytes;
    bool_t done = FALSE;
    serverinfo_t *si = cta->si;
    tcpconnentry_t *connection = cta->connection;
    pthread_setname_np(pthread_self(), cta->thread_name);
    free(args);
    struct sockaddr_storage local_addr, peer_addr;
    chitcphdr_t chitcp_header;
    uint8_t buf[1000];
    uint16_t payload_len;
    int ret;
    /* Get the local and peer addresses */
    socklen_t lsize, psize;
    lsize = psize = sizeof(struct sockaddr_storage);
    getsockname(connection->realsocket_recv, (struct sockaddr*) &local_addr, &lsize);
    getpeername(connection->realsocket_recv, (struct sockaddr*) &peer_addr, &psize);

    do
    {
        /* Receive a chiTCP header */
        nbytes = recv(connection->realsocket_recv, &chitcp_header, sizeof(chitcphdr_t), 0);
        if (nbytes == 0)
        {
            // Server closed the connection
            close(connection->realsocket_recv);
            done = 1;
        }
        else if (nbytes == -1)
        {
            chilog(ERROR, "Socket recv() failed on fd %d: %s", connection->realsocket_recv,
                    strerror(errno));
            close(connection->realsocket_recv);
            pthread_exit(NULL);
        }
        else
        {
            chilog(TRACE, "Received a chiTCP header.");
            chilog_chitcp(TRACE, (uint8_t *)&chitcp_header, LOG_INBOUND);

            /* Inspect the chiTCP header to determine ho w to proceed */

            payload_len = chitcp_ntohs(chitcp_header.payload_len);

            if(chitcp_header.proto == CHITCP_PROTO_TCP)
            {
                /* This means the header will be following by a TCP packet */

                /* Receive the packet */
                nbytes = recv(connection->realsocket_recv, buf, payload_len, 0);
                if (nbytes == 0)
                {
                    // Server closed the connection
                    close(connection->realsocket_recv);
                    done = 1;
                }
                else if (nbytes == -1)
                {
                    chilog(ERROR, "Socket recv() failed on fd %d: %s",
                            connection->realsocket_recv,
                            strerror(errno));
                    close(connection->realsocket_recv);
                    pthread_exit(NULL);
                }
                else
                {
                    chilog(TRACE, "chiTCP packet contains a TCP payload");

                    /* Allocate memory for received TCP packet */
                    tcp_packet_t *packet = malloc(sizeof(tcp_packet_t));
                    packet->raw = malloc(payload_len);
                    memcpy(packet->raw, buf, payload_len);
                    packet->length = payload_len;

                    /* Print the packet to the log */
                    chilog_tcp(TRACE, packet, LOG_INBOUND);

                    /* chitcpd_recv_tcp_packet does the heavy lifting of getting the
                     * packet to the right socket */
                    ret = chitcpd_recv_tcp_packet(si, packet, (struct sockaddr*) &local_addr, (struct sockaddr*) &peer_addr);

                    if(ret != CHITCP_OK)
                    {
                        /* TODO: Should send some sort of ICMP-ish message back to peer.
                         * For now, we just silently drop the packet */
                        chilog(WARNING, "Received a packet but did not find a socket to deliver it to (in real TCP, a ICMP message would be sent back to peer)");
                    }
                }
            }
            else
            {
                chilog(ERROR, "Received a chiTCP with an unknown payload type (proto=%i)", chitcp_header.proto);
                close(connection->realsocket_recv);
                done = 1;
            }

        }
    }
    while (!done);

    pthread_exit(NULL);
}

/*
 * chitcpd_get_connection - Get a TCP connection (if one already exists) to another chiTCP daemon
 *
 * si: Server info
 *
 * addr: Address of peer where chiTCP daemon is running
 *
 * Returns: Pointer to entry in connection table with connection to peer.
 *          NULL if a connection to that peer has not been established yet-
 *
 */
tcpconnentry_t* chitcpd_get_connection(serverinfo_t *si, struct sockaddr* addr)
{
    assert(addr->sa_family == AF_INET || addr->sa_family == AF_INET6);

    tcpconnentry_t *ret = NULL;

    pthread_mutex_lock(&si->lock_connection_table);
    /* Find connection in connection table table */
    for(int i=0; i < si->connection_table_size; i++)
    {
        if(!si->connection_table[i].available)
        {
            if(chitcp_addr_cmp(addr, (struct sockaddr *) &si->connection_table[i].peer_addr) == 0)
            {
                ret = &si->connection_table[i];
                break;
            }
        }
    }
    pthread_mutex_unlock(&si->lock_connection_table);

    return ret;
}


/*
 * chitcpd_get_available_connection_entry - Find an avalable slot in the connection table
 *
 * si: Server info
 *
 * Returns: Pointer to available entry in connection table.
 *          NULL if there are no available entries.
 *
 */
tcpconnentry_t* chitcpd_get_available_connection_entry(serverinfo_t *si)
{
    tcpconnentry_t *ret = NULL;

    /* Find matching slot in socket table */
    for(int i=0; i < si->connection_table_size; i++)
    {
        if(si->connection_table[i].available)
        {
            return &si->connection_table[i];
        }
    }

    return ret;
}


/*
 * chitcpd_create_connection - Establish a connection to another chiTCP daemon
 *
 * si: Server info
 *
 * addr: Address of peer running a chiTCP daemon
 *
 * Returns: Pointer to entry in connection table for this new connection.
 *          NULL if the connection table is full, or if connection was unsuccessful
 *
 */
tcpconnentry_t* chitcpd_create_connection(serverinfo_t *si, struct sockaddr* addr)
{
    assert(addr->sa_family == AF_INET || addr->sa_family == AF_INET6);

    socklen_t addrsize;
    tcpconnentry_t* connection = NULL;

    connection = chitcpd_get_available_connection_entry(si);

    if(connection == NULL)
        return connection;

    connection->available = FALSE;

    /* Set address of peer in connection entry
     * Note that we keep the IP address, but set the port to the chiTCP port
     * (since the peer address will be using an ephemeral port) */
    if(addr->sa_family == AF_INET)
    {
        addrsize = sizeof(struct sockaddr_in);
    }
    else if(addr->sa_family == AF_INET6)
        addrsize = sizeof(struct sockaddr_in6);
    else
    {
        /* Should not happen; assert at top of function will fail first */
        return NULL;
    }


    memcpy(&connection->peer_addr, addr, addrsize);
    chitcp_set_addr_port((struct sockaddr*) &connection->peer_addr, chitcp_htons(GET_CHITCPD_PORT));

    /* Establish connection */
    /* TODO: Fail gracefully if unable to connect to peer */
    connection->realsocket_send = socket(addr->sa_family,
                             SOCK_STREAM,
                             IPPROTO_TCP);

    /* The following value will be set later, either in this function (below)
     * or by the network thread (see comment below). For now, we simply
     * get rid of garbage data. (It is important to do this before calling
     * connect() below, otherwise there is a race condition.) */
    connection->realsocket_recv = -1;

    connect(connection->realsocket_send, (struct sockaddr*) &connection->peer_addr, addrsize);

    /* Create connection thread */

    /* If we're connecting to the loopback address, the connection thread
     * is created by the network thread (when the connection is accepted)
     * not here. The reason is that the connection thread handles all
     * inbound packets (through realsocket_recv), and we do not know what
     * the socket for receiving packets will be until wee accept() the connection.
     */
    if(!chitcp_addr_is_loopback((struct sockaddr *) &connection->peer_addr))
    {
        connection->realsocket_recv = connection->realsocket_send;
        chitcpd_create_connection_thread(si, connection);
    }

    return connection;
}

int chitcpd_create_connection_thread(serverinfo_t *si, tcpconnentry_t* connection)
{
    connection_thread_args_t *cta;
    pthread_t connection_thread;

    /* For naming the connection threads we create (for debugging/logging) */
    static int next_thread_id = 0;

    cta = malloc(sizeof(connection_thread_args_t));
    cta->si = si;
    cta->connection = connection;
    snprintf(cta->thread_name, 16, "connect-%d", next_thread_id++);

    if (pthread_create(&connection_thread, NULL, chitcpd_connection_thread_func, cta) != 0)
    {
        perror("Could not create a connection thread");
        return CHITCP_ETHREAD;
    }

    connection->thread = connection_thread;

    return CHITCP_OK;
}

/*
 * chitcpd_add_connection - Add a connection to the connection table.
 *
 * si: Server info
 *
 * realsocket: TCP socket of connection
 *
 * addr: Address of peer running a chiTCP daemon
 *
 * Returns: Pointer to available entry in connection table.
 *          NULL if connection table is full.
 *
 */
tcpconnentry_t* chitcpd_add_connection(serverinfo_t *si, socket_t realsocket_send, socket_t realsocket_recv, struct sockaddr* addr)
{
    assert(addr->sa_family == AF_INET || addr->sa_family == AF_INET6);

    tcpconnentry_t *ret = NULL;
    socklen_t addrsize;

    ret = chitcpd_get_available_connection_entry(si);

    if(ret == NULL)
        return ret;

    ret->available = FALSE;

    /* Set address of peer in connection entry
     * Note that we keep the IP address, but set the port to the chiTCP port
     * (since the peer address will be using an ephemeral port) */
    if(addr->sa_family == AF_INET)
    {
        addrsize = sizeof(struct sockaddr_in);
    }
    else if(addr->sa_family == AF_INET6)
        addrsize = sizeof(struct sockaddr_in6);
    else
    {
        /* Should not happen; assert at top of function will fail first */
        return NULL;
    }

    memcpy(&ret->peer_addr, addr, addrsize);
    chitcp_set_addr_port((struct sockaddr*) &ret->peer_addr, chitcp_htons(GET_CHITCPD_PORT));

    /* Set sockets */
    ret->realsocket_send = realsocket_send;
    ret->realsocket_recv = realsocket_recv;

    return ret;
}

/*
 * chitcpd_send_tcp_packet - Sends a TCP packet over chiTCP
 *
 * si: Serverinfo struct
 *
 * sock: Socket table entry
 *
 * tcp_packet: TCP packet to send
 *
 * Returns: Number of bytes of data (excluding packet headers) sent
 *
 */
int chitcpd_send_tcp_packet(serverinfo_t *si, chisocketentry_t *sock, tcp_packet_t* tcp_packet)
{
    enum chitcpd_debug_response r = chitcpd_debug_breakpoint(si, ptr_to_fd(si, sock), DBG_EVT_OUTGOING_PACKET, -1);

    if (r == DBG_RESP_DROP)
    {
        chilog(TRACE, "chitcpd_send_tcp_packet: dropping the packet");
        return tcp_packet->length; /* fake that the packet was sent */
    }
    tcpconnentry_t *connection = sock->socket_state.active.realtcpconn;

    /* Allocate memory for the chiTCP header and the TCP packet */
    uint8_t *full_packet = calloc(sizeof(chitcphdr_t) + tcp_packet->length, 1);


    /* Create the chiTCP header */
    chitcphdr_t *header = (chitcphdr_t *) full_packet;
    header->payload_len = chitcp_htons(tcp_packet->length);
    header->proto = CHITCP_PROTO_TCP;

    /* Copy the packet */
    memcpy(full_packet + sizeof(chitcphdr_t), tcp_packet->raw, tcp_packet->length);

    /* Print the chiTCP header and the full TCP packet */
    chilog(TRACE, "Sending a chiTCP packet with a TCP payload.");
    chilog(TRACE, "chiTCP Header:");
    chilog_chitcp(TRACE, (uint8_t*) header, LOG_OUTBOUND);

    chilog(TRACE, "TCP payload:");
    chilog_tcp(TRACE, tcp_packet, LOG_OUTBOUND);

    /* Send the chiTCP header and the TCP packet */
    int nbytes, nleft, nwritten = 0;

    uint8_t *buf = full_packet;
    nleft = sizeof(chitcphdr_t) + tcp_packet->length;

    /* TODO: Possible race condition if multiple TCP threads want to send
     * at the same time *and* this send() doesn't send the entire packet */
    while ( nleft > 0 ) {
        if ( (nbytes = send(connection->realsocket_send, buf, nleft, 0)) <= 0 ) {
            return -1;
        }
        nleft -= nbytes;
        buf += nbytes;
        nwritten += nbytes;
    }
    assert(nwritten == sizeof(chitcphdr_t) + tcp_packet->length);

    free(full_packet);

    return tcp_packet->length;
}

/* chitcpd_enqueue_packet - helper function for chitcpd_recv_tcp_packet
 *
 * Enqueue TCP_PACKET in ENTRY's list of packets, based on the results of a
 * debug_breakpoint. Via this breakpoint, a client can tell the server
 * to drop the packet, or enqueue it normally, or withhold it, etc. */
static void chitcpd_enqueue_packet(serverinfo_t *si, chisocketentry_t *entry, tcp_packet_t* tcp_packet);

/*
 * chitcpd_recv_tcp_packet - Handle the reception of a TCP packet over chiTCP
 *
 * si: Server info
 *
 * tcp_packet: TCP packet that has been received
 *
 * peer_addr: Address of peer from whence the TCP packet arrived
 *
 * Returns:
 *  - CHITCP_OK: Packet was processed correctly
 *  - CHITCP_ESOCKET: Invalid socket
 *
 */
int chitcpd_recv_tcp_packet(serverinfo_t *si, tcp_packet_t* tcp_packet, struct sockaddr *local_realaddr, struct sockaddr *peer_realaddr)
{
    tcphdr_t *header = (tcphdr_t*) tcp_packet->raw;
    chisocketentry_t *entry;
    struct sockaddr_storage local_addr, remote_addr;

    /* We construct the addresses based on the underlying TCP addresses */
    memcpy(&local_addr, local_realaddr, sizeof(struct sockaddr_storage));
    memcpy(&remote_addr, peer_realaddr, sizeof(struct sockaddr_storage));
    chitcp_set_addr_port((struct sockaddr*) &local_addr, header->dest);
    chitcp_set_addr_port((struct sockaddr*) &remote_addr, header->source);

    /* Get entry of socket that will receive this packet */
    entry = chitcpd_lookup_socket(si, (struct sockaddr *) &local_addr, (struct sockaddr *) &remote_addr, FALSE);

    if(entry == NULL)
    {
        chilog(DEBUG, "No socket listening on port %i", chitcp_ntohs(header->dest));

        return CHITCP_ESOCKET;
    }

    int sockfd = SOCKET_NO(si, entry);

    /* The socket has to be either active or passive */
    if(entry->actpas_type == SOCKET_UNINITIALIZED)
    {
        /* This should not happen */
        chilog(DEBUG, "Received packet for socket %i (port %i) but it is not initialized.", sockfd, chitcp_ntohs(header->dest));

        return CHITCP_ESOCKET;
    }

    if(entry->actpas_type == SOCKET_ACTIVE)
    {
        /* If the socket is active, basically all we have to do is add
         * the packet to the socket's pending packet queue */

        chilog(DEBUG, "Received packet for active socket %i (port %i)", sockfd, chitcp_ntohs(header->dest));
        chitcpd_enqueue_packet(si, entry, tcp_packet);
    }
    else if (entry->actpas_type == SOCKET_PASSIVE)
    {
        /* If the socket is passive, this means the packet is probably
         * a SYN packet that is initiating a three-way handshake.
         * However, it is not our responsibility to check this. We
         * simply enqueue the packet as a "pending connection"
         * in the passive socket; if it is a SYN packet, it will spawn
         * an active socket next time the application layer calls
         * chisocket_accept()
         */

        chilog(DEBUG, "Received packet for passive socket %i (port %i)", sockfd, chitcp_ntohs(header->dest));

        passive_chisocket_state_t *socket_state = &entry->socket_state.passive;
        chilog(DEBUG, "Enqueueing packet as pending connection in passive socket");

        /* Create pending connection */
        pending_connection_t *pending_connection = calloc(1, sizeof(pending_connection_t));
        pending_connection->initial_packet = tcp_packet;
        memcpy(&pending_connection->local_addr, &local_addr, sizeof(struct sockaddr_storage));
        memcpy(&pending_connection->remote_addr, &remote_addr, sizeof(struct sockaddr_storage));

        /* Add to queue */
        pthread_mutex_lock(&socket_state->lock_pending_connections);
        list_append(&socket_state->pending_connections, pending_connection);
        pthread_cond_broadcast(&socket_state->cv_pending_connections);
        pthread_mutex_unlock(&socket_state->lock_pending_connections);
    }

    return CHITCP_OK;
}

static void chitcpd_enqueue_packet(serverinfo_t *si, chisocketentry_t *entry, tcp_packet_t* tcp_packet)
{
    active_chisocket_state_t *socket_state = &entry->socket_state.active;
    enum chitcpd_debug_response r =
        chitcpd_debug_breakpoint(si, ptr_to_fd(si, entry), DBG_EVT_INCOMING_PACKET, -1);

    if (r == DBG_RESP_WITHHOLD || r == DBG_RESP_DUPLICATE)
    {
        /* Put the packet on the socket's withheld_packets queue */
        chilog(TRACE, "chitcpd_enqueue_packet: withholding a copy");
        pthread_mutex_lock(&socket_state->tcp_data.lock_withheld_packets);
        list_append(&socket_state->tcp_data.withheld_packets, tcp_packet);
        pthread_mutex_unlock(&socket_state->tcp_data.lock_withheld_packets);
    }

    if (r == DBG_RESP_DRAW_WITHHELD)
    {
        /* Put a previously withheld packet in the socket's packet queue */
        chilog(TRACE, "chitcpd_enqueue_packet: enqueueing a withheld packet");
        pthread_mutex_lock(&socket_state->tcp_data.lock_withheld_packets);
        tcp_packet_t *old_packet =
            (tcp_packet_t *) list_fetch(&socket_state->tcp_data.withheld_packets);
        pthread_mutex_unlock(&socket_state->tcp_data.lock_withheld_packets);
        if (old_packet != NULL)
        {
            pthread_mutex_lock(&socket_state->tcp_data.lock_pending_packets);
            list_append(&socket_state->tcp_data.pending_packets, old_packet);
            pthread_mutex_unlock(&socket_state->tcp_data.lock_pending_packets);
        }
    }

    if (r == DBG_RESP_NONE || r == DBG_RESP_DRAW_WITHHELD || r == DBG_RESP_DUPLICATE)
    {
        chilog(TRACE, "chitcpd_enqueue_packet: enqueueing a copy");

        /* Put the incoming packet in the socket's packet queue */
        pthread_mutex_lock(&socket_state->tcp_data.lock_pending_packets);
        list_append(&socket_state->tcp_data.pending_packets, tcp_packet);
        pthread_mutex_unlock(&socket_state->tcp_data.lock_pending_packets);

        /* Notify the socket that there is a pending packet */
        pthread_mutex_lock(&entry->socket_state.active.lock_event);
        entry->socket_state.active.flags.net_recv = 1;
        pthread_cond_broadcast(&entry->socket_state.active.cv_event);
        pthread_mutex_unlock(&entry->socket_state.active.lock_event);
    }

    if (r == DBG_RESP_DROP)
    {
        /* Nothing actually happens */
        chilog(TRACE, "chitcpd_enqueue_packet: dropping the packet");
    }
}
