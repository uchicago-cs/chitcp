#include <unistd.h>
#include <criterion/criterion.h>

#include "chitcp/debug_api.h"
#include "chitcp/tester.h"
#include "chitcp/utils.h"
#include "fixtures.h"

int sender(int sockfd, void *args);

int slow_receiver(int sockfd, void *args)
{
    int rc;
    int size = *((int *) args);
    uint8_t buf[size];

    sleep(2);

    rc = chitcp_socket_recv(sockfd, buf, size);
    cr_assert(rc == size,
              "Socket did not receive all the bytes (expected %i, got %i)", size, rc);

    for (int i = 0; i < size; i++)
        if(buf[i] != (i % 256))
            cr_assert_fail("Unexpected value encountered: buf[%i] == %i (expected %i)",
                           i, buf[i], (i % 256));

    return 0;
}

void test_slow_receiver(int nbytes)
{
    chitcp_tester_client_run_set(tester, sender, &nbytes);
    chitcp_tester_server_run_set(tester, slow_receiver, &nbytes);

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_run();

    tester_done();
}

/* Send BUFFER_SIZE + MSS (4096 + 536 = 4632) */
Test(persist, slow_receiver_4632bytes, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 5)
{
    test_slow_receiver(4632);
}

/* Send BUFFER_SIZE (4096) */
Test(persist, slow_receiver_4096bytes, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 5)
{
    test_slow_receiver(4096);
}

/* Send BUFFER_SIZE + 1 (4096 + 1 = 4097) */
Test(persist, slow_receiver_4097bytes, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 5)
{
    test_slow_receiver(4097);
}

/* Send BUFFER_SIZE + 2 (4096 + 2 = 4098) */
Test(persist, slow_receiver_4098bytes, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 5)
{
    test_slow_receiver(4098);
}

/* Send 2 * BUFFER_SIZE (4096 * 2 = 8192) */
Test(persist, slow_receiver_8192bytes, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 5)
{
    test_slow_receiver(8192);
}

/* Send 2 * BUFFER_SIZE + MSS (4096 * 2 + 536 = 8728) */
Test(persist, slow_receiver_8728bytes, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 5)
{
    test_slow_receiver(8192);
}