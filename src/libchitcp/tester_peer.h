#ifndef TESTER_PEER_H_
#define TESTER_PEER_H_

#include "chitcp/tester.h"
#include "chitcp/debug_api.h"

typedef enum
{
    TEST_EVENT_NONE  = 0,
    TEST_EVENT_INIT  = 1,
    TEST_EVENT_LISTEN  = 2,
    TEST_EVENT_ACCEPT  = 3,
    TEST_EVENT_CONNECT = 4,
    TEST_EVENT_RUN = 5,
    TEST_EVENT_CLOSE = 6,
    TEST_EVENT_EXIT = 7,
} test_event_t;

typedef enum
{
    STATE_UNINITIALIZED = 0,
    STATE_INITIALIZED = 1,

    STATE_SERVER_LISTENING = 2,
    STATE_SERVER_READY = 3,
    STATE_SERVER_CLOSING = 4,
    STATE_SERVER_CLOSED = 5,

    STATE_CLIENT_CONNECTING = 6,
    STATE_CLIENT_READY = 7,
    STATE_CLIENT_CLOSING = 8,
    STATE_CLIENT_CLOSED = 9,

    STATE_RUNNING_FUNCTION = 10,
} peer_state_t;

typedef struct chitcp_tester_peer
{
    int sockfd;
    int passive_sockfd;

    pthread_t peer_thread;

    chitcp_tester_runnable func;
    void *func_args;

    test_event_t event;
    pthread_cond_t cv_event;
    pthread_mutex_t lock_event;

    peer_state_t state;
    pthread_cond_t cv_state;
    pthread_mutex_t lock_state;

    debug_event_handler debug_handler_func;
    int debug_event_flags;
} chitcp_tester_peer_t;

typedef struct test_peer_thread_args
{
    chitcp_tester_t *tester;
    chitcp_tester_peer_t *peer;
} test_peer_thread_args_t;

void* chitcp_tester_peer_thread_func(void *args);

int chitcp_tester_peer_update_state(chitcp_tester_peer_t* peer, peer_state_t state);
int chitcp_tester_peer_wait_for_state(chitcp_tester_peer_t* peer, peer_state_t state);
int chitcp_tester_peer_event(chitcp_tester_peer_t* peer, test_event_t event);

#endif /* TESTER_PEER_H_ */
