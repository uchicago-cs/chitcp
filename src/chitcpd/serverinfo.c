/*
 *  chiTCP - A simple, testable TCP stack
 *
 *  Functions to manipulate chiTCP data structures.
 *
 *  See serverinfo.h for data structure declarations, as well
 *  as descriptions of functions.
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

#include <string.h>
#include <assert.h>
#include "serverinfo.h"
#include "chitcp/addr.h"
#include "chitcp/debug_api.h"
#include "chitcp/log.h"
#include "breakpoint.h"



/*
 * chitcpd_set_header_ports - Set the TCP ports on a packet, based on the local/remote
 *                            addresses in a socket entry.
 *
 * Note: This only makes sense in active sockets.
 *
 * entry: Socket entry
 *
 * header: Pointer to TCP header
 *
 * Returns: nothing.
 *
 */
void chitcpd_set_header_ports(chisocketentry_t *entry, tcphdr_t *header)
{
    assert(entry->actpas_type == SOCKET_ACTIVE);

    header->source = chitcp_get_addr_port( (struct sockaddr*) &entry->local_addr);
    header->dest = chitcp_get_addr_port( (struct sockaddr*) &entry->remote_addr);
}

/* See serverinfo.h */
int chitcpd_tcp_packet_create(chisocketentry_t *entry, tcp_packet_t *packet, const uint8_t* payload, uint16_t payload_len)
{
    int packet_len;

    packet_len = chitcp_tcp_packet_create(packet, payload, payload_len);
    chitcpd_set_header_ports(entry, TCP_PACKET_HEADER(packet));

    return packet_len;
}

/* See serverinfo.h */
void chitcpd_update_tcp_state(serverinfo_t *si, chisocketentry_t *entry, tcp_state_t newstate)
{
    pthread_mutex_lock(&entry->lock_tcp_state);
    entry->tcp_state = newstate;
    pthread_cond_broadcast(&entry->cv_tcp_state);

    chitcpd_debug_breakpoint(si, ptr_to_fd(si, entry), DBG_EVT_TCP_STATE_CHANGE, -1);

    pthread_mutex_unlock(&entry->lock_tcp_state);

    if (newstate == CLOSED && entry->actpas_type == SOCKET_ACTIVE)
    {
        active_chisocket_state_t *socket_state = &entry->socket_state.active;

        pthread_mutex_lock(&socket_state->lock_event);
        socket_state->flags.cleanup = 1;
        pthread_cond_broadcast(&socket_state->cv_event);
        pthread_mutex_unlock(&socket_state->lock_event);
    }
}

/* See serverinfo.h */
void chitcpd_timeout(serverinfo_t *si, chisocketentry_t *entry)
{
    active_chisocket_state_t *socket_state = &entry->socket_state.active;
    pthread_mutex_lock(&socket_state->lock_event);
    socket_state->flags.timeout = 1;
    pthread_cond_broadcast(&socket_state->cv_event);
    pthread_mutex_unlock(&socket_state->lock_event);
}

/* See serverinfo.h */
int chitcpd_allocate_socket(serverinfo_t *si, int *socket_index)
{
    int ret;
    chisocketentry_t *entry = NULL;

    pthread_mutex_lock(&si->lock_chisocket_table);

    /* Find available slot in socket table */
    for(int i=0; i < si->chisocket_table_size; i++)
    {
        if(si->chisocket_table[i].available)
        {
            si->chisocket_table[i].available = FALSE;

            entry = &si->chisocket_table[i];
            *socket_index = i;
            break;
        }
    }
    pthread_mutex_unlock(&si->lock_chisocket_table);

    if(entry == NULL)
    {
        chilog(DEBUG, "Did not find an available socket slot.");
        ret = CHITCP_ESOCKET;
    }
    else
    {
        chilog(DEBUG, "Assigned socket %i", *socket_index);

        entry->creator_thread = pthread_self();

        entry->actpas_type = SOCKET_UNINITIALIZED;
        entry->tcp_state = CLOSED;

        pthread_mutex_init(&entry->lock_tcp_state, NULL);
        pthread_cond_init(&entry->cv_tcp_state, NULL);

        ret = CHITCP_OK;
    }

    return ret;
}

/* See serverinfo.h */
int chitcpd_free_socket_entry(serverinfo_t *si, chisocketentry_t *entry)
{
    uint16_t port;
    struct sockaddr *addr;

    if(entry->actpas_type == SOCKET_PASSIVE)
    {
        chilog(TRACE, "Freeing entry for passive socket %i", SOCKET_NO(si, entry));

        passive_chisocket_state_t *socket_state = &entry->socket_state.passive;
        list_destroy(&socket_state->pending_connections);
        pthread_mutex_destroy(&socket_state->lock_pending_connections);
        pthread_cond_destroy(&socket_state->cv_pending_connections);
    }
    else if(entry->actpas_type == SOCKET_ACTIVE)
    {
        chilog(TRACE, "Freeing entry for active socket %i", SOCKET_NO(si, entry));

        active_chisocket_state_t *socket_state = &entry->socket_state.active;

        tcp_data_free(&socket_state->tcp_data);

        pthread_mutex_unlock(&socket_state->lock_event);
        pthread_mutex_destroy(&socket_state->lock_event);
        pthread_cond_destroy(&socket_state->cv_event);
    }

    pthread_mutex_destroy(&entry->lock_tcp_state);
    pthread_cond_destroy(&entry->cv_tcp_state);

    if (entry->debug_monitor != NULL)
    {
        chitcpd_debug_detach_monitor(entry);
    }
    pthread_mutex_destroy(&entry->lock_debug_monitor);

    /* Mark local port as available */
    addr = (struct sockaddr*) &entry->local_addr;
    if ((port = chitcp_ntohs(chitcp_get_addr_port(addr))) >= 0)
        si->port_table[port] = NULL;

    memset(entry, 0, sizeof(chisocketentry_t));

    entry->available = TRUE;

    chilog(TRACE, "Finished freeing entry for socket %i", SOCKET_NO(si, entry));

    return CHITCP_OK;
}

/* See serverinfo.h */
int chitcpd_find_ephemeral_port(serverinfo_t *si)
{
    uint16_t port = -1;

    for(int i = si->ephemeral_port_start; i < si->port_table_size; i++)
    {
        if(si->port_table[i] == NULL)
        {
            port = i;
            break;
        }
    }

    return port;
}


/* See serverinfo.h */
/* This implementation is based on the implementation of in_pcblookup in
 * TCP/IP Illustrated, Volume 2.
 */
chisocketentry_t* chitcpd_lookup_socket(serverinfo_t *si, struct sockaddr *local_addr, struct sockaddr *remote_addr, bool_t exact_match_only)
{
    assert(local_addr->sa_family == AF_INET || local_addr->sa_family == AF_INET6);
    assert(remote_addr->sa_family == AF_INET || remote_addr->sa_family == AF_INET6);
    assert(local_addr->sa_family == remote_addr->sa_family);

    int max_nwildcards = 3;
    chisocketentry_t *match = NULL;
    for(int i=0; i < si->chisocket_table_size; i++)
    {
        chisocketentry_t *this_entry = &si->chisocket_table[i];
        int nwildcards = 0;

        if(this_entry->available)
            continue;

        /* If the families differ, this is not the socket we're looking for */
        if(local_addr->sa_family != this_entry->local_addr.ss_family)
            continue;

        if(remote_addr->sa_family != this_entry->remote_addr.ss_family)
            continue;

        /* Check whether the local ports match */
        if(chitcp_addr_port_cmp(local_addr, (struct sockaddr *) &this_entry->local_addr))
            continue;

        /* Check whether the local address matches, accounting for
         * the fact that the socket could have a wildcard local address */
        if(chitcp_addr_is_any((struct sockaddr *) &this_entry->local_addr))
        {
            if(!chitcp_addr_is_any(local_addr))
                nwildcards++;
        }
        else
        {
            if(chitcp_addr_is_any(local_addr))
                nwildcards++;
            else if (chitcp_addr_cmp(local_addr, (struct sockaddr *) &this_entry->local_addr))
                continue;
        }

        /* Likewise for the remote address, but checking the ports too */
        if(chitcp_addr_is_any((struct sockaddr *) &this_entry->remote_addr))
        {
            if(!chitcp_addr_is_any(remote_addr))
                nwildcards++;
        }
        else
        {
            if(chitcp_addr_is_any(remote_addr))
                nwildcards++;
            else if (chitcp_addr_cmp(remote_addr, (struct sockaddr *) &this_entry->remote_addr) ||
                     chitcp_addr_port_cmp(remote_addr, (struct sockaddr *) &this_entry->remote_addr))
                continue;
        }

        /* If we're looking for an exact match only, we're not
         * interested in matches with wildcards */
        if(nwildcards > 0 && exact_match_only)
            continue;

        if(nwildcards < max_nwildcards)
        {
            match = this_entry;
            max_nwildcards = nwildcards;

            /* If we have an exact match, we've found what we're
             * looking for */
            if(nwildcards == 0)
                break;
        }
    }

    return match;
}
