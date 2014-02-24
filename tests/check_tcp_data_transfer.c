/*
 * check_chitcpd.c
 *
 *  Created on: Jan 11, 2014
 *      Author: borja
 */

#include <stdio.h>
#include <stdlib.h>
#include <check.h>
#include "serverinfo.h"
#include "server.h"
#include "chitcp/chitcpd.h"
#include "chitcp/debug_api.h"
#include "chitcp/tester.h"
#include "chitcp/utils.h"
#include "fixtures.h"

static const int bufsizes[6] = {1, 10, 536, 537, 1072, 4096};

uint8_t* generate_msg(int size)
{
    uint8_t *buf = malloc(size);

    for(int i=0; i < size; i++)
        buf[i] = i % 256;

    return buf;
}


int client_send_recv(int sockfd, void *args)
{
    int rc;
    int size = *((int *) args);

    uint8_t *buf = generate_msg(size);

    rc = chitcp_socket_send(sockfd, buf, size);
    ck_assert_msg(rc == size,
                  "Socket did not send all the bytes (expected %i, got %i)", size, rc);

    free(buf);

    return 0;
}


int server_send_recv(int sockfd, void *args)
{
    int rc;
    int size = *((int *) args);
    uint8_t buf[size];

    rc = chitcp_socket_recv(sockfd, buf, size);
    ck_assert_msg(rc == size,
                  "Socket did not receive all the bytes (expected %i, got %i)", size, rc);

    for (int i = 0; i < size; i++)
        ck_assert_msg(buf[i] == (i % 256),
                      "Unexpected value encountered: buf[%i] == %i (expected %i)",
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
    ck_assert_msg(rc == size,
                  "Client socket did not send all the bytes (expected %i, got %i)", size, rc);

    rc = chitcp_socket_recv(sockfd, recv_buf, size);
    ck_assert_msg(rc == size,
                  "Client socket did not receive all the bytes (expected %i, got %i)", size, rc);

    for (int i = 0; i < size; i++)
        ck_assert_msg(buf[i] == recv_buf[i],
                      "Client received unexpected value: recv_buf[%i] == %i (expected %i)",
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
    ck_assert_msg(rc == size,
                  "Server socket did not receive all the bytes (expected %i, got %i)", size, rc);

    for (int i = 0; i < size; i++)
        ck_assert_msg(buf[i] == (i % 256),
                      "Server received unexpected value: buf[%i] == %i (expected %i)",
                      i, buf[i], (i % 256));

    rc = chitcp_socket_send(sockfd, buf, size);
    ck_assert_msg(rc == size,
                  "Client socket did not send all the bytes (expected %i, got %i)", size, rc);

    return 0;
}

START_TEST (test_data_send_recv)
{
    int *nbytes = malloc(sizeof(int));

    *nbytes = bufsizes[_i];

    chitcp_tester_client_run_set(tester, client_send_recv, nbytes);
    chitcp_tester_server_run_set(tester, server_send_recv, nbytes);

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_run();

    tester_done();

    free(nbytes);
}
END_TEST

START_TEST (test_data_echo)
{
    int *nbytes = malloc(sizeof(int));

    *nbytes = bufsizes[_i];

    chitcp_tester_client_run_set(tester, client_echo, nbytes);
    chitcp_tester_server_run_set(tester, server_echo, nbytes);

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_run();

    tester_done();

    free(nbytes);
}
END_TEST

Suite* make_data_transfer_suite (void)
{
  Suite *s = suite_create ("TCP: Data transfer");

  /* Core test case */
  TCase *tc_send_recv = tcase_create ("Client sends, Server receives");
  tcase_add_checked_fixture (tc_send_recv, chitcpd_and_tester_setup, chitcpd_and_tester_teardown);
  tcase_add_loop_test (tc_send_recv, test_data_send_recv, 0, 6);
  suite_add_tcase (s, tc_send_recv);

  TCase *tc_echo = tcase_create ("Echo");
  tcase_add_checked_fixture (tc_echo, chitcpd_and_tester_setup, chitcpd_and_tester_teardown);
  tcase_add_loop_test (tc_echo, test_data_echo, 0, 6);
  suite_add_tcase (s, tc_echo);

  return s;
}
