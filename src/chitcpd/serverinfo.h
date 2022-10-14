/*
 *  chiTCP - A simple, testable TCP stack
 *
 *  This header file defines the main data structures used by the
 *  chiTCP daemon. It also declares a few functions that operate
 *  on those data structures.
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

#ifndef SERVERINFO_H_
#define SERVERINFO_H_

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#include "tcp.h"
#include "chitcp/types.h"
#include "chitcp/packet.h"
#include "chitcp/debug_api.h"

#define DEFAULT_MAX_SOCKETS (1024u)
#define DEFAULT_MAX_PORTS (65536u)
#define DEFAULT_MAX_CONNECTIONS (1024u)
#define DEFAULT_EPHEMERAL_PORT_START (49152u)

typedef struct chisocketentry chisocketentry_t;

/* Represents single TCP connection between chiTCP daemons */
typedef struct tcpconnentry
{
    /* Is this entry available? */
    bool_t available;

    /* Thread that is handling this connection */
    pthread_t thread;

    /* Real TCP sockets associated with this connection.
     * Note that, when two peers are distinct, these sockets
     * will have the same value. When connecting to the
     * loopback address, they'll have different values. */
    socket_t realsocket_send;
    socket_t realsocket_recv;

    /* Peer chiTCP daemon */
    struct sockaddr_storage peer_addr;

} tcpconnentry_t;


/* A pending connection in a passive socket */
typedef struct pending_connection
{
    /* First packet received */
    tcp_packet_t *initial_packet;

    /* Addresses */
    struct sockaddr_storage local_addr;
    struct sockaddr_storage remote_addr;

    /* List pointers */
    struct pending_connection *prev;
    struct pending_connection *next;
} pending_connection_t;


/* State that is specific to active sockets */
typedef struct active_chisocket_state
{
    /* TCP data (packets, buffers, pointers, etc.) */
    tcp_data_t tcp_data;

    /* Passive socket (if any) from which this socket was created */
    chisocketentry_t* parent_socket;

    /* Event flags, with lock and condition variable */
    union
    {
        struct
        {
            uint8_t app_connect:1,  /* Application has called connect() */
                    app_send:1,     /* Application has data to send */
                    app_recv:1,     /* Application has read data from the buffer */
                    net_recv:1,     /* Data has arrived through the network */
                    app_close:1,    /* Application has requested the connection be closed */
                    timeout_rtx:1,  /* A retransmission timeout has occurred. */
                    timeout_pst:1,  /* A persist timeout has occurred. */
                    cleanup:1;      /* Socket must release all its resources */
        };
        uint8_t raw;
    } flags;
    pthread_mutex_t lock_event;
    pthread_cond_t cv_event;

    /* Thread that does all the magic */
    pthread_t tcp_thread;

    /* Real TCP connection for this socket */
    tcpconnentry_t *realtcpconn;

} active_chisocket_state_t;

/* State that is specific to passive sockets */
typedef struct passive_chisocket_state
{
    /* Socket backlog */
    int backlog;

    /* Queue with pending connections received from the network */
    pending_connection_t *pending_connections;
    pthread_mutex_t lock_pending_connections;
    pthread_cond_t cv_pending_connections;
} passive_chisocket_state_t;


/* Container for thread-safe debug communication */
typedef struct debug_monitor
{
    pthread_mutex_t lock_sockfd;
    pthread_mutex_t lock_numwaiters;
    int numwaiters; /* Number of threads blocked on lock_sockfd */
    bool_t dying;   /* Has the peer closed the sockfd connection? */
    int sockfd;     /* The UNIX socket to which to write debug messages */
    int ref_count;  /* The number of chisockets registered to this monitor */
} debug_monitor_t;

/* Entry in socket table */
typedef struct chisocketentry
{
    /* Is this entry available? */
    bool_t available;

    /* Socket domain
     * Only AF_INET and AF_INET6 are supported. */
    int domain;

    /* Socket type
     * Only SOCK_STREAM and SOCK_RAW are supported. */
    int type;

    /* Socket protocol
     * Only IPPROTO_TCP and IPPROTO_RAW are supported */
    int protocol;

    /* Addresses */
    struct sockaddr_storage local_addr;
    struct sockaddr_storage remote_addr;

    /* TCP state (CLOSED, SYN_SENT, LISTEN, etc.) */
    tcp_state_t tcp_state;
    pthread_mutex_t lock_tcp_state;
    pthread_cond_t cv_tcp_state;

    /* Socket type: active or passive */
    socket_type_t actpas_type;

    /* Thread that created this entry */
    pthread_t creator_thread;

    /* Queue for withheld packets (simulating unreliable network) */
    withheld_tcp_packet_list_t *withheld_packets;
    pthread_mutex_t lock_withheld_packets;

    /* For debug communications */
    debug_monitor_t *debug_monitor;
    pthread_mutex_t lock_debug_monitor;
    int event_flags;

    union
    {
        active_chisocket_state_t active;
        passive_chisocket_state_t passive;
    } socket_state;

} chisocketentry_t;


/*
 * chitcpd_tcp_packet_create - Convenience function to create a TCP packet
 *                             for a specific socket entry
 *
 * Only the source port, destination port, and data offset are initialized.
 *
 * entry: Socket entry
 *
 * packet: Pointer to unitialized tcp_packet_t variable.
 *
 * payload: Pointer to payload. The payload will be DEEP COPIED to the packet.
 *
 * payload_len: Size of the payload in number of bytes.
 *
 * Returns: the size in bytes of the TCP packet.
 *
 */
int chitcpd_tcp_packet_create(chisocketentry_t *entry, tcp_packet_t *packet, const uint8_t* payload, uint16_t payload_len);

/* State of the chiTCP daemon */
typedef enum
{
    CHITCPD_STATE_READY                           = 1,  /* Initialized, ready to run */
    CHITCPD_STATE_STARTING                        = 2,  /* Starting */
    CHITCPD_STATE_RUNNING                         = 3,  /* Daemon is running */
    CHITCPD_STATE_STOPPING                        = 4,  /* Daemon is stopping; all threads must exit */
    CHITCPD_STATE_STOPPED                         = 5,  /* Daemon is stopped; must reinitialize to run again */
} chitcpd_state_t;


/* The packet_delivery_list_entry_t is used to keep track
 * of packets receoived from the network layer, and which
 * have to be delivered to the appropriate socket */
typedef struct packet_delivery_list_entry
{
    chisocketentry_t *entry;
    tcp_packet_t* tcp_packet;
    struct timespec delivery_time;
    char* log_prefix;
    struct sockaddr_storage local_addr;
    struct sockaddr_storage remote_addr;

    struct packet_delivery_list_entry *prev;
    struct packet_delivery_list_entry *next;
} packet_delivery_list_entry_t;


/* The serverinfo_t struct is a singleton data structure that contains
 * all the state for the chiTCP daemon. It is often the first parameter
 * in most chitcpd_* functions. */
typedef struct serverinfo
{
    /* Daemon state */
    chitcpd_state_t state;

    /* Condition variable to signal changes in server state */
    pthread_mutex_t lock_state;
    pthread_cond_t cv_state;

    /* Daemon TCP port and UNIX socket path */
    uint16_t server_port;
    char server_socket_path[UNIX_PATH_MAX];

    /* This is the thread that listens on a UNIX socket
     * for chiTCP requests. */
    pthread_t server_thread;
    socket_t server_socket;

    /* This is the thread that listens on a TCP port for
     * incoming chiTCP packets */
    pthread_t network_thread;
    socket_t network_socket;

    /* This is the thread that delivers the packets received
     * by the network thread (possibly delayed by a latency) */
    pthread_t delivery_thread;
    packet_delivery_list_entry_t *delivery_queue;
    pthread_mutex_t lock_delivery;
    pthread_cond_t cv_delivery;
    double latency;

    /* Connections to other chiTCP daemons */
    uint16_t connection_table_size;
    tcpconnentry_t *connection_table;
    pthread_mutex_t lock_connection_table;

    /* Socket table */
    uint16_t chisocket_table_size;
    chisocketentry_t *chisocket_table;
    pthread_mutex_t lock_chisocket_table;

    /* Table of pointers to socket entries.
     * If an entry is NULL, the port is available.
     * If not NULL, it contains a pointer to the socket that
     * is assigned to that port. */
    uint32_t port_table_size;
    uint16_t ephemeral_port_start;
    chisocketentry_t **port_table;

    /* The libcap file that this server is logging to. */
    const char *libpcap_file_name;
    FILE *libpcap_file;

} serverinfo_t;

#define SOCKET_NO(si, entry) ((int) (entry - si->chisocket_table))

/*
 * chitcpd_update_tcp_state - Updates the TCP state of a socket
 *
 * si: serverinfo struct
 *
 * entry: Socket entry
 *
 * new state: New state
 *
 * Returns: Nothing
 *
 */
void chitcpd_update_tcp_state(serverinfo_t *si, chisocketentry_t *entry, tcp_state_t newstate);

/*
 * chitcpd_timeout - Triggers a timeout of a socket.
 *
 * si: serverinfo struct
 *
 * entry: Socket entry
 *
 * type: Type of timeout
 *
 * Returns: Nothing
 *
 */
void chitcpd_timeout(serverinfo_t *si, chisocketentry_t *entry, tcp_timer_type_t type);


/*
 * chitcpd_allocate_socket - Allocate a socket entry
 *
 * si: Server info
 *
 * socket_entry: Output parameter with the index of the allocated entry
 *
 * Returns:
 *   - CHITCP_OK: Socket entry allocated succesfully
 *   - CHITCP_ESOCKET: No socket entries available *
 *
 */
int chitcpd_allocate_socket(serverinfo_t *si, int *socket_entry);


/*
 * chitcpd_free_socket_entry - Free resources allocated to a socket
 *
 * si: Server info
 *
 * entry: Pointer to entry in socket table.
 *
 * Returns:
 *   - CHITCP_OK: Socket entry allocated succesfully
 *
 */
int chitcpd_free_socket_entry(serverinfo_t *si, chisocketentry_t *entry);


/*
 * chitcpd_find_ephemeral_port - Find an ephemeral port
 *
 * si: Server info
 *
 * Returns: -1 if no ephemeral ports are available.
 *          Otherwise, the port number.
 *
 */
int chitcpd_find_ephemeral_port(serverinfo_t *si);


chisocketentry_t* chitcpd_lookup_socket(serverinfo_t *si, struct sockaddr *local_addr, struct sockaddr *remote_addr, bool_t exact_match_only);

void tcp_data_init(serverinfo_t *si, chisocketentry_t *entry);
void tcp_data_free(serverinfo_t *si, chisocketentry_t *entry);

#endif /* SERVERINFO_H_ */
