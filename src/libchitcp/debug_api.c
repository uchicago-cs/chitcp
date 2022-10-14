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

#include "chitcp/debug_api.h"
#include "daemon_api.h"
#include "chitcp/types.h"
#include "chitcp/socket.h"
#include "chitcp/utlist.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

/* Used by active_threads to retain socket state information between calls
 * to a debug_event_handler. (See active_thread below.) */
static pthread_key_t state_key;

struct debug_thread_args
{
    debug_event_handler handler;
    int fd;
};

/* Arguments to a thread that monitors debug events from a single active socket. */
struct active_thread_args
{
    debug_event_handler handler; /* function to run upon receipt of a debug event */
    int sockfd; /* chisocket being monitored */
    enum chitcpd_debug_event event_flag; /* the current event */
    enum chitcpd_debug_response response; /* the thread's response to the event */
    pthread_mutex_t lock;
    pthread_cond_t cv;
    pthread_t tid;

    /* List pointers */
    struct active_thread_args *prev;
    struct active_thread_args *next;
};

/* For more descriptive log messages. */
static char *event_names[] =
{
    "TCP_STATE_CHANGE",
    "INCOMING_PACKET",
    "OUTGOING_PACKET",
    "PENDING_CONNECTION",
    "KILL"
};

char *dbg_evt_str(enum chitcpd_debug_event evt)
{
    int i = 0;
    while ((evt >>= 1) > 0)
        i++;
    return event_names[i];
}

void dump_socket_state(struct debug_socket_state *state, bool_t include_buffers)
{
    printf("BEGIN Dumping socket state.\n");
    printf("  tcp_state: %s\n", tcp_str(state->tcp_state));
    printf("  ISS: %u\n", state->ISS);
    printf("  IRS: %u\n", state->IRS);
    printf("  SND_UNA: %u\n", state->SND_UNA);
    printf("  RCV_NXT: %u\n", state->RCV_NXT);
    printf("  SND_NXT: %u\n", state->SND_NXT);
    printf("  RCV_WND: %u\n", state->RCV_WND);
    printf("  SND_WND: %u\n", state->SND_WND);

    if (include_buffers && state->send && state->recv)
    {
        int i;
        printf("  send buffer: ");
        for (i = 0; i < state->send_len; i++)
        {
            printf("%02x ", state->send[i]);
        }
        printf("\n");
        printf("  recv buffer: ");
        for (i = 0; i < state->recv_len; i++)
        {
            printf("%02x ", state->recv[i]);
        }
        printf("\n");
    }

    printf("END Dumping socket state.\n");
}

static void *debug_thread(void *args);

static pthread_once_t state_key_init = PTHREAD_ONCE_INIT;
static void state_key_destructor(void *mem)
{
    free(mem);
}
static void create_state_key()
{
    pthread_key_create(&state_key, state_key_destructor);
}

int chitcpd_debug(int sockfd, int event_flags, debug_event_handler handler)
{
    int rc;
    int daemon_fd;
    ChitcpdMsg msg = CHITCPD_MSG__INIT;
    ChitcpdInitArgs ia = CHITCPD_INIT_ARGS__INIT;
    ChitcpdDebugArgs da = CHITCPD_DEBUG_ARGS__INIT;
    ChitcpdMsg *resp_p;
    struct debug_thread_args *dbt_args;
    pthread_t tid;

    pthread_once(&state_key_init, create_state_key);

    /* Create a new connection to the daemon */
    daemon_fd = chitcpd_connect();
    if (daemon_fd == CHITCP_ESOCKET)
    {
        /* errno is set during chitcpd_connect */
        return -1;
    }

    /* Let the daemon know this is a debug connection */
    msg.code = CHITCPD_MSG_CODE__INIT;
    msg.init_args = &ia;
    msg.init_args->connection_type = CHITCPD_CONNECTION_TYPE__DEBUG_CONNECTION;
    msg.init_args->debug = &da;

    da.sockfd = sockfd;
    da.event_flags = event_flags;

    rc = chitcpd_send_command(daemon_fd, &msg, &resp_p);
    if (rc < 0)
        return -1;

    /* Unpack response */
    assert(resp_p->resp != NULL);
    rc = resp_p->resp->ret;
    errno = resp_p->resp->error_code;
    chitcpd_msg__free_unpacked(resp_p, NULL);

    if (rc < 0)
    {
        perror("chitcpd_debug");
        close(sockfd);
        return -1;
    }

    dbt_args = malloc(sizeof(struct debug_thread_args));
    if (dbt_args == NULL)
    {
        /* malloc sets errno */
        close(sockfd);
        return -1;
    }
    dbt_args->fd = daemon_fd;
    dbt_args->handler = handler;

    if ((rc = pthread_create(&tid, NULL, debug_thread, (void *) dbt_args)) != 0)
    {
        errno = rc;
        perror("pthread_create");
        close(sockfd);
        return -1;
    }

    pthread_detach(tid);

    return CHITCP_OK;
}

/* See chitcp/debug_api.h. */
int chitcpd_debug_save_socket_state(debug_socket_state_t *state_info)
{
    debug_socket_state_t *old_state_info =
        (debug_socket_state_t *) pthread_getspecific(state_key);
    if (old_state_info != NULL)
    {
        free(old_state_info->recv);
        free(old_state_info->send);
        free(old_state_info);
    }

    return pthread_setspecific(state_key, state_info);
}

/* Declarations for use in debug_thread() below */
static enum chitcpd_debug_response send_and_get_from_active(struct active_thread_args *item, enum chitcpd_debug_event event_flag);
static void *active_thread(void *_args);

static void *debug_thread(void *_args)
{
    int rc;
    int sockfd, event_flag, new_sockfd; /* arguments for the event handler */
    bool_t is_active;
    struct debug_thread_args *args = (struct debug_thread_args *) _args;
    int daemon_fd = args->fd;
    debug_event_handler handler = args->handler;
    free(_args);
    ChitcpdMsg *event_msg;
    ChitcpdMsg resp_outer = CHITCPD_MSG__INIT;
    ChitcpdResp resp_inner = CHITCPD_RESP__INIT;
    bool_t passive = FALSE; /* whether the associated socket is passive */
    bool_t first_event = TRUE; /* whether this is the first event we've received */

    resp_outer.code = CHITCPD_MSG_CODE__RESP;
    resp_outer.resp = &resp_inner;

    struct active_thread_args *active_list = NULL;

    while (1)
    {
        rc = chitcpd_recv_msg(daemon_fd, &event_msg);
        if (rc < 0)
            break;

        assert(event_msg->debug_event_args != NULL);
        sockfd = event_msg->debug_event_args->sockfd;
        event_flag = event_msg->debug_event_args->event_flag;
        new_sockfd = event_msg->debug_event_args->new_sockfd;
        is_active = event_msg->debug_event_args->is_active;

        chitcpd_msg__free_unpacked(event_msg, NULL);

        if ( (!is_active && event_flag == DBG_EVT_PENDING_CONNECTION) || (is_active && first_event) )
        {
            /* If we're dealing with an active socket, we need to create an
             * active_thread upon the first debug event. In the passive case,
             * we may need to create a new active_thread, depending on what
             * the handler tells us to do. */

            first_event = FALSE;
            bool_t create_active_thread = TRUE;
            if (event_flag == DBG_EVT_PENDING_CONNECTION)
            {
                passive = TRUE;
                resp_inner.ret = handler(sockfd, event_flag, NULL, NULL, new_sockfd);
                if (resp_inner.ret == DBG_RESP_STOP)
                    break;
                if (resp_inner.ret != DBG_RESP_ACCEPT_MONITOR)
                    create_active_thread = FALSE;
            }

            if (create_active_thread)
            {
                struct active_thread_args *new_args =
                    malloc(sizeof(struct active_thread_args));
                if (new_args == NULL)
                {
                    fprintf(stderr, "debug_thread: Fatal error: no memory available to create new active_thread\n");
                    break;
                }

                pthread_cond_init(&new_args->cv, NULL);
                pthread_mutex_init(&new_args->lock, NULL);
                new_args->sockfd = passive ? new_sockfd : sockfd;
                new_args->handler = handler;

                /* The following two values are chosen because they do not
                 * represent an actual event or response. This is important
                 * for communication between this thread and the child. */
                new_args->event_flag = 0;
                new_args->response = -1;

                rc = pthread_create(&new_args->tid, NULL, active_thread, (void *) new_args);
                if (rc < 0)
                {
                    fprintf(stderr, "debug_thread: Fatal error: insufficient resources to create new active_thread\n");
                    pthread_mutex_destroy(&new_args->lock);
                    pthread_cond_destroy(&new_args->cv);
                    free(new_args);
                    break;
                }

                DL_APPEND(active_list, new_args);
            }
        }

        if (!is_active)
        {
            resp_inner.ret = handler(sockfd, event_flag, NULL, NULL, -1);
            if (resp_inner.ret == DBG_RESP_STOP)
                break;
        }
        else if (is_active)
        {
            /* We must pass this event on to the associated active_thread. */
            struct active_thread_args *item;
            DL_SEARCH_SCALAR(active_list, item, sockfd, sockfd);

            if (item != NULL)
            {
                resp_inner.ret = send_and_get_from_active(item, event_flag);
                if (resp_inner.ret == DBG_RESP_STOP)
                {
                    /* The other thread is terminating */
                    DL_DELETE(active_list, item);
                    pthread_join(item->tid, NULL);
                    pthread_mutex_destroy(&item->lock);
                    pthread_cond_destroy(&item->cv);
                    free(item);

                    /* If not passive, we will never get any new connections,
                     * so we need to terminate this thread. */
                    if (!passive)
                        break;
                }
            }
            else
            {
                fprintf(stderr, "debug_thread: Error: received event %s for unknown socket %d\n",
                        dbg_evt_str(event_flag), sockfd);
            }
        }

        rc = chitcpd_send_msg(daemon_fd, &resp_outer);
        if (rc < 0)
            break;
    }

    /* Free the resources in the active_list. */
    struct active_thread_args *item, *tmp;
    DL_FOREACH_SAFE(active_list, item, tmp)
    {
        /* Ask the corresponding thread to stop. */
        send_and_get_from_active(item, DBG_EVT_KILL);
        pthread_join(item->tid, NULL);
        pthread_mutex_destroy(&item->lock);
        pthread_cond_destroy(&item->cv);
        DL_DELETE(active_list, item);
        free(item);
    }

    close(daemon_fd);

    return NULL;
}

/* Send EVENT_FLAG to the thread associated with ITEM, and get response from same. */
static enum chitcpd_debug_response send_and_get_from_active(struct active_thread_args *item, enum chitcpd_debug_event event_flag)
{
    enum chitcpd_debug_response response;

    pthread_mutex_lock(&item->lock);
    item->event_flag = event_flag;
    pthread_cond_broadcast(&item->cv);

    /* Wait for the thread to respond */
    while (item->response == -1)
        pthread_cond_wait(&item->cv, &item->lock);

    response = item->response;
    item->response = -1;
    pthread_mutex_unlock(&item->lock);

    return response;
}

/* Thread function for handlers of active sockets created by passive sockets */
static void *active_thread(void *_args)
{
    struct active_thread_args *args = (struct active_thread_args *) _args;
    int done = FALSE;

    while (1)
    {
        pthread_mutex_lock(&args->lock);
        while (args->event_flag == 0)
            pthread_cond_wait(&args->cv, &args->lock);
        if (args->event_flag == DBG_EVT_KILL)
        {
            done = TRUE;
            args->response = DBG_RESP_NONE; /* any response but -1 */
        }
        else
        {
            /* Retrieve state information to send to the handler. */
            debug_socket_state_t *state_info, *saved_state_info;
            state_info = chitcpd_get_socket_state(args->sockfd, TRUE);
            if (state_info == NULL)
            {
                fprintf(stderr, "active_thread: could not get state_info!\nABORTING\n");
                args->response = DBG_RESP_STOP;
            }
            else
            {
                saved_state_info = (debug_socket_state_t *) pthread_getspecific(state_key);

                /* Call the handler. */
                args->response = args->handler(args->sockfd, args->event_flag,
                        state_info, saved_state_info, -1);

                /* Free state_info, unless the handler saved it to state_key. */
                if (pthread_getspecific(state_key) != state_info)
                {
                    free(state_info->send);
                    free(state_info->recv);
                    free(state_info);
                }
            }
        }
        args->event_flag = 0;

        if (args->response == DBG_RESP_STOP)
            done = TRUE;
        pthread_cond_broadcast(&args->cv);
        pthread_mutex_unlock(&args->lock);

        if (done)
        {
            /* Free the saved state. */
            chitcpd_debug_save_socket_state(NULL);

            pthread_exit(NULL);
        }
    }
}

debug_socket_state_t *chitcpd_get_socket_state(int sockfd, bool_t include_buffers)
{
    ChitcpdMsg req = CHITCPD_MSG__INIT;
    ChitcpdGetSocketStateArgs gssa = CHITCPD_GET_SOCKET_STATE_ARGS__INIT;
    ChitcpdMsg *resp_p;
    debug_socket_state_t *ret;
    int rc;

    int daemon_socket = chitcpd_get_socket();

    ret = malloc(sizeof(*ret));
    if (ret == NULL)
    {
        return NULL;
    }
    ret->send = NULL;
    ret->recv = NULL;

    /* Create request */
    req.code = CHITCPD_MSG_CODE__GET_SOCKET_STATE;
    req.get_socket_state_args = &gssa;

    gssa.sockfd = sockfd;

    rc = chitcpd_send_command(daemon_socket, &req, &resp_p);

    if (rc != CHITCP_OK)
    {
        perror("chitcpd_get_socket_state: Error when sending command to chiTCP daemon.\n");
        free(ret);
        return NULL;
    }

    /* Unpack response */
    assert(resp_p->resp != NULL);
    if (resp_p->resp->ret != CHITCP_OK)
    {
        errno = resp_p->resp->error_code;
        perror("chitcpd_get_socket_state: Could not get socket state from daemon");
        free(ret);
        return NULL;
    }

    assert(resp_p->resp->socket_state != NULL);
    ret->tcp_state = resp_p->resp->socket_state->tcp_state;
    ret->ISS = resp_p->resp->socket_state->iss;
    ret->IRS = resp_p->resp->socket_state->irs;
    ret->SND_UNA = resp_p->resp->socket_state->snd_una;
    ret->RCV_NXT = resp_p->resp->socket_state->rcv_nxt;
    ret->SND_NXT = resp_p->resp->socket_state->snd_nxt;
    ret->RCV_WND = resp_p->resp->socket_state->rcv_wnd;
    ret->SND_WND = resp_p->resp->socket_state->snd_wnd;

    chitcpd_msg__free_unpacked(resp_p, NULL);
    if (include_buffers)
    {
        /* Send the command to get the buffer contents */

        ChitcpdGetSocketBufferContentsArgs gsbca = CHITCPD_GET_SOCKET_BUFFER_CONTENTS_ARGS__INIT;
        gsbca.sockfd = sockfd;

        req.code = CHITCPD_MSG_CODE__GET_SOCKET_BUFFER_CONTENTS;
        req.get_socket_state_args = NULL;
        req.get_socket_buffer_contents_args = &gsbca;

        rc = chitcpd_send_command(daemon_socket, &req, &resp_p);

        if (rc != CHITCP_OK)
        {
            perror("chitcpd_get_socket_state: Error when sending command to chiTCP daemon");
            free(ret);
            return NULL;
        }

        /* Unpack response */
        assert(resp_p->resp != NULL);
        if (resp_p->resp->ret != CHITCP_OK)
        {
            errno = resp_p->resp->error_code;
            perror("chitcpd_get_socket_state: Could not get socket buffers from daemon");
            free(ret);
            return NULL;
        }

        assert(resp_p->resp->socket_buffer_contents != NULL);
        ret->recv_len = resp_p->resp->socket_buffer_contents->rcv.len;
        ret->send_len = resp_p->resp->socket_buffer_contents->snd.len;
        ret->recv = malloc(ret->recv_len);
        if (!ret->recv)
        {
            free(ret);
            return NULL;
        }
        ret->send = malloc(ret->send_len);
        if (!ret->send)
        {
            free(ret);
            return NULL;
        }
        memcpy(ret->recv, resp_p->resp->socket_buffer_contents->rcv.data, ret->recv_len);
        memcpy(ret->send, resp_p->resp->socket_buffer_contents->snd.data, ret->send_len);

        chitcpd_msg__free_unpacked(resp_p, NULL);
    }
    return ret;
}


int chitcpd_wait_for_state(int sockfd, tcp_state_t tcp_state)
{
    ChitcpdMsg req = CHITCPD_MSG__INIT;
    ChitcpdWaitForStateArgs wfsa = CHITCPD_WAIT_FOR_STATE_ARGS__INIT;
    ChitcpdMsg *resp_p;
    int ret, error_code;
    int rc;

    int daemon_socket = chitcpd_get_socket();
    if (daemon_socket < 0)
        CHITCPD_FAIL("Error when connecting to chiTCP daemon.")

    /* Create request */
    req.code = CHITCPD_MSG_CODE__WAIT_FOR_STATE;
    req.wait_for_state_args = &wfsa;

    wfsa.sockfd = sockfd;
    wfsa.tcp_state = tcp_state;

    rc = chitcpd_send_command(daemon_socket, &req, &resp_p);

    if(rc != CHITCP_OK)
        CHITCPD_FAIL("Error when communicating with chiTCP daemon.");

    /* Unpack response */
    assert(resp_p->resp != NULL);
    ret = resp_p->resp->ret;
    error_code = resp_p->resp->error_code;

    chitcpd_msg__free_unpacked(resp_p, NULL);

    ret = (error_code? -1 : ret);
    if(error_code) errno = error_code;

    return ret;
}
