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
#include "protobuf-wrapper.h"
#include "chitcp/log.h"
#include "serverinfo.h"

/* chitcpd_debug_breakpoint - If SOCKFD has a debug monitor which is watching
 *                  - events of type EVENT_FLAG, sends a debug event message
 *                  - to the corresponding client, and returns the client's
 *                  - response.
 *
 * Returns:
 *  DBG_RESP_NONE   - take no action (note: this is also returned if SOCKFD is invalid)
 *  value R > 0     - take action R (depending on the event)
 */
enum chitcpd_debug_response chitcpd_debug_breakpoint(serverinfo_t *si, int sockfd, int event_flag, int new_sockfd)
{
    chisocketentry_t *entry;
    debug_monitor_t *debug_mon;
    int numwaiters;
    int r; /* return code */
    ChitcpdMsg req = CHITCPD_MSG__INIT;
    ChitcpdDebugEventArgs dea = CHITCPD_DEBUG_EVENT_ARGS__INIT;
    ChitcpdMsg *resp;

    /* If the server is stopping, we don't process checkpoints. */
    if(si->state != CHITCPD_STATE_RUNNING)
    {
        chilog(DEBUG, "Ignoring breakpoint (socket %d, event %s): Server is stopping", sockfd,
               dbg_evt_str(event_flag));

        return DBG_RESP_NONE;
    }

    if(sockfd < 0 || sockfd >= si->chisocket_table_size || si->chisocket_table[sockfd].available)
    {
        chilog(ERROR, "Not a valid chisocket descriptor: %i", sockfd);
        return DBG_RESP_NONE;
    }

    entry = &si->chisocket_table[sockfd];

    /* Complicated synchronization is necessary to ensure that the debug
     * monitor is not destroyed while other threads are waiting to write
     * data to the client socket. */

    pthread_mutex_lock(&entry->lock_debug_monitor);
    debug_mon = entry->debug_monitor;
    if (!debug_mon || !(event_flag & entry->event_flags))
    {
        /* We don't care about this event */
        pthread_mutex_unlock(&entry->lock_debug_monitor);
        return DBG_RESP_NONE;
    }

    chilog(DEBUG, ">>> Reached a breakpoint: socket %d, event %s", sockfd,
           dbg_evt_str(event_flag));

    pthread_mutex_lock(&debug_mon->lock_numwaiters);
    debug_mon->numwaiters ++;
    pthread_mutex_unlock(&debug_mon->lock_numwaiters);
    pthread_mutex_unlock(&entry->lock_debug_monitor);

    pthread_mutex_lock(&debug_mon->lock_sockfd);
    pthread_mutex_lock(&debug_mon->lock_numwaiters);
    numwaiters = (debug_mon->numwaiters --);
    pthread_mutex_unlock(&debug_mon->lock_numwaiters);

    if (debug_mon->dying)
    {
        /* Abort send, and destroy the debug monitor if no other threads
         * are waiting on lock_sockfd. */
        pthread_mutex_unlock(&debug_mon->lock_sockfd);
        if (numwaiters == 0)
        {
            pthread_mutex_destroy(&debug_mon->lock_sockfd);
            pthread_mutex_destroy(&debug_mon->lock_numwaiters);
            free(debug_mon);
        }
        chilog (DEBUG, "<<< Exiting breakpoint");
        return DBG_RESP_NONE;
    }

    /* Send the message. */

    req.code = CHITCPD_MSG_CODE__DEBUG_EVENT;
    req.debug_event_args = &dea;

    dea.sockfd = sockfd;
    dea.event_flag = event_flag;
    dea.new_sockfd = new_sockfd;

    bool_t stop_debugging = FALSE;
    int ret;
    r = chitcpd_send_and_recv_msg(debug_mon->sockfd, &req, &resp);

    if (r != CHITCP_OK)
    {
        stop_debugging = TRUE;
        ret = DBG_RESP_NONE;
    }
    else
    {
        /* Unpack response */

        assert(resp->resp != NULL);
        ret = resp->resp->ret;
        chitcpd_msg__free_unpacked(resp, NULL);

        assert(ret >= 0);
        if (ret == DBG_RESP_STOP)
        {
            stop_debugging = TRUE;
            ret = DBG_RESP_NONE;
        }
        else
            pthread_mutex_unlock(&debug_mon->lock_sockfd);
    }

    if (stop_debugging)
    {
        /* This debug connection is no longer valid. We must carefully
         * pass this information to all other threads which might try
         * to write to this socket. */

        chilog(DEBUG, "Debug monitor for socket %d is no longer valid.", sockfd);

        debug_mon->dying = TRUE;

        /* Remove this debug monitor from the socket table */
        for (int i=0; i < si->chisocket_table_size; i++)
        {
            chisocketentry_t *e = &si->chisocket_table[i];
            if (e->available) continue;

            pthread_mutex_lock(&e->lock_debug_monitor);
            if (e->debug_monitor == debug_mon)
            {
                e->debug_monitor = NULL;
                e->event_flags = 0;
            }
            pthread_mutex_unlock(&e->lock_debug_monitor);
        }

        numwaiters = debug_mon->numwaiters;
        pthread_mutex_unlock(&debug_mon->lock_sockfd);

        /* Destroy the debug monitor if no other threads are waiting for it */
        if (numwaiters == 0)
        {
            pthread_mutex_destroy(&debug_mon->lock_sockfd);
            pthread_mutex_destroy(&debug_mon->lock_numwaiters);
            free(debug_mon);
        }
    }

    chilog (DEBUG, "<<< Exiting breakpoint");
    return ret;
}

