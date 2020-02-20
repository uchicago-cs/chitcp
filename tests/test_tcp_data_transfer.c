#include <stdio.h>
#include <stdlib.h>
#include <criterion/criterion.h>
#include <criterion/parameterized.h>
#include "serverinfo.h"
#include "server.h"
#include "chitcp/chitcpd.h"
#include "chitcp/debug_api.h"
#include "chitcp/utils.h"
#include "chitcp/tester.h"
#include "fixtures.h"


uint8_t* generate_msg(int size)
{
    uint8_t *buf = malloc(size);

    for(int i=0; i < size; i++)
        buf[i] = i % 256;

    return buf;
}


int sender(int sockfd, void *args)
{
    int rc;
    int size = *((int *) args);

    uint8_t *buf = generate_msg(size);

    rc = chitcp_socket_send(sockfd, buf, size);

    cr_assert(rc == size,
              "Socket did not send all the bytes (expected %i, got %i)", size, rc);

    free(buf);

    return 0;
}


int receiver(int sockfd, void *args)
{
    int rc;
    int size = *((int *) args);
    uint8_t buf[size];

    rc = chitcp_socket_recv(sockfd, buf, size);
    cr_assert(rc == size,
              "Socket did not receive all the bytes (expected %i, got %i)", size, rc);

    for (int i = 0; i < size; i++)
        if(buf[i] != (i % 256))
            cr_assert_fail("Unexpected value encountered: buf[%i] == %i (expected %i)",
                           i, buf[i], (i % 256));

    return 0;
}

int client_echo(int sockfd, void *args)
{
    int rc;
    int size = *((int *) args);

    uint8_t *buf = generate_msg(size);
    uint8_t *recv_buf = calloc(size, 1);

    rc = chitcp_socket_send(sockfd, buf, size);
    cr_assert(rc == size,
              "Client socket did not send all the bytes (expected %i, got %i)", size, rc);

    rc = chitcp_socket_recv(sockfd, recv_buf, size);
    cr_assert(rc == size,
              "Client socket did not receive all the bytes (expected %i, got %i)", size, rc);

    for (int i = 0; i < size; i++)
        if(buf[i] != recv_buf[i])
            cr_assert_fail("Client received unexpected value: recv_buf[%i] == %i (expected %i)",
                           i, recv_buf[i], buf[i]);

    free(buf);
    free(recv_buf);

    return 0;
}


int server_echo(int sockfd, void *args)
{
    int rc;
    int size = *((int *) args);
    uint8_t buf[size];

    rc = chitcp_socket_recv(sockfd, buf, size);
    cr_assert(rc == size,
              "Server socket did not receive all the bytes (expected %i, got %i)", size, rc);

    for (int i = 0; i < size; i++)
        if(buf[i] != (i % 256))
            cr_assert_fail("Server received unexpected value: buf[%i] == %i (expected %i)",
                           i, buf[i], (i % 256));

    rc = chitcp_socket_send(sockfd, buf, size);
    cr_assert(rc == size,
              "Client socket did not send all the bytes (expected %i, got %i)", size, rc);

    return 0;
}



static const int bufsizes[6] = {1, 10, 536, 537, 1072, 4096};

void half_duplex_client_sends(int nbytes)
{
    chitcp_tester_client_run_set(tester, sender, &nbytes);
    chitcp_tester_server_run_set(tester, receiver, &nbytes);

    /* Minimizes the possibility of a zero window, which we cannot
     * deal with in the first part of the project */
    si->latency = 0.05;

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_run();

    tester_done();
}

Test(data_transfer, half_duplex_client_sends_1byte, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 0.5)
{
    half_duplex_client_sends(1);
}

Test(data_transfer, half_duplex_client_sends_10bytes, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 0.5)
{
    half_duplex_client_sends(10);
}

Test(data_transfer, half_duplex_client_sends_535bytes, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 0.5)
{
    half_duplex_client_sends(535);
}

Test(data_transfer, half_duplex_client_sends_536bytes, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 0.5)
{
    half_duplex_client_sends(536);
}

Test(data_transfer, half_duplex_client_sends_537bytes, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 0.5)
{
    half_duplex_client_sends(537);
}

Test(data_transfer, half_duplex_client_sends_1072bytes, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 1.0)
{
    half_duplex_client_sends(1072);
}

Test(data_transfer, half_duplex_client_sends_4096bytes, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 1.0)
{
    half_duplex_client_sends(4096);
}

Test(data_transfer, half_duplex_client_sends_4097bytes, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 1.0)
{
    half_duplex_client_sends(4097);
}

Test(data_transfer, half_duplex_client_sends_32768bytes, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 5.0)
{
    half_duplex_client_sends(32768);
}


void half_duplex_server_sends(int nbytes)
{
    chitcp_tester_client_run_set(tester, receiver, &nbytes);
    chitcp_tester_server_run_set(tester, sender, &nbytes);

    /* Minimizes the possibility of a zero window, which we cannot
     * deal with in chiTCP */
    si->latency = 0.05;

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_run();

    tester_done();
}


Test(data_transfer, half_duplex_server_sends_1byte, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 0.5)
{
    half_duplex_server_sends(1);
}

Test(data_transfer, half_duplex_server_sends_10bytes, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 0.5)
{
    half_duplex_server_sends(10);
}

Test(data_transfer, half_duplex_server_sends_535bytes, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 0.5)
{
    half_duplex_server_sends(535);
}

Test(data_transfer, half_duplex_server_sends_536bytes, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 0.5)
{
    half_duplex_server_sends(536);
}

Test(data_transfer, half_duplex_server_sends_537bytes, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 0.5)
{
    half_duplex_server_sends(537);
}

Test(data_transfer, half_duplex_server_sends_1072bytes, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 1.0)
{
    half_duplex_server_sends(1072);
}

Test(data_transfer, half_duplex_server_sends_4096bytes, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 1.0)
{
    half_duplex_server_sends(4096);
}

Test(data_transfer, half_duplex_server_sends_4097bytes, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 1.0)
{
    half_duplex_server_sends(4097);
}

Test(data_transfer, half_duplex_server_sends_32768bytes, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 5.0)
{
    half_duplex_server_sends(32768);
}


void echo(int nbytes)
{
    chitcp_tester_client_run_set(tester, client_echo, &nbytes);
    chitcp_tester_server_run_set(tester, server_echo, &nbytes);

    /* Minimizes the possibility of a zero window, which we cannot
     * deal with in chiTCP */
    si->latency = 0.05;

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_run();

    tester_done();
}


Test(data_transfer, echo_1byte, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 0.5)
{
    echo(1);
}

Test(data_transfer, echo_10bytes, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 0.5)
{
    echo(10);
}

Test(data_transfer, echo_535bytes, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 0.5)
{
    echo(535);
}

Test(data_transfer, echo_536bytes, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 0.5)
{
    echo(536);
}

Test(data_transfer, echo_537bytes, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 0.5)
{
    echo(537);
}

Test(data_transfer, echo_1072bytes, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 1.0)
{
    echo(1072);
}

Test(data_transfer, echo_4096bytes, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 1.0)
{
    echo(4096);
}

Test(data_transfer, echo_4097bytes, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 1.0)
{
    echo(4097);
}

Test(data_transfer, echo_32768bytes, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 5.0)
{
    echo(32768);
}
