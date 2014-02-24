#ifndef TESTER_H_
#define TESTER_H_

#include "chitcp/debug_api.h"

typedef struct chitcp_tester_peer chitcp_tester_peer_t;

typedef struct chitcp_tester
{
    chitcp_tester_peer_t *server;
    chitcp_tester_peer_t *client;
} chitcp_tester_t;


typedef int (*chitcp_tester_runnable)(int sockfd, void *args);

/*
 * chitcp_tester_init - Initializes a tester
 *
 * tester: Tester data structure
 *
 * Returns:
 *  - CHITCP_OK: Packet was processed correctly
 *  - CHITCP_EINIT: Error initializing the tester
 *
 */
int chitcp_tester_init(chitcp_tester_t* tester);


/*
 * chitcp_tester_free - Frees a tester's resources
 *
 * tester: Tester data structure
 *
 * Returns:
 *  - CHITCP_OK: Packet was processed correctly
 *
 */
int chitcp_tester_free(chitcp_tester_t* tester);


/*
 * chitcp_tester_start - Starts the tester
 *
 * This function will launch a server thread and a client thread.
 * Each thread will create a socket, but won't call any other
 * socket functions.
 *
 * tester: Tester data structure
 *
 * Returns:
 *  - CHITCP_OK: Packet was processed correctly
 *  - CHITCP_ETHREAD: Could not create thread
 */
int chitcp_tester_start(chitcp_tester_t* tester);


/*
 * chitcp_tester_server_set_debug - Adds a debug handler to the server
 *
 * This function uses the chiTCP debug API (see debug_api.h) to assign
 * a debug handler to the server socket.
 *
 * tester: Tester data structure
 *
 * handler: Pointer to function of type debug_event_handler
 *
 * event_flags: Events to be handled by the debug handler
 *
 * Returns:
 *  - CHITCP_OK: Debug handler set correctly
 *  - CHITCP_EINVAL: Invalid parameter value
 */
int chitcp_tester_server_set_debug(chitcp_tester_t* tester, debug_event_handler handler, int event_flags);



/*
 * chitcp_tester_client_set_debug - Adds a debug handler to the client
 *
 * Same as chitcp_tester_server_set_debug, but for the client socket
 */
int chitcp_tester_client_set_debug(chitcp_tester_t* tester, debug_event_handler handler, int event_flags);



/*
 * chitcp_tester_server_wait_for_state - Wait for server socket to reach TCP state
 *
 * This function uses the chiTCP debug API (see debug_api.h) to wait
 * for the TCP state of a given socket to reach a certain state.
 *
 * tester: Tester data structure
 *
 * tcp_state: TCP state
 *
 * Returns:
 *  - CHITCP_OK: Debug handler set correctly
 *  - CHITCP_EINVAL: Invalid parameter value
 */
int chitcp_tester_server_wait_for_state(chitcp_tester_t* tester, tcp_state_t tcp_state);


/*
 * chitcp_tester_client_wait_for_state - Wait for client socket to reach TCP state
 *
 * Same as chitcp_tester_server_wait_for_state but for the client socket
 */
int chitcp_tester_client_wait_for_state(chitcp_tester_t* tester, tcp_state_t tcp_state);

/*
 * chitcp_tester_server_listen - Start listening on server socket
 *
 * Calls bind() and listen() on the server socket
 *
 * tester: Tester data structure
 *
 * Returns:
 *  - CHITCP_OK: Packet was processed correctly
 *  - CHITCP_ESYNC: Unable to signal server thread
 */
int chitcp_tester_server_listen(chitcp_tester_t* tester);


/*
 * chitcp_tester_server_accept - Tell server to accept a connection
 *
 * Calls accept() on the server socket
 *
 * tester: Tester data structure
 *
 * Returns:
 *  - CHITCP_OK: Packet was processed correctly
 *  - CHITCP_ESYNC: Unable to signal server thread
 */
int chitcp_tester_server_accept(chitcp_tester_t* tester);


/*
 * chitcp_tester_client_connect - Make client connect to server
 *
 * Calls connect() on the client socket
 *
 * tester: Tester data structure
 *
 * Returns:
 *  - CHITCP_OK: Packet was processed correctly
 *  - CHITCP_ESYNC: Unable to signal client thread
 */
int chitcp_tester_client_connect(chitcp_tester_t* tester);


/*
 * chitcp_tester_client_run_set - Specify a function for the client to run
 *
 *
 * tester: Tester data structure
 *
 * func: Function to run
 *
 * Returns:
 *  - CHITCP_OK: Function set correctly
 */
int chitcp_tester_client_run_set(chitcp_tester_t* tester, chitcp_tester_runnable func, void *args);


/*
 * chitcp_tester_client_run - Run the client's functions
 *
 * tester: Tester data structure
 *
 * Returns:
 *  - CHITCP_OK: Function set correctly
 */
int chitcp_tester_client_run(chitcp_tester_t* tester);


/* Same as above, but for the server */
int chitcp_tester_server_run_set(chitcp_tester_t* tester, chitcp_tester_runnable func, void *args);
int chitcp_tester_server_run(chitcp_tester_t* tester);


/*
 * chitcp_tester_client_close - Close client
 *
 * Calls close() on the client socket
 *
 * tester: Tester data structure
 *
 * Returns:
 *  - CHITCP_OK: Packet was processed correctly
 *  - CHITCP_ESYNC: Unable to signal client thread
 */
int chitcp_tester_client_close(chitcp_tester_t* tester);


/*
 * chitcp_tester_server_close - Close server
 *
 * Calls close() on the server socket
 *
 * tester: Tester data structure
 *
 * Returns:
 *  - CHITCP_OK: Packet was processed correctly
 *  - CHITCP_ESYNC: Unable to signal client thread
 */
int chitcp_tester_server_close(chitcp_tester_t* tester);



int chitcp_tester_server_exit(chitcp_tester_t* tester);
int chitcp_tester_client_exit(chitcp_tester_t* tester);

#endif /* TESTER_H_ */
