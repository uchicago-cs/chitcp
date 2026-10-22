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
#include "chitcp/segment.h"
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
    /* First segment received */
    tcp_segment_t *initial_segment;

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
    /* TCP data (segments, buffers, pointers, etc.) */
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

    /* Number of threads that have looked up this entry with
     * chitcpd_lookup_socket or chitcpd_acquire_socket and are still
     * using it (e.g., to deliver a segment to it), and whether the entry
     * is being freed. An entry is not freed until its reference count
     * drops to zero. Both are protected by the socket table lock.
     * "freeing" is also only set while holding lock_tcp_state (and, for
     * passive sockets, lock_pending_connections), so it can be safely
     * checked while holding any of those locks. */
    int refcount;
    bool_t freeing;

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

    /* Queue for withheld segments (simulating unreliable network) */
    withheld_tcp_segment_list_t *withheld_segments;
    pthread_mutex_t lock_withheld_segments;

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
 * chitcpd_tcp_segment_create - Convenience function to create a TCP segment
 *                             for a specific socket entry
 *
 * Only the source port, destination port, and data offset are initialized.
 *
 * entry: Socket entry
 *
 * segment: Pointer to unitialized tcp_segment_t variable.
 *
 * payload: Pointer to payload. The payload will be DEEP COPIED to the segment.
 *
 * payload_len: Size of the payload in number of bytes.
 *
 * Returns: the size in bytes of the TCP segment.
 *
 */
int chitcpd_tcp_segment_create(chisocketentry_t *entry, tcp_segment_t *segment, const uint8_t* payload, uint16_t payload_len);

/* State of the chiTCP daemon */
typedef enum
{
    CHITCPD_STATE_READY                           = 1,  /* Initialized, ready to run */
    CHITCPD_STATE_STARTING                        = 2,  /* Starting */
    CHITCPD_STATE_RUNNING                         = 3,  /* Daemon is running */
    CHITCPD_STATE_STOPPING                        = 4,  /* Daemon is stopping; all threads must exit */
    CHITCPD_STATE_STOPPED                         = 5,  /* Daemon is stopped; must reinitialize to run again */
} chitcpd_state_t;


/* The segment_delivery_list_entry_t is used to keep track
 * of segments receoived from the network layer, and which
 * have to be delivered to the appropriate socket */
typedef struct segment_delivery_list_entry
{
    /* Note: we store the segment's addresses, and not a pointer to the socket
     * entry, because the socket could be freed before the segment is delivered.
     * The socket is looked up when the segment is actually delivered. */
    tcp_segment_t* tcp_segment;
    struct timespec delivery_time;
    char* log_prefix;
    struct sockaddr_storage local_addr;
    struct sockaddr_storage remote_addr;

    struct segment_delivery_list_entry *prev;
    struct segment_delivery_list_entry *next;
} segment_delivery_list_entry_t;


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

    /* This is the thread that delivers the segments received
     * by the network thread (possibly delayed by a latency) */
    pthread_t delivery_thread;
    segment_delivery_list_entry_t *delivery_queue;
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
    pthread_cond_t cv_chisocket_table;

    /* Number of TCP threads that are still running. When the daemon
     * stops, it waits for this to drop to zero (so no TCP thread is
     * still using the socket table when it is freed). Protected by
     * lock_chisocket_table (and signalled through cv_chisocket_table) */
    int num_tcp_threads;

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
 * Besides updating the state, this function wakes up any threads waiting
 * for the socket to change state (e.g., a connect() or accept() call),
 * and triggers the DBG_EVT_TCP_STATE_CHANGE debug breakpoint. If the
 * debug handler responds with DBG_RESP_DRAW_WITHHELD, one of the socket's
 * withheld segments (if any) is placed in the delivery queue.
 *
 * If the new state is CLOSED, the socket is also flagged for cleanup:
 * once the current event has been handled, the TCP thread will generate
 * a CLEANUP event, and will then free the socket (including its TCP data).
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
 * If any other threads are using the socket entry (see chitcpd_lookup_socket),
 * this function will block until they release it. The caller must not
 * hold any locks that those threads may need to acquire (in particular,
 * the socket's lock_event or lock_pending_segments).
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


/*
 * chitcpd_lookup_socket - Find the socket that matches a pair of addresses
 *
 * If a socket is found, its reference count is incremented, to ensure
 * that it will not be freed while the caller is using it. The caller
 * MUST call chitcpd_release_socket once it is done using the entry.
 * Sockets that are in the process of being freed are not returned.
 *
 * si: Server info
 *
 * local_addr, remote_addr: Addresses to match
 *
 * exact_match_only: If true, do not return matches with wildcard addresses
 *
 * Returns: Pointer to the socket entry, or NULL if no socket matches.
 *
 */
chisocketentry_t* chitcpd_lookup_socket(serverinfo_t *si, struct sockaddr *local_addr, struct sockaddr *remote_addr, bool_t exact_match_only);


/*
 * chitcpd_acquire_socket - Get the entry for a socket descriptor
 *
 * As with chitcpd_lookup_socket, the entry's reference count is incremented,
 * and the caller MUST call chitcpd_release_socket once it is done using it.
 *
 * si: Server info
 *
 * sockfd: Socket descriptor
 *
 * Returns: Pointer to the socket entry, or NULL if the descriptor is not
 *          valid, or if the socket has been freed (or is being freed)
 *
 */
chisocketentry_t* chitcpd_acquire_socket(serverinfo_t *si, int sockfd);


/*
 * chitcpd_release_socket - Release a socket returned by chitcpd_lookup_socket
 *                          or chitcpd_acquire_socket
 *
 * si: Server info
 *
 * entry: Pointer to entry in socket table.
 *
 * Returns: Nothing
 *
 */
void chitcpd_release_socket(serverinfo_t *si, chisocketentry_t *entry);

void tcp_data_init(serverinfo_t *si, chisocketentry_t *entry);
void tcp_data_free(serverinfo_t *si, chisocketentry_t *entry);

#endif /* SERVERINFO_H_ */
