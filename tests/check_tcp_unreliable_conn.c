/*
* check_tcp_unreliable_conn.c
*
*  Created on: December 22, 2014
*      Author: Tristan Rasmussen
*/

#include <stdio.h>
#include <stdlib.h>
#include <check.h>

#include "serverinfo.h"
#include "server.h"
#include "chitcp/chitcpd.h"
#include "chitcp/debug_api.h"
#include "chitcp/tester.h"
#include "fixtures.h"

int client_send_recv(int sockfd, void *args);
int server_send_recv(int sockfd, void *args);

static int drop_count;
static int dropped;
static tcp_state_t drop_in_state;
/* A simple debug handler that will drop up to drop_count packets. */
enum chitcpd_debug_response drop_packets(int sockfd, enum chitcpd_debug_event event_flag, debug_socket_state_t *state_info, debug_socket_state_t *saved_state_info, int new_sockfd)
{
    if (event_flag == DBG_EVT_PENDING_CONNECTION)
    {
        return DBG_RESP_ACCEPT_MONITOR;
    }
    else if (event_flag == DBG_EVT_INCOMING_PACKET)
    {
        tcp_state_t curs = state_info->tcp_state;
        ck_assert_msg(IS_VALID_TCP_STATE(curs), "Unknown TCP state.");
        if (curs == drop_in_state)
        {
            if (dropped < drop_count)
            {
                dropped++;
                return DBG_RESP_DROP;
            }
            else
            {
                return DBG_RESP_NONE;
            }
        }
    }
    return DBG_RESP_NONE;
}

/*
 * This makes things much more diffuclt. It will drop packets at random, dictated
 * by the drop percentage.
 */
static float drop_percentage = 0.25; /* Percentage of packets to drop. */
static unsigned int seed = 0; /* Change this number to change the dropped packets */
enum chitcpd_debug_response drop_random_packets(int sockfd, enum chitcpd_debug_event event_flag, debug_socket_state_t *state_info, debug_socket_state_t *saved_state_info, int new_sockfd)
{

    if (event_flag == DBG_EVT_PENDING_CONNECTION)
    {
        return DBG_RESP_ACCEPT_MONITOR;
    }
    else if (event_flag == DBG_EVT_INCOMING_PACKET)
    {
        float r = (float)rand_r(&seed)/(float)(RAND_MAX);
        if (r < drop_percentage)
        {
            return DBG_RESP_DROP;
        }
        else
        {
            return DBG_RESP_NONE;
        }
    }
    return DBG_RESP_NONE;
}


/* This allows a test to specify debug commands for the first 16 packets in
 * the established state. This allows us to reliably reproduce various out of
 * order scenarios.
 */
static int packet_id = 0;
static enum chitcpd_debug_response packet_sequence[16];
static int packet_sequence_size = 0;

enum chitcpd_debug_response out_of_order_handler(int sockfd, enum chitcpd_debug_event event_flag, debug_socket_state_t *state_info, debug_socket_state_t *saved_state_info, int new_sockfd)
{
    if (event_flag == DBG_EVT_PENDING_CONNECTION)
    {
        return DBG_RESP_ACCEPT_MONITOR;
    }
    else if (event_flag == DBG_EVT_INCOMING_PACKET)
    {
        tcp_state_t curs = state_info->tcp_state;
        ck_assert_msg(IS_VALID_TCP_STATE(curs), "Unknown TCP state.");
        if (curs == ESTABLISHED)
        {
            enum chitcpd_debug_response response;
            if (packet_id < packet_sequence_size)
            {
                response = packet_sequence[packet_id];
            }
            else
            {
                response = DBG_RESP_DROP;
            }
            packet_id++;
            return response;
        }
    }
    return DBG_RESP_NONE;
}

START_TEST (drop_syn_packet)
{
    drop_count = 1;
    dropped = 0;
    drop_in_state = CLOSED;

    chitcp_tester_server_set_debug(tester, drop_packets,
    DBG_EVT_PENDING_CONNECTION | DBG_EVT_INCOMING_PACKET | DBG_EVT_OUTGOING_PACKET);

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_done();
}
END_TEST

START_TEST (drop_fin_packet)
{
    drop_count = 1;
    dropped = 0;
    drop_in_state = ESTABLISHED;

    chitcp_tester_server_set_debug(tester, drop_packets,
    DBG_EVT_PENDING_CONNECTION | DBG_EVT_INCOMING_PACKET);

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    chitcp_tester_client_close(tester);
    chitcp_tester_client_wait_for_state(tester, FIN_WAIT_2);
    chitcp_tester_server_close(tester);

    chitcp_tester_server_wait_for_state(tester, CLOSED);
    chitcp_tester_client_wait_for_state(tester, CLOSED);

    tester_done();
}
END_TEST

START_TEST (drop_single_packet)
{
    int *nbytes = malloc(sizeof(int));

    *nbytes = 15;
    drop_count = 1;
    dropped = 0;
    drop_in_state = ESTABLISHED;

    chitcp_tester_client_run_set(tester, client_send_recv, nbytes);
    chitcp_tester_server_run_set(tester, server_send_recv, nbytes);

    chitcp_tester_server_set_debug(tester, drop_packets,
    DBG_EVT_PENDING_CONNECTION | DBG_EVT_INCOMING_PACKET);

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_run();

    tester_done();

    free(nbytes);
}
END_TEST

START_TEST (drop_multiple_packets)
{
    int *nbytes = malloc(sizeof(int));

    *nbytes = 15;
    drop_count = 2;
    dropped = 0;
    drop_in_state = ESTABLISHED;

    chitcp_tester_client_run_set(tester, client_send_recv, nbytes);
    chitcp_tester_server_run_set(tester, server_send_recv, nbytes);

    chitcp_tester_server_set_debug(tester, drop_packets,
    DBG_EVT_PENDING_CONNECTION | DBG_EVT_INCOMING_PACKET);

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_run();

    tester_done();

    free(nbytes);
}
END_TEST

START_TEST (random_drop)
{
    int *nbytes = malloc(sizeof(int));

    *nbytes = 2048;


    chitcp_tester_client_run_set(tester, client_send_recv, nbytes);
    chitcp_tester_server_run_set(tester, server_send_recv, nbytes);

    chitcp_tester_server_set_debug(tester, drop_random_packets,
    DBG_EVT_PENDING_CONNECTION | DBG_EVT_INCOMING_PACKET);

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_run();

    tester_done();

    free(nbytes);
}
END_TEST


START_TEST (single_out_of_order)
{
    int *nbytes = malloc(sizeof(int));

    *nbytes = 1608; /* MSS * 3 to ensure 3 data packets are sent. */
    packet_id = 0;
    packet_sequence_size = 3;
    /* Packet should arrive in the sequence: 2, 1, 3 */
    packet_sequence[0] = DBG_RESP_WITHHOLD;
    packet_sequence[1] = DBG_RESP_NONE;
    packet_sequence[2] = DBG_RESP_DRAW_WITHHELD;

    chitcp_tester_client_run_set(tester, client_send_recv, nbytes);
    chitcp_tester_server_run_set(tester, server_send_recv, nbytes);

    chitcp_tester_server_set_debug(tester, out_of_order_handler,
    DBG_EVT_PENDING_CONNECTION | DBG_EVT_INCOMING_PACKET);

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_run();

    tester_done();

    free(nbytes);
}
END_TEST


START_TEST (double_out_of_order)
{
    int *nbytes = malloc(sizeof(int));

    *nbytes = 2114;
    packet_id = 0;
    /* The packets should arrive as follows: 3, 2, 4, 1 (requires retransmission) */
    packet_sequence_size = 5;
    packet_sequence[0] = DBG_RESP_DROP;
    packet_sequence[1] = DBG_RESP_WITHHOLD;
    packet_sequence[2] = DBG_RESP_NONE;
    packet_sequence[3] = DBG_RESP_DRAW_WITHHELD;
    packet_sequence[4] = DBG_RESP_NONE;

    chitcp_tester_client_run_set(tester, client_send_recv, nbytes);
    chitcp_tester_server_run_set(tester, server_send_recv, nbytes);

    chitcp_tester_server_set_debug(tester, out_of_order_handler,
    DBG_EVT_PENDING_CONNECTION | DBG_EVT_INCOMING_PACKET);

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_run();

    tester_done();

    free(nbytes);
}
END_TEST

Suite* make_unreliable_connection_suite (void)
{
    Suite *s = suite_create ("TCP: Unreliable Connection");

    /* Core test case */
    TCase *tc_unreliable = tcase_create ("Dropped packets");
    tcase_add_checked_fixture (tc_unreliable, chitcpd_and_tester_setup, chitcpd_and_tester_teardown);
    tcase_add_test (tc_unreliable, drop_single_packet);
    tcase_add_test (tc_unreliable, drop_multiple_packets);
    tcase_add_test(tc_unreliable, drop_syn_packet);
    tcase_add_test(tc_unreliable, drop_fin_packet);
    tcase_add_test(tc_unreliable, random_drop);
    /* Set a 10 seconds since the random drop can take a while */
    tcase_set_timeout(tc_unreliable, 10);

    TCase *tc_out_of_order = tcase_create ("Out of order packets");
    tcase_add_checked_fixture (tc_out_of_order, chitcpd_and_tester_setup, chitcpd_and_tester_teardown);
    tcase_add_test (tc_out_of_order, single_out_of_order);
    tcase_add_test (tc_out_of_order, double_out_of_order);

    suite_add_tcase (s, tc_unreliable);
    suite_add_tcase (s, tc_out_of_order);

    return s;
}
