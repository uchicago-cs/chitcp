/*
 *  chiTCP - A simple, testable TCP stack
 *
 *  The TCP thread is the one that actually handles the TCP-over-TCP
 *  connection (not to be confused with the real TCP connection between
 *  chiTCP daemons handled in connection.c). Every active socket
 *  has its own TCP thread (which will be created when
 *  chisocket_connect or chisocket_accept is called).
 *
 *  Note how, in general, the chisocket layer never runs the TCP protocol;
 *  instead, it modifies the tcp_data field of an active socket (e.g., by
 *  enqueueing a new packet that arrived through the network), and notifies
 *  the TCP thread that it needs to wake up and handle this change in the
 *  TCP data.
 *
 *  The actual implementation of the TCP protocol is in tcp.c, and the code
 *  in this file only handles the dispatch of events and packets to the
 *  appropriate functions in tcp.c.
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
#include <assert.h>

#include "serverinfo.h"
#include "connection.h"
#include "tcp_thread.h"
#include "chitcp/packet.h"
#include "chitcp/types.h"
#include "chitcp/log.h"
#include "chitcp/debug_api.h"
#include "breakpoint.h"

/* Dispatch table */

typedef int (*tcp_state_handler_function)(serverinfo_t *__si, chisocketentry_t *__entry, tcp_event_type_t __event);

#define TCP_STATE_HANDLER_NAME(NAME) chitcpd_tcp_state_handle_ ## NAME
#define TCP_STATE_HANDLER_ENTRY(NAME) [NAME] = chitcpd_tcp_state_handle_ ## NAME
#define TCP_STATE_HANDLER_FUNCTION(NAME) int chitcpd_tcp_state_handle_ ## NAME (serverinfo_t *__si, chisocketentry_t *__entry, tcp_event_type_t __event)

TCP_STATE_HANDLER_FUNCTION(CLOSED);
TCP_STATE_HANDLER_FUNCTION(LISTEN);
TCP_STATE_HANDLER_FUNCTION(SYN_RCVD);
TCP_STATE_HANDLER_FUNCTION(SYN_SENT);
TCP_STATE_HANDLER_FUNCTION(ESTABLISHED);
TCP_STATE_HANDLER_FUNCTION(FIN_WAIT_1);
TCP_STATE_HANDLER_FUNCTION(FIN_WAIT_2);
TCP_STATE_HANDLER_FUNCTION(CLOSE_WAIT);
TCP_STATE_HANDLER_FUNCTION(CLOSING);
TCP_STATE_HANDLER_FUNCTION(TIME_WAIT);
TCP_STATE_HANDLER_FUNCTION(LAST_ACK);

tcp_state_handler_function tcp_state_handlers[] =
{
    TCP_STATE_HANDLER_ENTRY(CLOSED),
    TCP_STATE_HANDLER_ENTRY(LISTEN),
    TCP_STATE_HANDLER_ENTRY(SYN_RCVD),
    TCP_STATE_HANDLER_ENTRY(SYN_SENT),
    TCP_STATE_HANDLER_ENTRY(ESTABLISHED),
    TCP_STATE_HANDLER_ENTRY(FIN_WAIT_1),
    TCP_STATE_HANDLER_ENTRY(FIN_WAIT_2),
    TCP_STATE_HANDLER_ENTRY(CLOSE_WAIT),
    TCP_STATE_HANDLER_ENTRY(CLOSING),
    TCP_STATE_HANDLER_ENTRY(TIME_WAIT),
    TCP_STATE_HANDLER_ENTRY(LAST_ACK)
};


void chilog_tcp_data(loglevel_t level, tcp_data_t *tcp_data, tcp_state_t state)
{
    int snd_buf_size, snd_buf_capacity, rcv_buf_size, rcv_buf_capacity;

    snd_buf_size = circular_buffer_count(&tcp_data->send);
    snd_buf_capacity = circular_buffer_capacity(&tcp_data->send);
    rcv_buf_size = circular_buffer_count(&tcp_data->recv);
    rcv_buf_capacity = circular_buffer_capacity(&tcp_data->recv);

    flockfile(stdout);
    chilog(level, "   ······················································");
    chilog(level, "                         %s", tcp_str(state));
    chilog(level, "");
    chilog(level, "            ISS:  %10i           IRS:  %10i", tcp_data->ISS, tcp_data->IRS);
    chilog(level, "        SND.UNA:  %10i ", tcp_data->SND_UNA);
    chilog(level, "        SND.NXT:  %10i       RCV.NXT:  %10i ", tcp_data->SND_NXT, tcp_data->RCV_NXT);
    chilog(level, "        SND.WND:  %10i       RCV.WND:  %10i ", tcp_data->SND_WND, tcp_data->RCV_WND);
    chilog(level, "    Send Buffer: %4i / %4i   Recv Buffer: %4i / %4i", snd_buf_size, snd_buf_capacity, rcv_buf_size, rcv_buf_capacity);
    chilog(level, "");
    chilog(level, "       Pending packets: %4i    Closing? %s", list_size(&tcp_data->pending_packets), tcp_data->closing?"YES":"NO");
    chilog(level, "   ······················································");
    funlockfile(stdout);
}



/*
 * chitcpd_dispatch_tcp - Dispatch an event to TCP
 *
 * Uses dispatch table defined above.
 *
 * si: Server info
 *
 * entry: Pointer to socket entry
 *
 * event: Event being dispatched
 *
 * Returns: Nothing.
 *
 */
void chitcpd_dispatch_tcp(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event)
{
    int rc;
    tcp_state_t state = entry->tcp_state;
    active_chisocket_state_t *socket_state = &entry->socket_state.active;

    chilog(DEBUG, ">>> Handling event %s on state %s", tcp_event_str(event), tcp_str(state));
    chilog(DEBUG, ">>> TCP data BEFORE handling:");
    chilog_tcp_data(DEBUG, &socket_state->tcp_data, state);

    rc = tcp_state_handlers[state](si, entry, event);

    chilog(DEBUG, "<<< TCP data AFTER handling:");
    chilog_tcp_data(DEBUG, &socket_state->tcp_data, entry->tcp_state);
    if(state != entry->tcp_state)
    {
        chilog(DEBUG, "<<< Finished handling event %s on state %s", tcp_event_str(event), tcp_str(state));
        chilog(DEBUG, "<<< New state: %s", tcp_str(entry->tcp_state));
    }
    else
        chilog(DEBUG, "<<< Finished handling event %s on state %s", tcp_event_str(event), tcp_str(state));

    if(rc != CHITCP_OK)
    {
        chilog(ERROR, "Error when handling event %s on state %s", tcp_event_str(event), tcp_str(state));
    }
}


/* Advance declaration of TCP thread function */
void* chitcpd_tcp_thread_func(void *args);

typedef struct tcp_thread_args
{
    serverinfo_t *si;
    chisocketentry_t *entry;
    char thread_name[16];
} tcp_thread_args_t;


/*
 * chitcpd_tcp_start_thread - Starts a TCP thread
 *
 * si: Server info
 *
 * entry: Pointer to socket entry for which a TCP thread is being started
 *
 * Returns: TODO
 *
 */
int chitcpd_tcp_start_thread(serverinfo_t *si, chisocketentry_t *entry)
{
    tcp_thread_args_t *tta = malloc(sizeof(tcp_thread_args_t));
    tta->si = si;
    tta->entry = entry;
    snprintf (tta->thread_name, 16, "tcp--fd-%d", ptr_to_fd(si, entry));

    if (pthread_create(&entry->socket_state.active.tcp_thread, NULL, chitcpd_tcp_thread_func, tta) < 0)
    {
        perror("Could not create TCP thread");
        free(tta);
        return CHITCP_ETHREAD;
    }

    return CHITCP_OK;
}


/*
 * chitcpd_tcp_thread_func - TCP thread function
 *
 * args: arguments (in tcp_thread_args_t)
 *
 * Returns: TODO
 *
 */
void* chitcpd_tcp_thread_func(void *args)
{
    tcp_thread_args_t *tta = (tcp_thread_args_t *) args;
    serverinfo_t *si = tta->si;
    chisocketentry_t *entry = tta->entry;
    pthread_setname_np(pthread_self(), tta->thread_name);
    active_chisocket_state_t *socket_state = &entry->socket_state.active;
    tcp_data_t *tcp_data = &socket_state->tcp_data;
    int done = FALSE;

    /* Detach thread */
    pthread_detach(pthread_self());

    /* Initialize buffers */
    circular_buffer_init(&tcp_data->send, TCP_BUFFER_SIZE);
    circular_buffer_init(&tcp_data->recv, TCP_BUFFER_SIZE);

    chilog(DEBUG, "TCP thread running");

    /* The TCP thread is basically an event loop, where we wait for an
     * event to happen that merits waking up the TCP thread. The events
     * are indicated through the active socket entry's "flags" attribute.
     *
     * The possible events are:
     *
     * - app_close: Either the application has explicitly called chisocket_close()
     *              or chitcpd has determined this socket has to be closed.
     *
     * - app_connect: The application has called chisocket_connect()
     *
     * - app_recv: The application has called chisocket_recv()
     *
     * - app_send: The application has called chisocket_send()
     *
     * - net_recv: A packet has arrived over the network
     *
     * - cleanup: The thread must release its resources and exit
     *
     * - timeout: A timeout has expired and the TCP thread must handle it.
     *
     * Once an event is received, we clear the corresponding bit in the flags,
     * and release the lock on the flags. This means that, while the event is being
     * processed, the corresponding flag could be raised again (which means
     * the event loop just happens again, without having to go to sleep).
     *
     * For the most part, handling an event just involves calling
     * chitcpd_dispatch_tcp to call the appropriate function in tcp.c,
     * which will actually run through the TCP protocol.
     *
     */
    while(!done)
    {
        /* Wait for event */
        chilog(TRACE, "Waiting for TCP event");
        pthread_mutex_lock(&socket_state->lock_event);
        while(socket_state->flags.raw == 0)
            pthread_cond_wait(&socket_state->cv_event, &socket_state->lock_event);

        if(socket_state->flags.app_close)
        {
            chilog(TRACE, "Event received: app_close");
            socket_state->flags.app_close = 0;
            pthread_mutex_unlock(&socket_state->lock_event);

            chitcpd_dispatch_tcp(si, entry, APPLICATION_CLOSE);
        }
        else if(socket_state->flags.app_connect)
        {
            chilog(TRACE, "Event received: app_connect");
            socket_state->flags.app_connect = 0;
            pthread_mutex_unlock(&socket_state->lock_event);

            chitcpd_dispatch_tcp(si, entry, APPLICATION_CONNECT);
        }
        else if(socket_state->flags.app_recv)
        {
            chilog(TRACE, "Event received: app_recv");
            socket_state->flags.app_recv = 0;
            pthread_mutex_unlock(&socket_state->lock_event);

            chitcpd_dispatch_tcp(si, entry, APPLICATION_RECEIVE);
        }
        else if(socket_state->flags.app_send)
        {
            chilog(TRACE, "Event received: app_send");
            socket_state->flags.app_send = 0;
            pthread_mutex_unlock(&socket_state->lock_event);

            chitcpd_dispatch_tcp(si, entry, APPLICATION_SEND);
        }
        else if(socket_state->flags.net_recv)
        {
            chilog(TRACE, "Event received: net_recv");

            socket_state->flags.net_recv = 0;
            pthread_mutex_unlock(&socket_state->lock_event);

            chitcpd_dispatch_tcp(si, entry, PACKET_ARRIVAL);

            /* If there are more packets to process, set net_recv to 1 again */
            if(!list_empty(&socket_state->tcp_data.pending_packets))
            {
                pthread_mutex_lock(&socket_state->lock_event);
                socket_state->flags.net_recv = 1;
                pthread_mutex_unlock(&socket_state->lock_event);
            }
        }
        else if(socket_state->flags.cleanup)
        {
            chilog(DEBUG, "Event received: cleanup");

            /* Cleanup can only happen in the CLOSED state */
            assert(entry->tcp_state == CLOSED);

            chitcpd_dispatch_tcp(si, entry, CLEANUP);
            chitcpd_free_socket_entry(si, entry);

            done = TRUE;
        }
        else if(socket_state->flags.timeout)
        {
            chilog(TRACE, "Event received: timeout");
            socket_state->flags.timeout = 0;
            pthread_mutex_unlock(&socket_state->lock_event);

            chitcpd_dispatch_tcp(si, entry, TIMEOUT);
        }
        chilog(TRACE, "TCP event has been handled");
    }

    return NULL;
}
