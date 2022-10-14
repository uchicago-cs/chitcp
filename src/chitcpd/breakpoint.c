/*
 *  chiTCP - A simple, testable TCP stack
 *
 *  debug breakpoint insertion functions
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

#include "breakpoint.h"
#include <pthread.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <errno.h>
#include "protobuf-wrapper.h"
#include "chitcp/log.h"
#include "serverinfo.h"

/* Internal functions */
static void debug_mon_destroy(debug_monitor_t *debug_mon);
static void debug_mon_unlock(debug_monitor_t *debug_mon);
static void debug_mon_release(debug_monitor_t *debug_mon);
static void debug_mon_lock(debug_monitor_t *debug_mon, pthread_mutex_t *mutex_to_unlock);
static void debug_mon_remove_from_chisocket_table(serverinfo_t *si, debug_monitor_t *debug_mon);
static void attach_monitor_and_flags_to_entry(debug_monitor_t *debug_mon, int event_flags, chisocketentry_t *entry);
static void detach_monitor_from_entry(debug_monitor_t *debug_mon, chisocketentry_t *entry);
static int exchange_breakpoint_messages(debug_monitor_t *debug_mon, int sockfd, int event_flag, int new_sockfd, bool_t is_active);
static bool_t socket_monitors_event(chisocketentry_t *entry, int event_flag);
static debug_monitor_t *obtain_debug_mon(chisocketentry_t *entry, int event_flag);
static bool_t valid_parameters(serverinfo_t *si, int sockfd, int event_flag);
static void handle_special_breakpoint_responses(int *response, serverinfo_t *si, chisocketentry_t *entry, debug_monitor_t *debug_mon, int event_flag, int new_sockfd);


int chitcpd_init_debug_connection(serverinfo_t *si, int sockfd, int event_flags, int client_socket)
{
    debug_monitor_t *debug_mon;

    chilog(TRACE, ">>> Initializing debug connection");

    if(sockfd < 0 || sockfd >= si->chisocket_table_size || si->chisocket_table[sockfd].available)
    {
        chilog(ERROR, "Not a valid chisocket descriptor: %i", sockfd);
        return EBADF;
    }

    debug_mon = malloc(sizeof(debug_monitor_t));
    if (!debug_mon)
    {
        return errno;
    }
    pthread_mutex_init(&debug_mon->lock_numwaiters, NULL);
    debug_mon->numwaiters = 0;
    debug_mon->dying = FALSE;
    pthread_mutex_init(&debug_mon->lock_sockfd, NULL);
    debug_mon->sockfd = client_socket;
    debug_mon->ref_count = 1;

    chisocketentry_t *entry = &si->chisocket_table[sockfd];
    pthread_mutex_lock(&entry->lock_debug_monitor);
    if (entry->debug_monitor != NULL)
    {
        /* Some other thread is already registered as debugging this socket */
        chilog(TRACE, "Socket %d already has a debug monitor", sockfd);
        pthread_mutex_unlock(&entry->lock_debug_monitor);
        pthread_mutex_destroy(&debug_mon->lock_numwaiters);
        pthread_mutex_destroy(&debug_mon->lock_sockfd);
        free(debug_mon);
        return EAGAIN;
    }

    entry->event_flags = event_flags;
    entry->debug_monitor = debug_mon;
    pthread_mutex_unlock(&entry->lock_debug_monitor);

    chilog(DEBUG, "Created new debug monitor for socket %d", sockfd);
    chilog(TRACE, "<<< Finished initializing debug connection");

    return CHITCP_OK;
}


/* chitcpd_debug_breakpoint - If SOCKFD has a debug monitor which is watching
 *                  - events of type EVENT_FLAG, sends a debug event message
 *                  - to the corresponding client, and returns the client's
 *                  - response.
 *
 *  See breakpoint.h for more detailed documentation.
 *
 * Returns:
 *  DBG_RESP_NONE   - take no action (note: this is also returned if SOCKFD is invalid)
 *  value R > 0     - take action R (depending on the event)
 */
enum chitcpd_debug_response chitcpd_debug_breakpoint(serverinfo_t *si, int sockfd, int event_flag, int new_sockfd)
{
    if (!valid_parameters(si, sockfd, event_flag))
        return DBG_RESP_NONE;

    chisocketentry_t *entry = &si->chisocket_table[sockfd];
    debug_monitor_t *debug_mon = obtain_debug_mon(entry, event_flag);
    if (!debug_mon)
        /* The client doesn't care about this event. */
        return DBG_RESP_NONE;
    
    chilog(DEBUG, ">>> Reached a breakpoint: socket %d, event %s", sockfd,
           dbg_evt_str(event_flag));

    /* The breakpoint proper. */
    int response = exchange_breakpoint_messages(debug_mon, sockfd, event_flag, new_sockfd, entry->actpas_type == SOCKET_ACTIVE);
    
    /* Certain client responses must be handled BEFORE we return. */
    handle_special_breakpoint_responses(&response, si, entry, debug_mon, event_flag, new_sockfd);

    if (debug_mon->dying)
    {
        /* Since we were the first thread to discover that debug_mon is dying,
         * we initiate procedures for removing it. */
        chilog(DEBUG, "Debug monitor for socket %d is dying.", sockfd);
        debug_mon_remove_from_chisocket_table(si, debug_mon);
    }

    debug_mon_release(debug_mon);

    chilog (DEBUG, "<<< Exiting breakpoint");
    return response;
}

void chitcpd_debug_detach_monitor(chisocketentry_t *entry)
{
    debug_monitor_t *debug_mon = entry->debug_monitor;
    debug_mon_lock(debug_mon, NULL);
    detach_monitor_from_entry(debug_mon, entry);
    debug_mon_release(debug_mon);
}

/* valid_parameters -
 *  Does some sanity checking to make sure that
 *  the breakpoint should actually be handled.
 */
static bool_t valid_parameters(serverinfo_t *si, int sockfd, int event_flag)
{
    /* If the server is stopping, we don't process checkpoints. */
    if(si->state != CHITCPD_STATE_RUNNING)
    {
        chilog(DEBUG, "Ignoring breakpoint (socket %d, event %s): Server is stopping", sockfd,
               dbg_evt_str(event_flag));

        return FALSE;
    }

    if(sockfd < 0 || sockfd >= si->chisocket_table_size || si->chisocket_table[sockfd].available)
    {
        chilog(ERROR, "Not a valid chisocket descriptor: %i", sockfd);
        return FALSE;
    }

    return TRUE;
}

/* obtain_debug_mon -
 *  If entry is monitoring events of type event_flag, lock entry's
 *  debug_monitor and return it. Otherwise, return NULL.
 */
static debug_monitor_t *obtain_debug_mon(chisocketentry_t *entry, int event_flag)
{
    pthread_mutex_lock(&entry->lock_debug_monitor);

    if (!socket_monitors_event(entry, event_flag))
    {
        pthread_mutex_unlock(&entry->lock_debug_monitor);
        return NULL;
    }

    debug_monitor_t *debug_mon = entry->debug_monitor;
    debug_mon_lock(debug_mon, &entry->lock_debug_monitor);

    /* Perhaps this debug_mon is no longer being used with this socket. */
    if (debug_mon != entry->debug_monitor)
    {
        debug_mon_release(debug_mon);
        return NULL;
    }

    return debug_mon;
}

static bool_t socket_monitors_event(chisocketentry_t *entry, int event_flag)
{
    if (entry->debug_monitor && (event_flag & entry->event_flags))
        return TRUE;
    else
        return FALSE;
}

/* Ensure debug_mon is locked before calling. */
static int exchange_breakpoint_messages(debug_monitor_t *debug_mon, int sockfd, int event_flag, int new_sockfd, bool_t is_active)
{
    ChitcpdMsg req = CHITCPD_MSG__INIT;
    ChitcpdDebugEventArgs dea = CHITCPD_DEBUG_EVENT_ARGS__INIT;

    req.code = CHITCPD_MSG_CODE__DEBUG_EVENT;
    req.debug_event_args = &dea;

    dea.sockfd = sockfd;
    dea.event_flag = event_flag;
    dea.new_sockfd = new_sockfd;
    dea.is_active = is_active;

    ChitcpdMsg *resp;
    int r = chitcpd_send_and_recv_msg(debug_mon->sockfd, &req, &resp);

    if (r != CHITCP_OK)
    {
        debug_mon->dying = TRUE;
        return DBG_RESP_NONE;
    }
    else
    {
        assert(resp->resp != NULL);
        int ret = resp->resp->ret;
        chitcpd_msg__free_unpacked(resp, NULL);

        assert(ret >= 0);
        return ret;
    }
}

/* Ensure debug_mon is locked before calling. */
static void handle_special_breakpoint_responses(int *response, serverinfo_t *si, chisocketentry_t *entry, debug_monitor_t *debug_mon, int event_flag, int new_sockfd)
{
    if (*response == DBG_RESP_STOP)
    {
        *response = DBG_RESP_NONE;
        detach_monitor_from_entry(debug_mon, entry);
    }
    else if (*response == DBG_RESP_ACCEPT_MONITOR
                && event_flag == DBG_EVT_PENDING_CONNECTION)
    {
        /* The client might have improperly used the response code DBG_RESP_ACCEPT_MONITOR,
         * so we must double-check that the event_flag was DBG_EVT_PENDING_CONNECTION. */

        *response = DBG_RESP_NONE;
        chisocketentry_t *active_entry = &si->chisocket_table[new_sockfd];
        attach_monitor_and_flags_to_entry(debug_mon, entry->event_flags, active_entry);
        chilog(DEBUG, "Added debug monitor for new active socket %d", new_sockfd);
    }
}

/* Ensure debug_mon is locked before calling. */
static void detach_monitor_from_entry(debug_monitor_t *debug_mon, chisocketentry_t *entry)
{
    pthread_mutex_lock(&entry->lock_debug_monitor);
    if (entry->debug_monitor == debug_mon)
    {
        entry->debug_monitor = NULL;
        entry->event_flags = 0;

        if (--(debug_mon->ref_count) == 0)
            debug_mon->dying = TRUE;
    }
    pthread_mutex_unlock(&entry->lock_debug_monitor);
}
 
/* Ensure debug_mon is locked before calling. */
static void attach_monitor_and_flags_to_entry(debug_monitor_t *debug_mon, int event_flags, chisocketentry_t *entry)
{
    pthread_mutex_lock(&entry->lock_debug_monitor);
    entry->debug_monitor = debug_mon;
    entry->event_flags = event_flags;
    pthread_mutex_unlock(&entry->lock_debug_monitor);

    debug_mon->ref_count++;
}

/* Removes all pointers to debug_mon from si->chisocket_table. */
/* Ensure debug_mon is locked before calling. */
static void debug_mon_remove_from_chisocket_table(serverinfo_t *si, debug_monitor_t *debug_mon)
{
    for (int i=0; i < si->chisocket_table_size; i++)
    {
        chisocketentry_t *e = &si->chisocket_table[i];
        detach_monitor_from_entry(debug_mon, e);
    }
}

/* debug_mon_lock -
 *  Locking a debug monitor can take a long time. Therefore, this function
 *  allows the user to supply a locked mutex which will be unlocked
 *  AS SOON AS this thread is on the waiting list for this debug_monitor.
 *  (This allows other threads which need said mutex to make progress.)
 */
static void debug_mon_lock(debug_monitor_t *debug_mon, pthread_mutex_t *mutex_to_unlock)
{
    pthread_mutex_lock(&debug_mon->lock_numwaiters);
    debug_mon->numwaiters ++;
    pthread_mutex_unlock(&debug_mon->lock_numwaiters);
    if (mutex_to_unlock)
        pthread_mutex_unlock(mutex_to_unlock);

    pthread_mutex_lock(&debug_mon->lock_sockfd);
    pthread_mutex_lock(&debug_mon->lock_numwaiters);
    debug_mon->numwaiters --;
    pthread_mutex_unlock(&debug_mon->lock_numwaiters);
}

/* debug_mon_release -
 *  If debug_mon is dying and no other threads are waiting to use it,
 *  free the resources associated with debug_mon. Else, just unlock it.
 */
static void debug_mon_release(debug_monitor_t *debug_mon)
{
    if (debug_mon->dying && debug_mon->numwaiters == 0)
        debug_mon_destroy(debug_mon);
    else
        debug_mon_unlock(debug_mon);
}

static void debug_mon_unlock(debug_monitor_t *debug_mon)
{
    pthread_mutex_unlock(&debug_mon->lock_sockfd);
}

static void debug_mon_destroy(debug_monitor_t *debug_mon)
{
    close(debug_mon->sockfd);
    pthread_mutex_destroy(&debug_mon->lock_sockfd);
    pthread_mutex_destroy(&debug_mon->lock_numwaiters);
    free(debug_mon);
}

