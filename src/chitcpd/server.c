/*
 *  chiTCP - A simple, testable TCP stack
 *
 *  The functions in this file set up and start the threads
 *  of the chiTCP daemon.
 *
 *  More specifically, the daemon spawns two threads:
 *
 *  THE SERVER THREAD: This thread listens on a UNIX socket (by default,
 *  /tmp/chitcpd.socket). Local applications will connect to this socket
 *  to send requests to chitcpd (for example, to create a new chisocket).
 *  The server thread is in charge of handling these requests, and it
 *  spawns a new HANDLER THREAD for each connection on the UNIX socket.
 *
 *  The code for the handler thread is contained in handlers.c
 *
 *  THE NETWORK THREAD: This thread listens on a TCP socket (by default,
 *  23300). chiTCP daemons on different hosts communicate via TCP.
 *  For example, a chiTCP daemon on one host that needs to deliver a
 *  chiTCP packet to another host will do so via this TCP connection.
 *  The network thread is in charge of handling these connections, and
 *  spawns a new CONNECTION THREAD for each connection on this TCP socket.
 *
 *  The code for the connection thread is contained in connection.c
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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "server.h"
#include "connection.h"
#include "handlers.h"
#include "breakpoint.h"
#include "protobuf-wrapper.h"
#include "chitcp/chitcpd.h"
#include "chitcp/log.h"
#include "chitcp/addr.h"

/* Server thread and network thread functions.
 * We declare them in advance because chitcpd_server_init will
 * be calling them */
int chitcpd_server_start_thread(serverinfo_t *si);
void* chitcpd_server_thread_func(void *args);
int chitcpd_server_start_network_thread(serverinfo_t *si);
void* chitcpd_server_network_thread_func(void *args);

/* Arguments to the server thread */
typedef struct server_thread_args
{
    serverinfo_t *si;
} server_thread_args_t;

/* Arguments to the network thread */
typedef struct network_thread_args
{
    serverinfo_t *si;
} network_thread_args_t;


/*
 * chitcpd_server_init - Starts the chiTCP daemon
 *
 * si: Server info
 *
 * Returns:
 *  - CHITCP_OK: Daemon (including server thread and network thread)
 *               has started correctly.
 *  - CHITCP_ENOMEM: Could not allocate memory for daemon
 *  - CHITCP_ESOCKET: Could not create a socket needed by daemon
 *  - CHITCP_ETHREAD: Could not create a thread needed by daemon
 *
 */
int chitcpd_server_init(serverinfo_t *si)
{
    /* TODO: Make this configurable */
    si->chisocket_table_size = DEFAULT_MAX_SOCKETS;
    si->port_table_size = DEFAULT_MAX_PORTS;
    si->connection_table_size = DEFAULT_MAX_CONNECTIONS;
    si->ephemeral_port_start = DEFAULT_EPHEMERAL_PORT_START;

    /* Initialize chisocket table */
    pthread_mutex_init(&si->lock_chisocket_table, NULL);
    si->chisocket_table = calloc(si->chisocket_table_size, sizeof(chisocketentry_t));

    if(si->chisocket_table == NULL)
    {
        perror("Could not initialize chisocket table");
        return CHITCP_ENOMEM;
    }

    for(int i=0; i< si->chisocket_table_size; i++)
    {
        pthread_mutex_init(&si->chisocket_table[i].lock_debug_monitor, NULL);
        si->chisocket_table[i].available = TRUE;
    }


    /* Initialize connection table */
    pthread_mutex_init(&si->lock_connection_table, NULL);
    si->connection_table = calloc(si->connection_table_size, sizeof(tcpconnentry_t));

    if(si->connection_table == NULL)
    {
        perror("Could not initialize connection table");
        return CHITCP_ENOMEM;
    }

    for(int i=0; i< si->connection_table_size; i++)
        si->connection_table[i].available = TRUE;

    /* Initialize port table */
    /* This is an array of pointers, and they are all set to NULL */
    si->port_table = calloc(si->port_table_size, sizeof(chisocketentry_t*));

    if(si->port_table == NULL)
    {
        perror("Could not initialize port table");
        return CHITCP_ENOMEM;
    }

    pthread_mutex_init(&si->lock_state, NULL);
    pthread_cond_init(&si->cv_state, NULL);

    si->state = CHITCPD_STATE_READY;

    return CHITCP_OK;
}

int chitcpd_server_start(serverinfo_t *si)
{
    int rc;

    pthread_mutex_lock(&si->lock_state);
    si->state = CHITCPD_STATE_STARTING;
    pthread_cond_broadcast(&si->cv_state);
    pthread_mutex_unlock(&si->lock_state);

    /* Start server thread */
    rc = chitcpd_server_start_thread(si);
    if(rc != 0)
    {
        return rc;
    }

    /* Start network thread */
    rc = chitcpd_server_start_network_thread(si);
    if(rc != 0)
    {
        return rc;
    }

    /* Note that, even though the above two functions will each spawn a
     * thread, we not need to wait for any "I'm done setting up" signal
     * from those threads, since the above two functions already put
     * the daemon's UNIX and TCP sockets in a listening state so, even
     * though we may not have reached the accept() call on those sockets
     * by the time we change the state to CHITCPD_STATE_RUNNING, the daemon
     * is effectively ready to receive connections (they will just be queued
     * until accept() is called).
     *
     * TODO: Things could go wrong before accept() is called, so it would
     *       be worth adding some synchronization here just in case.
     */

    pthread_mutex_lock(&si->lock_state);
    si->state = CHITCPD_STATE_RUNNING;
    pthread_cond_broadcast(&si->cv_state);
    pthread_mutex_unlock(&si->lock_state);

    return CHITCP_OK;
}

/*
 * chitcpd_server_wait - Wait for chiTCP daemon to exit
 *
 * si: Server info
 *
 * Returns:
 *  - CHITCP_OK: chiTCP daemon has exited correctly
 *
 */
int chitcpd_server_wait(serverinfo_t *si)
{
    chilog(DEBUG, "Waiting for chiTCP daemon to stop.");

    /* TODO: Retrieve return values */
    pthread_join(si->server_thread, NULL);
    pthread_join(si->network_thread, NULL);

    pthread_mutex_lock(&si->lock_state);
    si->state = CHITCPD_STATE_STOPPED;
    pthread_cond_broadcast(&si->cv_state);
    pthread_mutex_unlock(&si->lock_state);

    chilog(DEBUG, "chiTCP daemon has fully stopped.");

    return CHITCP_OK;
}


int chitcpd_server_stop(serverinfo_t *si)
{
    int rc;

    chilog(DEBUG, "Stopping the chiTCP daemon.");

    /* To stop the server, all we need to do is shutdown the server's
     * TCP and UNIX sockets. Once the server is running, its two
     * threads (server and network) are blocking on accept() on
     * each of these two sockets. Shutting down the sockets forces
     * a return from those calls which, combined with the change
     * in the state of the server, will result in an orderly shutdown. */

    pthread_mutex_lock(&si->lock_state);
    si->state = CHITCPD_STATE_STOPPING;
    pthread_cond_broadcast(&si->cv_state);
    pthread_mutex_unlock(&si->lock_state);

    rc = shutdown(si->network_socket, SHUT_RDWR);
    if(rc != 0)
        return CHITCP_ESOCKET;

    rc = shutdown(si->server_socket, SHUT_RDWR);
    if(rc != 0)
        return CHITCP_ESOCKET;

    chilog(DEBUG, "chiTCP daemon is now in STOPPING state.");

    return CHITCP_OK;
}

int chitcpd_server_free(serverinfo_t *si)
{
    free(si->chisocket_table);
    free(si->connection_table);
    free(si->port_table);

    pthread_mutex_destroy(&si->lock_state);
    pthread_cond_destroy(&si->cv_state);

    return CHITCP_OK;
}

/*
 * chitcpd_server_start_thread - Starts the server thread
 *
 * si: Server info
 *
 * Returns:
 *  - CHITCP_OK: The thread has started correctly
 *  - CHITCP_ESOCKET: Could not create socket
 *  - CHITCP_ETHREAD: Could not create thread
 *
 */
int chitcpd_server_start_thread(serverinfo_t *si)
{
    int len;

    struct sockaddr_un server_addr;

    if ((si->server_socket = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        perror("Could not open UNIX socket");
        return CHITCP_ESOCKET;
    }

    /* Create server address */
    server_addr.sun_family = AF_UNIX;
    strcpy(server_addr.sun_path, si->server_socket_path);
    len = sizeof(struct sockaddr_un);

    /* Unlink socket. TODO: Check whether it already exists. */
    unlink(server_addr.sun_path);

    /* Bind socket */
    if (bind(si->server_socket, (struct sockaddr *)&server_addr, len) == -1)
    {
        perror("Could not bind to UNIX socket");
        close(si->server_socket);
        return CHITCP_ESOCKET;
    }

    /* Start listening */
    if (listen(si->server_socket, 5) == -1)
    {
        perror("UNIX socket listen() failed");
        close(si->server_socket);
        return CHITCP_ESOCKET;
    }

    /* Create server thread */
    server_thread_args_t *sta = malloc(sizeof(server_thread_args_t));
    sta->si = si;

    if (pthread_create(&si->server_thread, NULL, chitcpd_server_thread_func, sta) < 0)
    {
        perror("Could not create server thread");
        free(sta);
        return CHITCP_ETHREAD;
    }

    return CHITCP_OK;
}


/* Used only in the following function to keep track of
 * the UNIX socket handler threads. */
typedef struct handler_thread
{
    pthread_t thread;
    socket_t handler_socket;
    pthread_mutex_t handler_lock;
} handler_thread_t;

/*
 * chitcpd_server_thread_func - Server thread function
 *
 * This function will spawn a handler thread (see handler.c) for each
 * new connection on the UNIX socket.
 *
 * args: arguments (a serverinfo_t variable in server_threads_args_t)
 *
 * Returns: Nothing.
 *
 */
void* chitcpd_server_thread_func(void *args)
{
    socklen_t sunSize;
    handler_thread_t *handler_thread;
    server_thread_args_t *sta;
    serverinfo_t *si;
    handler_thread_args_t *ha;
    list_t handler_thread_list;
    int rc;
    ChitcpdMsg *req;
    ChitcpdInitArgs *init_args;
    ChitcpdConnectionType conntype;
    ChitcpdMsg resp_outer = CHITCPD_MSG__INIT;
    ChitcpdResp resp_inner = CHITCPD_RESP__INIT;

    resp_outer.code = CHITCPD_MSG_CODE__RESP;
    resp_outer.resp = &resp_inner;

    /* For naming the handler threads we create (for debugging/logging) */
    int next_thread_id = 0;
    pthread_setname_np(pthread_self(), "unix_server");

    /* Unpack arguments */
    sta = (server_thread_args_t *) args;
    si = sta->si;

    list_init(&handler_thread_list);

    struct sockaddr_un client_addr;

    /* Accept connections on the UNIX socket */
    for(;;)
    {
        socket_t client_socket;

        /* Accept a connection */
        sunSize = sizeof(client_addr);
        if ((client_socket = accept(si->server_socket, (struct sockaddr *)&client_addr, &sunSize)) == -1)
        {
            /* If accept() returns in the CHITCPD_STATE_STOPPING, we don't
             * care what the error is. We just break out of the loop and
             * initiate an orderly shutdown. */
            if(si->state == CHITCPD_STATE_STOPPING)
                break;

            /* If this particular connection fails, no need to kill the entire thread. */
            perror("Could not accept() connection on UNIX socket");
            continue;
        }

        /* We receive a single message, which has to be an INIT message */
        rc = chitcpd_recv_msg(client_socket, &req);
        if (rc < 0)
        {
            if(si->state == CHITCPD_STATE_STOPPING)
                break;
            else
            {
                chilog(ERROR, "Error when receiving lead message through UNIX socket");
                shutdown(client_socket, SHUT_RDWR);
                continue;
            }
        }

        if(req->code != CHITCPD_MSG_CODE__INIT)
        {
            chilog(ERROR, "Expected INIT message, instead got message code %i", req->code);
            chitcpd_msg__free_unpacked(req, NULL);
            shutdown(client_socket, SHUT_RDWR);
            continue;
        }

        /* Unpack INIT request */
        assert(req->init_args != NULL);
        init_args = req->init_args;

        conntype = init_args->connection_type;


        /* There are two types of connections: command connections and debug
         * connections.
         *
         * When a command connection is created, a new thread is created to
         * handle the incoming chisocket commands on that connection (socket,
         * send, recv, etc.)
         *
         * When a debug connection is created, no additional thread is necessary.
         * The connection on the UNIX socket is simply "handed off" to a
         * debug monitor that will be associated with a chiTCP socket.
         * That UNIX socket is then used to send back debug messages.
         */

        if (conntype == CHITCPD_CONNECTION_TYPE__COMMAND_CONNECTION)
        {
            /* Create arguments for handler thread */
            ha = malloc(sizeof(handler_thread_args_t));
            ha->si = si;

            handler_thread = malloc(sizeof(handler_thread_t));
            handler_thread->handler_socket = client_socket;
            pthread_mutex_init(&handler_thread->handler_lock, NULL);

            /* Create handler thread to handle this connection */
            ha->client_socket = handler_thread->handler_socket;
            ha->handler_lock = &handler_thread->handler_lock;
            snprintf(ha->thread_name, 16, "handler-%d", next_thread_id++);
            if (pthread_create(&handler_thread->thread, NULL, chitcpd_handler_dispatch, ha) != 0)
            {
                perror("Could not create a worker thread");

                resp_outer.resp->ret = CHITCP_ETHREAD;
                resp_outer.resp->error_code = 0;
                rc = chitcpd_send_msg(client_socket, &resp_outer);

                free(ha);
                close(ha->client_socket);
                close(si->server_socket);
                // TODO: Perform an orderly shutdown instead of exiting
                pthread_exit(NULL);
            }
            resp_outer.resp->ret = CHITCP_OK;
            resp_outer.resp->error_code = 0;
            rc = chitcpd_send_msg(client_socket, &resp_outer);

            list_append(&handler_thread_list, handler_thread);
        }
        else if(conntype == CHITCPD_CONNECTION_TYPE__DEBUG_CONNECTION)
        {
            int debug_sockfd, debug_event_flags;
            ChitcpdDebugArgs *debug_args;

            /* Unpack debug parameter */
            assert(init_args->debug != NULL);
            debug_args = init_args->debug;

            debug_sockfd = debug_args->sockfd;
            debug_event_flags = debug_args->event_flags;

            rc = chitcpd_init_debug_connection(si, debug_sockfd, debug_event_flags, client_socket);
            if(rc == CHITCP_OK)
            {
                resp_outer.resp->ret = CHITCP_OK;
                resp_outer.resp->error_code = 0;
                rc = chitcpd_send_msg(client_socket, &resp_outer);
            }
            else
            {
                chilog(ERROR, "Error when creating debug connection for socket %i", debug_sockfd);
                resp_outer.resp->ret = CHITCP_EINIT;
                resp_outer.resp->error_code = rc;
                rc = chitcpd_send_msg(client_socket, &resp_outer);

                shutdown(client_socket, SHUT_RDWR);
            }
        }
        else
        {
            chilog(ERROR, "Received INIT message with unknown connection type %i", conntype);
            resp_outer.resp->ret = CHITCP_EINVAL;
            resp_outer.resp->error_code = 0;
            rc = chitcpd_send_msg(client_socket, &resp_outer);
            shutdown(client_socket, SHUT_RDWR);
        }

        chitcpd_msg__free_unpacked(req, NULL);
    }

    while(!list_empty(&handler_thread_list))
    {
        /* For each handler thread we spawned, we close its socket, which
         * will force the thread to exit (and we then join it).
         *
         * Note that closing a handler thread will also free up all chiTCP
         * sockets created through that thread, and will also terminate
         * all associated TCP threads.
         *
         * TODO: We should simply detach those threads, since they can exit
         * before an orderly shutdown and would be left lingering until
         * we call join here. */
        handler_thread_t *ht = list_fetch(&handler_thread_list);

        /* We don't want to shutdown the handler's socket if an operation is
         * in progress. The handler thread may have read a command, but
         * not sent a response back yet */
        pthread_mutex_lock(&ht->handler_lock);
        shutdown(ht->handler_socket, SHUT_RDWR);
        pthread_mutex_unlock(&ht->handler_lock);
        pthread_join(ht->thread, NULL);
        pthread_mutex_destroy(&ht->handler_lock);
        free(ht);
    }

    list_destroy(&handler_thread_list);

    pthread_exit(NULL);
}


/*
 * chitcpd_server_start_network_thread - Starts the network thread
 *
 * si: Server info
 *
 * Returns:
 *  - CHITCP_OK: The thread has started correctly
 *  - CHITCP_ESOCKET: Could not create socket
 *  - CHITCP_ETHREAD: Could not create thread
 *
 */
int chitcpd_server_start_network_thread(serverinfo_t *si)
{
    int yes=1;

    struct sockaddr_in server_addr;

    memset(&server_addr, 0, sizeof(server_addr));

    /* TODO: Add IPv6 support */
    server_addr.sin_family = AF_INET;          // IPv4
    server_addr.sin_port = si->server_port;
    server_addr.sin_addr.s_addr = INADDR_ANY;  // Bind to any address

    /* Create the socket */
    si->network_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(si->network_socket  == -1)
    {
        perror("Could not open network socket");
        return CHITCP_ESOCKET;
    }

    /* Make port immediately available after we close the socket */
    if(setsockopt(si->network_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
    {
        perror("Socket setsockopt() failed");
        close(si->network_socket);
        return CHITCP_ESOCKET;
    }

    /* Bind the socket to the address */
    if(bind(si->network_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1)
    {
        perror("Socket bind() failed");
        close(si->network_socket);
        return CHITCP_ESOCKET;
    }

    /* Start listening */
    if(listen(si->network_socket, 5) == -1)
    {
        perror("Network socket listen() failed");
        close(si->network_socket);
        return CHITCP_ESOCKET;
    }

    /* Create arguments to network thread */
    network_thread_args_t *nta = malloc(sizeof(network_thread_args_t));
    nta->si = si;

    /* Create network thread */
    if (pthread_create(&si->network_thread, NULL, chitcpd_server_network_thread_func, nta) < 0)
    {
        perror("Could not create network thread");
        free(nta);
        return CHITCP_ETHREAD;
    }

    return CHITCP_OK;
}


/*
 * chitcpd_server_network_thread_func - Server thread function
 *
 * This function will spawn a connection thread (see connection.c)
 * for each new connection on the UNIX socket.
 *
 * args: arguments (a serverinfo_t variable in network_thread_args_t)
 *
 * Returns: Nothing.
 *
 */
void* chitcpd_server_network_thread_func(void *args)
{
    socklen_t sunSize;
    network_thread_args_t *nta;
    socket_t realsocket;
    serverinfo_t *si;
    tcpconnentry_t* connection;
    char addr_str[100];

    pthread_setname_np(pthread_self(), "network_server");

    /* Unpack arguments */
    nta = (network_thread_args_t *) args;
    si = nta->si;

    struct sockaddr_storage client_addr;

    /* Accept connections on the TCP socket */
    for(;;)
    {
        /* Accept a connection */
        sunSize = sizeof(client_addr);
        if ((realsocket = accept(si->network_socket, (struct sockaddr *)&client_addr, &sunSize)) == -1)
        {
            /* If accept() returns in the CHITCPD_STATE_STOPPING, we don't
             * care what the error is. We just break out of the loop and
             * initiate an orderly shutdown. */
            if(si->state == CHITCPD_STATE_STOPPING)
                break;

            /* If this particular connection fails, no need to kill the entire thread. */
            perror("Could not accept() connection on network socket");
            continue;
        }

        chitcp_addr_str((struct sockaddr *) &client_addr, addr_str, sizeof(addr_str));
        chilog(INFO, "TCP connection received from %s", addr_str);

        /* Check whether the connection already exists. */
        connection = chitcpd_get_connection(si, (struct sockaddr *) &client_addr);
        if (connection != NULL)
        {
            /* If this is a loopback connection, there is already an entry in
             * the connection table, but we need to update its receive socket
             * and create its connection thread. */
           if(chitcp_addr_is_loopback((struct sockaddr *) &client_addr))
           {
               connection->realsocket_recv = realsocket;

               if(chitcpd_create_connection_thread(si, connection) != CHITCP_OK)
               {
                   perror("Could not create connection thread.");
                   // TODO: Perform orderly shutdown
                   pthread_exit(NULL);
               }

               continue;
           }
           else
           /* Otherwise, this is an error. The peer chiTCP daemon tried to create
            * a second connection, which shouldn't happen. */
            {
                perror("Peer chiTCP daemon tried to establish more than one connection.");
                close(realsocket);
                close(si->server_socket);
                // TODO: Perform orderly shutdown instead of just exiting
                pthread_exit(NULL);
            }
        }

        /* If this is not a loopback connection, we need to add an entry
         * for this connection */
        connection = chitcpd_add_connection(si, realsocket, realsocket, (struct sockaddr*) &client_addr);

        if (!connection)
        {
            perror("Could not create a connection to a peer chiTCP daemon");
            // TODO: Perform orderly shutdown
            pthread_exit(NULL);
        }

        if(chitcpd_create_connection_thread(si, connection) != CHITCP_OK)
        {
            perror("Could not create connection thread.");
            // TODO: Perform orderly shutdown
            pthread_exit(NULL);
        }
    }

    /* Close all TCP connections. This will force an exit of the
     * corresponding connection threads. */
    for(int i=0; i < si->connection_table_size; i++)
    {
        connection = &si->connection_table[i];
        if(!connection->available)
        {
            shutdown(connection->realsocket_recv, SHUT_RDWR);
            if (connection->realsocket_recv != connection->realsocket_send)
                shutdown(connection->realsocket_send, SHUT_RDWR);
            pthread_join(connection->thread, NULL);
        }
    }

    pthread_exit(NULL);
}
