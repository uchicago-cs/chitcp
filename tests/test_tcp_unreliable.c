/*
* check_tcp_unreliable_conn.c
*
*  Created on: December 22, 2014
*      Author: Tristan Rasmussen
*/

#include <stdio.h>
#include <stdlib.h>
#include <criterion/criterion.h>

#include "serverinfo.h"
#include "server.h"
#include "chitcp/chitcpd.h"
#include "chitcp/debug_api.h"
#include "chitcp/tester.h"
#include "fixtures.h"

int sender(int sockfd, void *args);
int receiver(int sockfd, void *args);
int client_echo(int sockfd, void *args);
int server_echo(int sockfd, void *args);


static int drop_count;
static int dropped;
static bool_t drop_in_passive;
static bool_t drop_in_active;
static tcp_state_t drop_in_state;

/* A simple debug handler that will drop up to drop_count packets. */
enum chitcpd_debug_response drop_packets(int sockfd, enum chitcpd_debug_event event_flag, debug_socket_state_t *state_info, debug_socket_state_t *saved_state_info, int new_sockfd)
{
    if (event_flag == DBG_EVT_PENDING_CONNECTION)
    {
        return DBG_RESP_ACCEPT_MONITOR;
    }
    else if (event_flag == DBG_EVT_INCOMING_PACKET || event_flag == DBG_EVT_OUTGOING_PACKET)
    {
        tcp_state_t curs;

        if(state_info)
        {
            curs = state_info->tcp_state;
            cr_assert(IS_VALID_TCP_STATE(curs), "Unknown TCP state.");
        }

        if ( (drop_in_passive && state_info == NULL) || (drop_in_active && state_info != NULL && curs == drop_in_state) )
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
 * This makes things much more difficult. It will drop packets at random, dictated
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
        tcp_state_t curs;

        if(state_info)
        {
            curs = state_info->tcp_state;
            cr_assert(IS_VALID_TCP_STATE(curs), "Unknown TCP state.");

            if(curs == ESTABLISHED)
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

        if(state_info)
        {
            tcp_state_t curs = state_info->tcp_state;

            cr_assert(IS_VALID_TCP_STATE(curs), "Unknown TCP state.");
            if (curs == ESTABLISHED)
            {
                enum chitcpd_debug_response response;
                if (packet_id < packet_sequence_size)
                {
                    response = packet_sequence[packet_id];
                }
                else
                {
                    response = DBG_RESP_NONE;
                }
                packet_id++;
                return response;
            }
        }
    }
    return DBG_RESP_NONE;
}

/* This test performs a three-way handshake, but drops the SYN packet sent by
 * the active opener. It should detect that no SYN-ACK was received, and resent
 * the same SYN packet. */
Test(unreliable_conn_init, drop_syn, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 2.0)
{
    drop_count = 1;
    dropped = 0;
    drop_in_passive = TRUE;
    drop_in_active = FALSE;

    chitcp_tester_server_set_debug(tester, drop_packets,
    DBG_EVT_PENDING_CONNECTION | DBG_EVT_INCOMING_PACKET);

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_done();
}

/* This test performs a three-way handshake, but drops the SYN-ACK packet sent by
 * the passive opener. This will result in both the SYN packet timing out (because
 * the active opener didn't receive an ACK for its SYN) and the SYN-ACK packet itself
 * timing out (because it was also not ACK'd) */
Test(unreliable_conn_init, drop_synack, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 2.0)
{
    drop_count = 1;
    dropped = 0;
    drop_in_state = SYN_SENT;
    drop_in_passive = FALSE;
    drop_in_active = TRUE;

    chitcp_tester_client_set_debug(tester, drop_packets,
    DBG_EVT_PENDING_CONNECTION | DBG_EVT_INCOMING_PACKET);

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_done();
}

/* This tests performs a three-way handshake and drops the final ACK. However,
 * the active opener then sends some data, which piggybacks the missing ACK for
 * the SYN. The data should be received correctly *and* the passive opener should
 * transition to ESTABLISHED. Note that there should be no timeouts in this test.*/
Test(unreliable_conn_init, drop_ack, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 2.0)
{
    int *nbytes = malloc(sizeof(int));

    *nbytes = 15;
    drop_count = 1;
    dropped = 0;
    drop_in_state = SYN_RCVD;
    drop_in_passive = FALSE;
    drop_in_active = TRUE;

    chitcp_tester_server_set_debug(tester, drop_packets,
    DBG_EVT_PENDING_CONNECTION | DBG_EVT_INCOMING_PACKET);

    chitcp_tester_client_run_set(tester, sender, nbytes);
    chitcp_tester_server_run_set(tester, receiver, nbytes);

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);

    /* At this point, the client is in ESTABLISHED, but the server is
     * in SYN_RCVD because the last message in the three-way handshake
     * (the ACK) was dropped. However, receiving some data should make
     * the server transition to ESTABLISHED
     *
     * Note: We call the chitcp_tester_*_run functions manually (instead
     * of tester_run() because we need the client code to run first; otherwise
     * the test will block waiting for the server to go into ESTABLISHED  */
    chitcp_tester_client_run(tester);
    chitcp_tester_server_run(tester);

    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_done();

    free(nbytes);
}

/* This tests established a connection (without any packet drops), and then the client
 * initiates a connection termination by sending a FIN packet, which is dropped.
 * The client should detect this and re-send the FIN. */
Test(unreliable_conn_term, drop_fin_1, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 2.0)
{
    drop_count = 1;
    dropped = 0;
    drop_in_state = ESTABLISHED;
    drop_in_passive = FALSE;
    drop_in_active = TRUE;

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

/* This tests established a connection (without any packet drops), and then the client
 * initiates a connection termination by sending a FIN packet, which is delivered
 * normally (with the client transitioning to FIN_WAIT_2). The server then initiates
 * a termination of its side of the connection by sending a FIN packet, which is dropped.
 * The server should detect this (because it did not receive an ACK in the LAST_ACK state)
 * and should re-send the FIN packet. */
Test(unreliable_conn_term, drop_fin_2, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 2.0)
{
    drop_count = 1;
    dropped = 0;
    drop_in_state = FIN_WAIT_2;
    drop_in_passive = FALSE;
    drop_in_active = TRUE;

    chitcp_tester_client_set_debug(tester, drop_packets,
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


/* This test established a connection, and then has the client send 15 bytes to the server
 * (which should be sent in a single packet). This packet is dropped, and should be re-sent. */
Test(unreliable_data_transfer, drop_single_packet, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 2.0)
{
    int *nbytes = malloc(sizeof(int));

    *nbytes = 15;
    drop_count = 1;
    dropped = 0;
    drop_in_state = ESTABLISHED;
    drop_in_passive = FALSE;
    drop_in_active = TRUE;

    chitcp_tester_client_run_set(tester, sender, nbytes);
    chitcp_tester_server_run_set(tester, receiver, nbytes);

    chitcp_tester_server_set_debug(tester, drop_packets,
    DBG_EVT_PENDING_CONNECTION | DBG_EVT_INCOMING_PACKET);

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_run();

    tester_done();

    free(nbytes);
}


/* This test establishes a connection, and then has the client send 15 bytes to the server
 * (which should be sent in a single packet). This packet is dropped, and should be re-sent.
 * However, the re-sent packet will *also* be dropped, so it should be sent again.*/
Test(unreliable_data_transfer, drop_multiple_packets, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 4.0)
{
    int *nbytes = malloc(sizeof(int));

    *nbytes = 15;
    drop_count = 2;
    dropped = 0;
    drop_in_state = ESTABLISHED;
    drop_in_passive = FALSE;
    drop_in_active = TRUE;

    chitcp_tester_client_run_set(tester, sender, nbytes);
    chitcp_tester_server_run_set(tester, receiver, nbytes);

    chitcp_tester_server_set_debug(tester, drop_packets,
    DBG_EVT_PENDING_CONNECTION | DBG_EVT_INCOMING_PACKET);

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_run();

    tester_done();

    free(nbytes);
}

/* This test establishes a connection, and then has the client send 2680 bytes to the server
 * (which should be sent in five packets). The first packet is dropped, and should be re-sent.
 * If go-back-N is implemented the subsequent four packets are resent as well. */
Test(unreliable_data_transfer, go_back_n, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 4.0)
{
    int *nbytes = malloc(sizeof(int));

    *nbytes = 2680;
    drop_count = 1;
    dropped = 0;
    drop_in_state = ESTABLISHED;
    drop_in_passive = FALSE;
    drop_in_active = TRUE;

    chitcp_tester_client_run_set(tester, sender, nbytes);
    chitcp_tester_server_run_set(tester, receiver, nbytes);

    chitcp_tester_server_set_debug(tester, drop_packets,
    DBG_EVT_PENDING_CONNECTION | DBG_EVT_INCOMING_PACKET);

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_run();

    tester_done();

    free(nbytes);
}

Test(rtt_estimation, rtt_0_75s, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 10)
{
    int *nbytes = malloc(sizeof(int));

    *nbytes = 16384;

    chitcp_tester_client_run_set(tester, sender, nbytes);
    chitcp_tester_server_run_set(tester, receiver, nbytes);
    si->latency = 0.375; // RTT = 0.75s

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_run();

    chitcp_tester_client_close(tester);
    chitcp_tester_client_wait_for_state(tester, FIN_WAIT_2);
    chitcp_tester_server_close(tester);

    chitcp_tester_server_wait_for_state(tester, CLOSED);
    chitcp_tester_client_wait_for_state(tester, CLOSED);

    tester_done();

    free(nbytes);
}

Test(rtt_estimation, rtt_1_5s, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 20.0)
{
    int *nbytes = malloc(sizeof(int));

    *nbytes = 16384;

    chitcp_tester_client_run_set(tester, sender, nbytes);
    chitcp_tester_server_run_set(tester, receiver, nbytes);
    si->latency = 0.75; // RTT = 1.5s

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_run();

    chitcp_tester_client_close(tester);
    chitcp_tester_client_wait_for_state(tester, FIN_WAIT_2);
    chitcp_tester_server_close(tester);

    chitcp_tester_server_wait_for_state(tester, CLOSED);
    chitcp_tester_client_wait_for_state(tester, CLOSED);

    tester_done();

    free(nbytes);
}

Test(rtt_estimation, rtt_3s, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 40.0)
{
    int *nbytes = malloc(sizeof(int));

    *nbytes = 16384;

    chitcp_tester_client_run_set(tester, sender, nbytes);
    chitcp_tester_server_run_set(tester, receiver, nbytes);
    si->latency = 1.5; // RTT = 3s

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_run();

    chitcp_tester_client_close(tester);
    chitcp_tester_client_wait_for_state(tester, FIN_WAIT_2);
    chitcp_tester_server_close(tester);

    chitcp_tester_server_wait_for_state(tester, CLOSED);
    chitcp_tester_client_wait_for_state(tester, CLOSED);

    tester_done();

    free(nbytes);
}


/* This test sends 32KB of data, and drops packets with probability p=0.025 */
Test(unreliable_data_transfer, random_drop_025_1, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 2.0)
{
    int *nbytes = malloc(sizeof(int));

    *nbytes = 32768;
    drop_percentage = 0.025;
    seed = 23300;

    chitcp_tester_client_run_set(tester, sender, nbytes);
    chitcp_tester_server_run_set(tester, receiver, nbytes);

    chitcp_tester_server_set_debug(tester, drop_random_packets,
    DBG_EVT_PENDING_CONNECTION | DBG_EVT_INCOMING_PACKET);

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_run();

    tester_done();

    free(nbytes);
}

/* This test sends 32KB of data, and drops packets with probability p=0.025 */
Test(unreliable_data_transfer, random_drop_025_2, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 2.0)
{
    int *nbytes = malloc(sizeof(int));

    *nbytes = 32768;
    drop_percentage = 0.025;
    seed = 23310;

    chitcp_tester_client_run_set(tester, sender, nbytes);
    chitcp_tester_server_run_set(tester, receiver, nbytes);

    chitcp_tester_server_set_debug(tester, drop_random_packets,
    DBG_EVT_PENDING_CONNECTION | DBG_EVT_INCOMING_PACKET);

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_run();

    tester_done();

    free(nbytes);
}

/* This test sends 32KB of data, and drops packets with probability p=0.025 */
Test(unreliable_data_transfer, random_drop_025_3, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 2.0)
{
    int *nbytes = malloc(sizeof(int));

    *nbytes = 32768;
    drop_percentage = 0.025;
    seed = 12100;

    chitcp_tester_client_run_set(tester, sender, nbytes);
    chitcp_tester_server_run_set(tester, receiver, nbytes);

    chitcp_tester_server_set_debug(tester, drop_random_packets,
    DBG_EVT_PENDING_CONNECTION | DBG_EVT_INCOMING_PACKET);

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_run();

    tester_done();

    free(nbytes);
}

/* This test sends 32KB of data, and drops packets with probability p=0.05 */
Test(unreliable_data_transfer, random_drop_05_1, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 4.0)
{
    int *nbytes = malloc(sizeof(int));

    *nbytes = 32768;
    drop_percentage = 0.05;
    seed = 23300;

    chitcp_tester_client_run_set(tester, sender, nbytes);
    chitcp_tester_server_run_set(tester, receiver, nbytes);

    chitcp_tester_server_set_debug(tester, drop_random_packets,
    DBG_EVT_PENDING_CONNECTION | DBG_EVT_INCOMING_PACKET);

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_run();

    tester_done();

    free(nbytes);
}

/* This test sends 32KB of data, and drops packets with probability p=0.05 */
Test(unreliable_data_transfer, random_drop_05_2, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 4.0)
{
    int *nbytes = malloc(sizeof(int));

    *nbytes = 32768;
    drop_percentage = 0.05;
    seed = 23310;

    chitcp_tester_client_run_set(tester, sender, nbytes);
    chitcp_tester_server_run_set(tester, receiver, nbytes);

    chitcp_tester_server_set_debug(tester, drop_random_packets,
    DBG_EVT_PENDING_CONNECTION | DBG_EVT_INCOMING_PACKET);

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_run();

    tester_done();

    free(nbytes);
}

/* This test sends 32KB of data, and drops packets with probability p=0.05 */
Test(unreliable_data_transfer, random_drop_05_3, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 4.0)
{
    int *nbytes = malloc(sizeof(int));

    *nbytes = 32768;
    drop_percentage = 0.05;
    seed = 12100;

    chitcp_tester_client_run_set(tester, sender, nbytes);
    chitcp_tester_server_run_set(tester, receiver, nbytes);

    chitcp_tester_server_set_debug(tester, drop_random_packets,
    DBG_EVT_PENDING_CONNECTION | DBG_EVT_INCOMING_PACKET);

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_run();

    tester_done();

    free(nbytes);
}

/* This test sends 32KB of data, and drops packets with probability p=0.10 */
Test(unreliable_data_transfer, random_drop_10_1, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 8.0)
{
    int *nbytes = malloc(sizeof(int));

    *nbytes = 32768;
    drop_percentage = 0.10;
    seed = 23300;

    chitcp_tester_client_run_set(tester, sender, nbytes);
    chitcp_tester_server_run_set(tester, receiver, nbytes);

    chitcp_tester_server_set_debug(tester, drop_random_packets,
    DBG_EVT_PENDING_CONNECTION | DBG_EVT_INCOMING_PACKET);

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_run();

    tester_done();

    free(nbytes);
}

/* This test sends 32KB of data, and drops packets with probability p=0.10 */
Test(unreliable_data_transfer, random_drop_10_2, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 8.0)
{
    int *nbytes = malloc(sizeof(int));

    *nbytes = 32768;
    drop_percentage = 0.10;
    seed = 23310;

    chitcp_tester_client_run_set(tester, sender, nbytes);
    chitcp_tester_server_run_set(tester, receiver, nbytes);

    chitcp_tester_server_set_debug(tester, drop_random_packets,
    DBG_EVT_PENDING_CONNECTION | DBG_EVT_INCOMING_PACKET);

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_run();

    tester_done();

    free(nbytes);
}

/* This test sends 32KB of data, and drops packets with probability p=0.10 */
Test(unreliable_data_transfer, random_drop_10_3, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 8.0)
{
    int *nbytes = malloc(sizeof(int));

    *nbytes = 32768;
    drop_percentage = 0.10;
    seed = 12100;

    chitcp_tester_client_run_set(tester, sender, nbytes);
    chitcp_tester_server_run_set(tester, receiver, nbytes);

    chitcp_tester_server_set_debug(tester, drop_random_packets,
    DBG_EVT_PENDING_CONNECTION | DBG_EVT_INCOMING_PACKET);

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_run();

    tester_done();

    free(nbytes);
}


/* This test sends 64KB of data, and drops packets with probability p=0.25 */
Test(unreliable_data_transfer, random_drop_25, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 20.0)
{
    int *nbytes = malloc(sizeof(int));

    *nbytes = 65536;
    drop_percentage = 0.25;
    seed = 23300;

    chitcp_tester_client_run_set(tester, sender, nbytes);
    chitcp_tester_server_run_set(tester, receiver, nbytes);

    chitcp_tester_server_set_debug(tester, drop_random_packets,
                                   DBG_EVT_PENDING_CONNECTION | DBG_EVT_INCOMING_PACKET);

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_run();

    tester_done();

    free(nbytes);
}



/* This test results in five packets being sent, but with the first one arriving
 * out of order (after the other four packets). The receiving socket should be
 * able to reassemble these packets without any retransmissions (if out-of-order
 * delivery is not implemented, then all five packets will time out) */
Test(unreliable_out_of_order, out_of_order_1, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 0.5)
{
    int *nbytes = malloc(sizeof(int));

    *nbytes = 2680; /* MSS * 5 to ensure five data packets are sent. */
    packet_id = 0;
    packet_sequence_size = 5;

    /* Packet should arrive in the sequence: 2, 3, 4, 1, 5 */
    packet_sequence[0] = DBG_RESP_WITHHOLD;
    packet_sequence[1] = DBG_RESP_NONE;
    packet_sequence[2] = DBG_RESP_NONE;
    packet_sequence[3] = DBG_RESP_NONE;
    packet_sequence[4] = DBG_RESP_DRAW_WITHHELD;

    chitcp_tester_client_run_set(tester, sender, nbytes);
    chitcp_tester_server_run_set(tester, receiver, nbytes);

    chitcp_tester_server_set_debug(tester, out_of_order_handler,
    DBG_EVT_PENDING_CONNECTION | DBG_EVT_INCOMING_PACKET);

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_run();

    tester_done();

    free(nbytes);
}


/* This test results in four packets being sent, but with two packets out of order.
 * The packets arrive in this order: 3, 1, 4, 2 */
Test(unreliable_out_of_order, out_of_order_2, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 0.5)
{
    int *nbytes = malloc(sizeof(int));

    *nbytes = 2144;
    packet_id = 0;

    /* The packets should arrive as follows: 3, 1, 4, 2 */
    packet_sequence_size = 4;
    packet_sequence[0] = DBG_RESP_WITHHOLD;
    packet_sequence[1] = DBG_RESP_WITHHOLD;
    packet_sequence[2] = DBG_RESP_DRAW_WITHHELD;
    packet_sequence[3] = DBG_RESP_DRAW_WITHHELD;

    chitcp_tester_client_run_set(tester, sender, nbytes);
    chitcp_tester_server_run_set(tester, receiver, nbytes);

    chitcp_tester_server_set_debug(tester, out_of_order_handler,
    DBG_EVT_PENDING_CONNECTION | DBG_EVT_INCOMING_PACKET);

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_run();

    tester_done();

    free(nbytes);
}


/* This test results in eight packets being sent, with two of the packets arriving out
 * of order. When the first out-of-order packet arrives, it should only be possible
 * to process packets 1 through 4, while packets 6 and 7 should remain in the out
 * of order list until packet 5 arrives.
 *
 * The packets arrive in this order: 2, 3, 5, 6, 1, 7, 4 */
Test(unreliable_out_of_order, out_of_order_3, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 0.5)
{
    int *nbytes = malloc(sizeof(int));

    *nbytes = 3752;
    packet_id = 0;

    packet_sequence_size = 7;
    packet_sequence[0] = DBG_RESP_WITHHOLD;
    packet_sequence[1] = DBG_RESP_NONE;
    packet_sequence[2] = DBG_RESP_NONE;
    packet_sequence[3] = DBG_RESP_WITHHOLD;
    packet_sequence[4] = DBG_RESP_NONE;
    packet_sequence[5] = DBG_RESP_DRAW_WITHHELD;
    packet_sequence[6] = DBG_RESP_DRAW_WITHHELD;

    chitcp_tester_client_run_set(tester, sender, nbytes);
    chitcp_tester_server_run_set(tester, receiver, nbytes);

    chitcp_tester_server_set_debug(tester, out_of_order_handler,
    DBG_EVT_PENDING_CONNECTION | DBG_EVT_INCOMING_PACKET);

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_run();

    tester_done();

    free(nbytes);
}


/* This test results in eight packets being sent, with the first one being delayed
 * until the very end (i.e., packets arrive in order 2,3,4,5,6,7,8,1).
 *
 * Unlike the other out-of-order tests, this test results in the TCP window being full.
 * The reason is that we are trying to send a total of 4288 bytes, but the window is
 * just 4096 bytes. In previous tests, this was not an issue because we acknowledged packets
 * as they arrived, which kept the window fairly open. However, because the first packet is
 * delayed, the sender will not receive any acknowledgements, and will have to keep
 * all the packets in the send buffer. This means the 8th packet must have only 344 bytes
 * (to avoid overflowing the window). Once the 1st packet arrives, and the out-of-order
 * packets can be reassembled, the sender will receive acknowledgements (that will open
 * up the window) and will send the remaining 192 bytes in a ninth packet.
 *
 * This test should result in no retransmissions. If you open the libpcap file in Wireshark,
 * it should show a "TCP Window Full" packet.
 */
Test(unreliable_out_of_order, full_window_1, .init = chitcpd_and_tester_setup, .fini = chitcpd_and_tester_teardown, .timeout = 0.5)
{
    int *nbytes = malloc(sizeof(int));

    *nbytes = 4288;
    packet_id = 0;

    packet_sequence_size = 8;
    packet_sequence[0] = DBG_RESP_WITHHOLD;
    packet_sequence[1] = DBG_RESP_NONE;
    packet_sequence[2] = DBG_RESP_NONE;
    packet_sequence[3] = DBG_RESP_NONE;
    packet_sequence[4] = DBG_RESP_NONE;
    packet_sequence[5] = DBG_RESP_NONE;
    packet_sequence[6] = DBG_RESP_NONE;
    packet_sequence[7] = DBG_RESP_DRAW_WITHHELD;

    chitcp_tester_client_run_set(tester, sender, nbytes);
    chitcp_tester_server_run_set(tester, receiver, nbytes);

    chitcp_tester_server_set_debug(tester, out_of_order_handler,
    DBG_EVT_PENDING_CONNECTION | DBG_EVT_INCOMING_PACKET);

    tester_connect();

    chitcp_tester_client_wait_for_state(tester, ESTABLISHED);
    chitcp_tester_server_wait_for_state(tester, ESTABLISHED);

    tester_run();

    tester_done();

    free(nbytes);
}

