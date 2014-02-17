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
} test_event_t;

typedef struct chitcp_tester_peer
{
    int sockfd;
    int passive_sockfd;

    pthread_t peer_thread;

    test_event_t event;
    pthread_cond_t cv_event;
    pthread_mutex_t lock_event;

    debug_event_handler debug_handler_func;
    int debug_event_flags;
} chitcp_tester_peer_t;

typedef struct test_peer_thread_args
{
    chitcp_tester_t *tester;
    chitcp_tester_peer_t *peer;
} test_peer_thread_args_t;

void* chitcp_tester_peer_thread_func(void *args);

int chitcp_tester_peer_event(chitcp_tester_peer_t* peer, test_event_t event);

#endif /* TESTER_PEER_H_ */
