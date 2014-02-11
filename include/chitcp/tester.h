#ifndef TESTER_H_
#define TESTER_H_

#include "chitcp/debug_api.h"

typedef struct chitcp_tester_peer chitcp_tester_peer_t;

typedef struct chitcp_tester
{
    chitcp_tester_peer_t *server;
    chitcp_tester_peer_t *client;
} chitcp_tester_t;


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


int chitcp_tester_server_set_debug(chitcp_tester_t* tester, debug_event_handler handler, int event_flags);
int chitcp_tester_client_set_debug(chitcp_tester_t* tester, debug_event_handler handler, int event_flags);


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


#endif /* TESTER_H_ */
