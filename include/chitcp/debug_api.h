/*
 *  chiTCP - A simple, testable TCP stack
 *
 * user interface for debug calls
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

#ifndef __CHITCPD_DEBUG_API__H_
#define __CHITCPD_DEBUG_API__H_

#include "chitcp/types.h"

/* 
 *
 * Overview of the Debug API:
 *
 * The chitcp daemon defines a number of different breakpoints, which a user
 * can use via the functions in this file to debug a given implementation of
 * tcp. This is done by defining a debug_event_handler (see below), then
 * using chitcpd_debug() (see below) to associate this handler with a given
 * socket and set of breakpoints. When chitcpd reaches a breakpoint, it will
 * send a message to the associated debug_event_handler, wait for a response,
 * then take a specific action depending on the response code (see below).
 *
 * It is not safe to use socket library calls (i.e. the functions defined in
 * include/chitcp/socket.h) from within a debug_event_handler.
 *
 * (The reason for this is that if a socket library call from
 * within a debug_event_handler reaches a breakpoint associated with that
 * debug_event_handler, it will block, and the call will never complete.)
 *
 * On the other hand, chitcpd_get_socket_state() (see below) is perfectly
 * safe, and it is probably the only library call you will need to use from
 * within a debug_event_handler.
 *
 */


/* These are the breakpoints defined by chitcpd. */
enum chitcpd_debug_event {
    DBG_EVT_TCP_STATE_CHANGE    = 1 << 0,
        /* Occurs just after a tcp thread changes the socket's tcp state */
    DBG_EVT_INCOMING_PACKET     = 1 << 1,
        /* Occurs just before passing an incoming packet to the socket */
    DBG_EVT_OUTGOING_PACKET     = 1 << 2,
        /* Occurs just before sending the socket's packet to the network */
    DBG_EVT_PENDING_CONNECTION  = 1 << 3,
        /* Occurs just before starting a new tcp thread for the new active
         * socket associated with the original (passive) socket. */
    DBG_EVT_KILL = 1 << 4
        /* Not used in daemon communications. Used on client side when
         * one thread asks another thread to terminate. */
};


/* Methods for turning an enum value into a string suitable for printing in
 * debug print statements or log messages. */
char *dbg_evt_str(enum chitcpd_debug_event evt);
char *tcp_str(tcp_state_t state);


/* Response codes telling the daemon which action to take at a breakpoint. */
enum chitcpd_debug_response {
    /* For use with any breakpoint. */
    DBG_RESP_NONE, /* take no action (i.e. continue as normal) */
    DBG_RESP_STOP, /* take no action and turn off all breakpoints
                      for the socket */

    /* For use with DBG_EVT_PENDING_CONNECTION. */
    DBG_RESP_ACCEPT_MONITOR,
        /* Use the same set of breakpoints for the new (active) socket as
         * are being used for the old (passive) socket. */

    /* For use with DBG_EVT_INCOMING_PACKET and DBG_EVT_OUTGOING_PACKET. */
    DBG_RESP_DROP, /* drop the packet */

    /* For use with DBG_EVT_INCOMING_PACKET only. */
    DBG_RESP_DUPLICATE, /* receive one copy as normal; withhold another copy */
    DBG_RESP_WITHHOLD, /* withhold the packet */
    DBG_RESP_DRAW_WITHHELD /* receive this packet, and also one of the packets
                              which was previously withheld (if any) */
};


/* Struct containing information about a chisocket, returned by
 * chitcpd_get_socket_state (see below). */
typedef struct debug_socket_state
{
    tcp_state_t tcp_state; /* e.g. ESTABLISHED, SYN_SENT, etc. */
    uint32_t ISS;
    uint32_t IRS;
    uint32_t SND_UNA;
    uint32_t RCV_NXT;
    uint32_t SND_NXT;
    uint32_t RCV_WND;
    uint32_t SND_WND;

    uint8_t *send;
    int send_len;
    uint8_t *recv;
    int recv_len;
} debug_socket_state_t;

/* Print the socket information to stdout. (Useful for debugging.) */
void dump_socket_state(struct debug_socket_state *state, bool_t include_buffers);

/* Saves socket state STATE for use with debug_event_handlers (see below).
 * Returns: ENOMEM on insufficient memory, 0 otherwise */
int chitcpd_debug_save_socket_state(struct debug_socket_state *state);

/*
 * Here is the prototype for a debug_event_handler. These are called when a
 * breakpoint is reached.
 *
 * sockfd:      the socket that generated the event
 * event_flag:  which type of event it was
 * socket_state: contains the current socket state for SOCKFD
 * saved_socket_state: contains the contents of this thread's most recent call
 *                      to chitcpd_debug_save_socket_state
 * new_sockfd:  in the case DBG_EVT_PENDING_CONNECTION, the new (active) socket
 *
 * Note: both SOCKET_STATE and SAVED_SOCKET_STATE will be NULL if SOCKFD is
 * passive. Similarly, NEW_SOCKFD will be -1 if EVENT_FLAG is not
 * DBG_EVT_PENDING_CONNECTION.
 *
 * Returns:     response code corresponding to the action to take
 */
typedef enum chitcpd_debug_response (*debug_event_handler)(
                int sockfd, enum chitcpd_debug_event event_flag,
                debug_socket_state_t *socket_state,
                debug_socket_state_t *saved_socket_state, int new_sockfd);

/* 
 * Registers HANDLER to socket SOCKFD with breakpoints EVENT_FLAGS, so that we
 * will run HANDLER every time an event in EVENT_FLAGS happens to SOCKFD.
 *
 * (Internally, this creates a new connection to chitcpd and a new thread that
 * listens on this connection for debug events, running HANDLER each time one
 * of these events occurs. The thread is destroyed if/when HANDLER returns
 * DBG_RESP_STOP.)
 *
 * EVENT_FLAGS is a bitwise OR of one or more of enum chitcpd_debug_event.
 *
 * Returns:
 *  - 0: success
 *  - -1: error (and sets errno accordingly)
 *
 * Error codes:
 *  EBADF  - SOCKFD is invalid
 *  EAGAIN - SOCKFD already has a registered handler
 *  ENOMEM - no memory (either in daemon process or on client-side)
 *  EPROTO - internal protobuf-c error (please submit a bug report if this happens)
 */
int chitcpd_debug(int sockfd, int event_flags, debug_event_handler handler);


/* Get the interesting socket state associated with SOCKFD (see debug_socket_state
 * above). Caller will need to free the returned structure. If INCLUDE_BUFFERS,
 * this structure will have non-null values for members "send" and "recv" (see
 * debug_socket_state_t above); these will also need to be freed.
 *
 * Returns NULL if SOCKFD is invalid (in particular, if SOCKFD is not active). */
debug_socket_state_t *chitcpd_get_socket_state(int sockfd, bool_t include_buffers);


int chitcpd_wait_for_state(int sockfd, tcp_state_t tcp_state);

#endif /* __CHITCPD_DEBUG_API__H_ */
