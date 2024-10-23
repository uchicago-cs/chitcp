/*
 *  chiTCP - A simple, testable TCP stack
 *
 *  Implementation of the TCP protocol.
 *
 *  chiTCP follows a state machine approach to implementing TCP.
 *  This means that there is a handler function for each of
 *  the TCP states (CLOSED, LISTEN, SYN_RCVD, etc.). If an
 *  event (e.g., a packet arrives) while the connection is
 *  in a specific state (e.g., ESTABLISHED), then the handler
 *  function for that state is called, along with information
 *  about the event that just happened.
 *
 *  Each handler function has the following prototype:
 *
 *  int f(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event);
 *
 *  si is a pointer to the chiTCP server info. The functions in
 *       this file will not have to access the data in the server info,
 *       but this pointer is needed to call other functions.
 *
 *  entry is a pointer to the socket entry for the connection that
 *          is being handled. The socket entry contains the actual TCP
 *          data (variables, buffers, etc.), which can be extracted
 *          like this:
 *
 *            tcp_data_t *tcp_data = &entry->socket_state.active.tcp_data;
 *
 *          Other than that, no other fields in "entry" should be read
 *          or modified.
 *
 *  event is the event that has caused the TCP thread to wake up. The
 *          list of possible events corresponds roughly to the ones
 *          specified in https://datatracker.ietf.org/doc/html/rfc9293#section-3.10.
 *          They are:
 *
 *            APPLICATION_CONNECT: Application has called socket_connect()
 *            and a three-way handshake must be initiated.
 *
 *            APPLICATION_SEND: Application has called socket_send() and
 *            there is unsent data in the send buffer.
 *
 *            APPLICATION_RECEIVE: Application has called socket_recv() and
 *            any received-and-acked data in the recv buffer will be
 *            collected by the application (up to the maximum specified
 *            when calling socket_recv).
 *
 *            APPLICATION_CLOSE: Application has called socket_close() and
 *            a connection tear-down should be initiated.
 *
 *            PACKET_ARRIVAL: A packet has arrived through the network and
 *            needs to be processed (RFC 9293 calls this "SEGMENT ARRIVES")
 *
 *            TIMEOUT: A timeout (e.g., a retransmission timeout) has
 *            happened.
 *
 */

/*
 *  Copyright (c) 2013-2014, The University of Chicago
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or withsend
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
 *    software withsend specific prior written permission.
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
 *  ARISING IN ANY WAY send OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "chitcp/log.h"
#include "chitcp/utils.h"
#include "chitcp/buffer.h"
#include "chitcp/chitcpd.h"
#include "serverinfo.h"
#include "connection.h"
#include "tcp.h"
#include <stdlib.h>
#include <string.h>


/* Forward declaration */
int chitcpd_tcp_handle_packet(serverinfo_t *si, chisocketentry_t *entry);


void tcp_data_init(serverinfo_t *si, chisocketentry_t *entry)
{
    tcp_data_t *tcp_data = &entry->socket_state.active.tcp_data;

    tcp_data->pending_packets = NULL;
    pthread_mutex_init(&tcp_data->lock_pending_packets, NULL);
    pthread_cond_init(&tcp_data->cv_pending_packets, NULL);

    /* Initialization of additional tcp_data_t fields,
     * and creation of retransmission thread, goes here */
}

void tcp_data_free(serverinfo_t *si, chisocketentry_t *entry)
{
    tcp_data_t *tcp_data = &entry->socket_state.active.tcp_data;

    circular_buffer_free(&tcp_data->send);
    circular_buffer_free(&tcp_data->recv);
    chitcp_packet_list_destroy(&tcp_data->pending_packets);
    pthread_mutex_destroy(&tcp_data->lock_pending_packets);
    pthread_cond_destroy(&tcp_data->cv_pending_packets);

    /* Cleanup of additional tcp_data_t fields goes here */
}


int chitcpd_tcp_state_handle_CLOSED(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event)
{
    if (event == APPLICATION_CONNECT)
    {
        /* Your code goes here */
    }
    else if (event == CLEANUP)
    {
        /* Any additional cleanup goes here */
    }
    else
        chilog(WARNING, "In CLOSED state, received unexpected event.");

    return CHITCP_OK;
}

int chitcpd_tcp_state_handle_LISTEN(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event)
{
    if (event == PACKET_ARRIVAL)
    {
        chitcpd_tcp_handle_packet(si, entry);
    }
    else
        chilog(WARNING, "In LISTEN state, received unexpected event.");

    return CHITCP_OK;
}

int chitcpd_tcp_state_handle_SYN_RCVD(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event)
{
    if (event == PACKET_ARRIVAL)
    {
        chitcpd_tcp_handle_packet(si, entry);
    }
    else if (event == TIMEOUT_RTX)
    {
    /* Your code goes here */
    }
    else
        chilog(WARNING, "In SYN_RCVD state, received unexpected event.");

    return CHITCP_OK;
}

int chitcpd_tcp_state_handle_SYN_SENT(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event)
{
    if (event == PACKET_ARRIVAL)
    {
        chitcpd_tcp_handle_packet(si, entry);
    }
    else if (event == TIMEOUT_RTX)
    {
    /* Your code goes here */
    }
    else
        chilog(WARNING, "In SYN_SENT state, received unexpected event.");

    return CHITCP_OK;
}

int chitcpd_tcp_state_handle_ESTABLISHED(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event)
{
    if (event == APPLICATION_SEND)
    {
        /* Your code goes here */
    }
    else if (event == PACKET_ARRIVAL)
    {
        chitcpd_tcp_handle_packet(si, entry);
    }
    else if (event == APPLICATION_RECEIVE)
    {
        /* Your code goes here */
    }
    else if (event == APPLICATION_CLOSE)
    {
        /* Your code goes here */
    }
    else if (event == TIMEOUT_RTX)
    {
      /* Your code goes here */
    }
    else if (event == TIMEOUT_PST)
    {
        /* Your code goes here */
    }
    else
        chilog(WARNING, "In ESTABLISHED state, received unexpected event (%i).", event);

    return CHITCP_OK;
}

int chitcpd_tcp_state_handle_FIN_WAIT_1(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event)
{
    if (event == PACKET_ARRIVAL)
    {
        chitcpd_tcp_handle_packet(si, entry);
    }
    else if (event == APPLICATION_RECEIVE)
    {
        /* Your code goes here */
    }
    else if (event == TIMEOUT_RTX)
    {
      /* Your code goes here */
    }
    else if (event == TIMEOUT_PST)
    {
        /* Your code goes here */
    }
    else
       chilog(WARNING, "In FIN_WAIT_1 state, received unexpected event (%i).", event);

    return CHITCP_OK;
}


int chitcpd_tcp_state_handle_FIN_WAIT_2(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event)
{
    if (event == PACKET_ARRIVAL)
    {
        chitcpd_tcp_handle_packet(si, entry);
    }
    else if (event == APPLICATION_RECEIVE)
    {
        /* Your code goes here */
    }
    else if (event == TIMEOUT_RTX)
    {
      /* Your code goes here */
    }
    else
        chilog(WARNING, "In FIN_WAIT_2 state, received unexpected event (%i).", event);

    return CHITCP_OK;
}


int chitcpd_tcp_state_handle_CLOSE_WAIT(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event)
{
    if (event == APPLICATION_CLOSE)
    {
        /* Your code goes here */
    }
    else if (event == PACKET_ARRIVAL)
    {
        chitcpd_tcp_handle_packet(si, entry);
    }
    else if (event == TIMEOUT_RTX)
    {
      /* Your code goes here */
    }
    else if (event == TIMEOUT_PST)
    {
        /* Your code goes here */
    }
    else
       chilog(WARNING, "In CLOSE_WAIT state, received unexpected event (%i).", event);


    return CHITCP_OK;
}


int chitcpd_tcp_state_handle_CLOSING(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event)
{
    if (event == PACKET_ARRIVAL)
    {
        chitcpd_tcp_handle_packet(si, entry);
    }
    else if (event == TIMEOUT_RTX)
    {
      /* Your code goes here */
    }
    else if (event == TIMEOUT_PST)
    {
        /* Your code goes here */
    }
    else
       chilog(WARNING, "In CLOSING state, received unexpected event (%i).", event);

    return CHITCP_OK;
}


int chitcpd_tcp_state_handle_TIME_WAIT(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event)
{
    chilog(WARNING, "Running handler for TIME_WAIT. This should not happen.");

    return CHITCP_OK;
}


int chitcpd_tcp_state_handle_LAST_ACK(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event)
{
    if (event == PACKET_ARRIVAL)
    {
        chitcpd_tcp_handle_packet(si, entry);
    }
    else if (event == TIMEOUT_RTX)
    {
      /* Your code goes here */
    }
    else if (event == TIMEOUT_PST)
    {
        /* Your code goes here */
    }
    else
       chilog(WARNING, "In LAST_ACK state, received unexpected event (%i).", event);

    return CHITCP_OK;
}


/*
 * chitcpd_tcp_handle_packet - Handles the arrival of a packet
 *
 * This function is called any time the PACKET_ARRIVAL event takes
 * place in any state. It must implement section 3.10.7 of RFC 9293.
 * For convenience, we have included the full text of that section
 * inside the function. We recommend interleaving the verbatim
 * text of the RFC with the code that implements each portion
 * of section 3.10.7
 *
 * si: serverinfo struct
 *
 * entry: Socket entry
 *
 * Returns:
 *   - CHITCP_OK: Packet was processed
 *   - CHITCP_ENOENT: No packets available for processing
 *
 */
int chitcpd_tcp_handle_packet(serverinfo_t *si, chisocketentry_t *entry)
{
    tcp_data_t *tcp_data = &entry->socket_state.active.tcp_data;
    tcp_packet_t *packet = NULL;

    /* Extract the packet at the head of the pending packets queue */
    pthread_mutex_lock(&(tcp_data->lock_pending_packets));
    if(tcp_data->pending_packets)
    {
        packet = tcp_data->pending_packets->packet;
        chitcp_packet_list_pop_head(&tcp_data->pending_packets);
    }
    pthread_mutex_unlock(&(tcp_data->lock_pending_packets));

    if (packet == NULL)
    {
        chilog(WARNING, "No pending packets found.");
        return CHITCP_ENOENT;
    }

    chilog(DEBUG, "Processing TCP packet");
    chilog_tcp(DEBUG, packet, LOG_INBOUND);

    /* Implement section 3.10.7 of RFC 9293 below */

    // 3.10.7.  SEGMENT ARRIVES
    //
    // 3.10.7.1.  CLOSED STATE
    //
    //    If the state is CLOSED (i.e., TCB does not exist), then
    //
    //       all data in the incoming segment is discarded.  An incoming
    //       segment containing a RST is discarded.  An incoming segment not
    //       containing a RST causes a RST to be sent in response.  The
    //       acknowledgment and sequence field values are selected to make the
    //       reset sequence acceptable to the TCP endpoint that sent the
    //       offending segment.
    //
    //       If the ACK bit is off, sequence number zero is used,
    //
    //          <SEQ=0><ACK=SEG.SEQ+SEG.LEN><CTL=RST,ACK>
    //
    //       If the ACK bit is on,
    //
    //          <SEQ=SEG.ACK><CTL=RST>
    //
    //       Return.
    //
    // 3.10.7.2.  LISTEN STATE
    //
    //    If the state is LISTEN, then
    //
    //       First, check for a RST:
    //
    //       -  An incoming RST segment could not be valid since it could not
    //          have been sent in response to anything sent by this incarnation
    //          of the connection.  An incoming RST should be ignored.  Return.
    //
    //       Second, check for an ACK:
    //
    //       -  Any acknowledgment is bad if it arrives on a connection still
    //          in the LISTEN state.  An acceptable reset segment should be
    //          formed for any arriving ACK-bearing segment.  The RST should be
    //          formatted as follows:
    //
    //             <SEQ=SEG.ACK><CTL=RST>
    //
    //       -  Return.
    //
    //       Third, check for a SYN:
    //
    //       -  If the SYN bit is set, check the security.  If the security/
    //          compartment on the incoming segment does not exactly match the
    //          security/compartment in the TCB, then send a reset and return.
    //
    //             <SEQ=0><ACK=SEG.SEQ+SEG.LEN><CTL=RST,ACK>
    //
    //       -  Set RCV.NXT to SEG.SEQ+1, IRS is set to SEG.SEQ, and any other
    //          control or text should be queued for processing later.  ISS
    //          should be selected and a SYN segment sent of the form:
    //
    //             <SEQ=ISS><ACK=RCV.NXT><CTL=SYN,ACK>
    //
    //       -  SND.NXT is set to ISS+1 and SND.UNA to ISS.  The connection
    //          state should be changed to SYN-RECEIVED.  Note that any other
    //          incoming control or data (combined with SYN) will be processed
    //          in the SYN-RECEIVED state, but processing of SYN and ACK should
    //          not be repeated.  If the listen was not fully specified (i.e.,
    //          the remote socket was not fully specified), then the
    //          unspecified fields should be filled in now.
    //
    //       Fourth, other data or control:
    //
    //       -  This should not be reached.  Drop the segment and return.  Any
    //          other control or data-bearing segment (not containing SYN) must
    //          have an ACK and thus would have been discarded by the ACK
    //          processing in the second step, unless it was first discarded by
    //          RST checking in the first step.
    //
    // 3.10.7.3.  SYN-SENT STATE
    //
    //    If the state is SYN-SENT, then
    //
    //       First, check the ACK bit:
    //
    //       -  If the ACK bit is set,
    //
    //          o  If SEG.ACK =< ISS or SEG.ACK > SND.NXT, send a reset (unless
    //             the RST bit is set, if so drop the segment and return)
    //
    //                <SEQ=SEG.ACK><CTL=RST>
    //
    //          o  and discard the segment.  Return.
    //
    //          o  If SND.UNA < SEG.ACK =< SND.NXT, then the ACK is acceptable.
    //             Some deployed TCP code has used the check SEG.ACK == SND.NXT
    //             (using "==" rather than "=<"), but this is not appropriate
    //             when the stack is capable of sending data on the SYN because
    //             the TCP peer may not accept and acknowledge all of the data
    //             on the SYN.
    //
    //       Second, check the RST bit:
    //
    //       -  If the RST bit is set,
    //
    //          o  A potential blind reset attack is described in RFC 5961 [9].
    //             The mitigation described in that document has specific
    //             applicability explained therein, and is not a substitute for
    //             cryptographic protection (e.g., IPsec or TCP-AO).  A TCP
    //             implementation that supports the mitigation described in RFC
    //             5961 SHOULD first check that the sequence number exactly
    //             matches RCV.NXT prior to executing the action in the next
    //             paragraph.
    //
    //          o  If the ACK was acceptable, then signal to the user "error:
    //             connection reset", drop the segment, enter CLOSED state,
    //             delete TCB, and return.  Otherwise (no ACK), drop the
    //             segment and return.
    //
    //       Third, check the security:
    //
    //       -  If the security/compartment in the segment does not exactly
    //          match the security/compartment in the TCB, send a reset:
    //
    //          o  If there is an ACK,
    //
    //                <SEQ=SEG.ACK><CTL=RST>
    //
    //          o  Otherwise,
    //
    //                <SEQ=0><ACK=SEG.SEQ+SEG.LEN><CTL=RST,ACK>
    //
    //       -  If a reset was sent, discard the segment and return.
    //
    //       Fourth, check the SYN bit:
    //
    //       -  This step should be reached only if the ACK is ok, or there is
    //          no ACK, and the segment did not contain a RST.
    //
    //       -  If the SYN bit is on and the security/compartment is
    //          acceptable, then RCV.NXT is set to SEG.SEQ+1, IRS is set to
    //          SEG.SEQ.  SND.UNA should be advanced to equal SEG.ACK (if there
    //          is an ACK), and any segments on the retransmission queue that
    //          are thereby acknowledged should be removed.
    //
    //       -  If SND.UNA > ISS (our SYN has been ACKed), change the
    //          connection state to ESTABLISHED, form an ACK segment
    //
    //             <SEQ=SND.NXT><ACK=RCV.NXT><CTL=ACK>
    //
    //       -  and send it.  Data or controls that were queued for
    //          transmission MAY be included.  Some TCP implementations
    //          suppress sending this segment when the received segment
    //          contains data that will anyways generate an acknowledgment in
    //          the later processing steps, saving this extra acknowledgment of
    //          the SYN from being sent.  If there are other controls or text
    //          in the segment, then continue processing at the sixth step
    //          under Section 3.10.7.4 where the URG bit is checked; otherwise,
    //          return.
    //
    //       -  Otherwise, enter SYN-RECEIVED, form a SYN,ACK segment
    //
    //             <SEQ=ISS><ACK=RCV.NXT><CTL=SYN,ACK>
    //
    //       -  and send it.  Set the variables:
    //
    //             SND.WND <- SEG.WND
    //
    //             SND.WL1 <- SEG.SEQ
    //
    //             SND.WL2 <- SEG.ACK
    //
    //          If there are other controls or text in the segment, queue them
    //          for processing after the ESTABLISHED state has been reached,
    //          return.
    //
    //       -  Note that it is legal to send and receive application data on
    //          SYN segments (this is the "text in the segment" mentioned
    //          above).  There has been significant misinformation and
    //          misunderstanding of this topic historically.  Some firewalls
    //          and security devices consider this suspicious.  However, the
    //          capability was used in T/TCP [21] and is used in TCP Fast Open
    //          (TFO) [48], so is important for implementations and network
    //          devices to permit.
    //
    //       Fifth, if neither of the SYN or RST bits is set, then drop the
    //       segment and return.
    //
    // 3.10.7.4.  Other States
    //
    //    Otherwise,
    //
    //       First, check sequence number:
    //
    //       -  SYN-RECEIVED STATE
    //
    //       -  ESTABLISHED STATE
    //
    //       -  FIN-WAIT-1 STATE
    //
    //       -  FIN-WAIT-2 STATE
    //
    //       -  CLOSE-WAIT STATE
    //
    //       -  CLOSING STATE
    //
    //       -  LAST-ACK STATE
    //
    //       -  TIME-WAIT STATE
    //
    //          o  Segments are processed in sequence.  Initial tests on
    //             arrival are used to discard old duplicates, but further
    //             processing is done in SEG.SEQ order.  If a segment's
    //             contents straddle the boundary between old and new, only the
    //             new parts are processed.
    //
    //          o  In general, the processing of received segments MUST be
    //             implemented to aggregate ACK segments whenever possible
    //             (MUST-58).  For example, if the TCP endpoint is processing a
    //             series of queued segments, it MUST process them all before
    //             sending any ACK segments (MUST-59).
    //
    //          o  There are four cases for the acceptability test for an
    //             incoming segment:
    //
    //             +=========+=========+======================================+
    //             | Segment | Receive | Test                                 |
    //             | Length  | Window  |                                      |
    //             +=========+=========+======================================+
    //             | 0       | 0       | SEG.SEQ = RCV.NXT                    |
    //             +---------+---------+--------------------------------------+
    //             | 0       | >0      | RCV.NXT =< SEG.SEQ <                 |
    //             |         |         | RCV.NXT+RCV.WND                      |
    //             +---------+---------+--------------------------------------+
    //             | >0      | 0       | not acceptable                       |
    //             +---------+---------+--------------------------------------+
    //             | >0      | >0      | RCV.NXT =< SEG.SEQ <                 |
    //             |         |         | RCV.NXT+RCV.WND                      |
    //             |         |         |                                      |
    //             |         |         | or                                   |
    //             |         |         |                                      |
    //             |         |         | RCV.NXT =< SEG.SEQ+SEG.LEN-1         |
    //             |         |         | < RCV.NXT+RCV.WND                    |
    //             +---------+---------+--------------------------------------+
    //
    //                         Table 6: Segment Acceptability Tests
    //
    //          o  In implementing sequence number validation as described
    //             here, please note Appendix A.2.
    //
    //          o  If the RCV.WND is zero, no segments will be acceptable, but
    //             special allowance should be made to accept valid ACKs, URGs,
    //             and RSTs.
    //
    //          o  If an incoming segment is not acceptable, an acknowledgment
    //             should be sent in reply (unless the RST bit is set, if so
    //             drop the segment and return):
    //
    //             <SEQ=SND.NXT><ACK=RCV.NXT><CTL=ACK>
    //
    //          o  After sending the acknowledgment, drop the unacceptable
    //             segment and return.
    //
    //          o  Note that for the TIME-WAIT state, there is an improved
    //             algorithm described in [40] for handling incoming SYN
    //             segments that utilizes timestamps rather than relying on the
    //             sequence number check described here.  When the improved
    //             algorithm is implemented, the logic above is not applicable
    //             for incoming SYN segments with Timestamp Options, received
    //             on a connection in the TIME-WAIT state.
    //
    //          o  In the following it is assumed that the segment is the
    //             idealized segment that begins at RCV.NXT and does not exceed
    //             the window.  One could tailor actual segments to fit this
    //             assumption by trimming off any portions that lie outside the
    //             window (including SYN and FIN) and only processing further
    //             if the segment then begins at RCV.NXT.  Segments with higher
    //             beginning sequence numbers SHOULD be held for later
    //             processing (SHLD-31).
    //
    //       Second, check the RST bit:
    //
    //       -  RFC 5961 [9], Section 3 describes a potential blind reset
    //          attack and optional mitigation approach.  This does not provide
    //          a cryptographic protection (e.g., as in IPsec or TCP-AO) but
    //          can be applicable in situations described in RFC 5961.  For
    //          stacks implementing the protection described in RFC 5961, the
    //          three checks below apply; otherwise, processing for these
    //          states is indicated further below.
    //
    //          1)  If the RST bit is set and the sequence number is outside
    //              the current receive window, silently drop the segment.
    //
    //          2)  If the RST bit is set and the sequence number exactly
    //              matches the next expected sequence number (RCV.NXT), then
    //              TCP endpoints MUST reset the connection in the manner
    //              prescribed below according to the connection state.
    //
    //          3)  If the RST bit is set and the sequence number does not
    //              exactly match the next expected sequence value, yet is
    //              within the current receive window, TCP endpoints MUST send
    //              an acknowledgment (challenge ACK):
    //
    //              <SEQ=SND.NXT><ACK=RCV.NXT><CTL=ACK>
    //
    //              After sending the challenge ACK, TCP endpoints MUST drop
    //              the unacceptable segment and stop processing the incoming
    //              packet further.  Note that RFC 5961 and Errata ID 4772 [99]
    //              contain additional considerations for ACK throttling in an
    //              implementation.
    //
    //       -  SYN-RECEIVED STATE
    //
    //          o  If the RST bit is set,
    //
    //             +  If this connection was initiated with a passive OPEN
    //                (i.e., came from the LISTEN state), then return this
    //                connection to LISTEN state and return.  The user need not
    //                be informed.  If this connection was initiated with an
    //                active OPEN (i.e., came from SYN-SENT state), then the
    //                connection was refused; signal the user "connection
    //                refused".  In either case, the retransmission queue
    //                should be flushed.  And in the active OPEN case, enter
    //                the CLOSED state and delete the TCB, and return.
    //
    //       -  ESTABLISHED STATE
    //
    //       -  FIN-WAIT-1 STATE
    //
    //       -  FIN-WAIT-2 STATE
    //
    //       -  CLOSE-WAIT STATE
    //
    //          o  If the RST bit is set, then any outstanding RECEIVEs and
    //             SEND should receive "reset" responses.  All segment queues
    //             should be flushed.  Users should also receive an unsolicited
    //             general "connection reset" signal.  Enter the CLOSED state,
    //             delete the TCB, and return.
    //
    //       -  CLOSING STATE
    //
    //       -  LAST-ACK STATE
    //
    //       -  TIME-WAIT STATE
    //
    //          o  If the RST bit is set, then enter the CLOSED state, delete
    //             the TCB, and return.
    //
    //       Third, check security:
    //
    //       -  SYN-RECEIVED STATE
    //
    //          o  If the security/compartment in the segment does not exactly
    //             match the security/compartment in the TCB, then send a reset
    //             and return.
    //
    //       -  ESTABLISHED STATE
    //
    //       -  FIN-WAIT-1 STATE
    //
    //       -  FIN-WAIT-2 STATE
    //
    //       -  CLOSE-WAIT STATE
    //
    //       -  CLOSING STATE
    //
    //       -  LAST-ACK STATE
    //
    //       -  TIME-WAIT STATE
    //
    //          o  If the security/compartment in the segment does not exactly
    //             match the security/compartment in the TCB, then send a
    //             reset; any outstanding RECEIVEs and SEND should receive
    //             "reset" responses.  All segment queues should be flushed.
    //             Users should also receive an unsolicited general "connection
    //             reset" signal.  Enter the CLOSED state, delete the TCB, and
    //             return.
    //
    //       -  Note this check is placed following the sequence check to
    //          prevent a segment from an old connection between these port
    //          numbers with a different security from causing an abort of the
    //          current connection.
    //
    //       Fourth, check the SYN bit:
    //
    //       -  SYN-RECEIVED STATE
    //
    //          o  If the connection was initiated with a passive OPEN, then
    //             return this connection to the LISTEN state and return.
    //             Otherwise, handle per the directions for synchronized states
    //             below.
    //
    //       -  ESTABLISHED STATE
    //
    //       -  FIN-WAIT-1 STATE
    //
    //       -  FIN-WAIT-2 STATE
    //
    //       -  CLOSE-WAIT STATE
    //
    //       -  CLOSING STATE
    //
    //       -  LAST-ACK STATE
    //
    //       -  TIME-WAIT STATE
    //
    //          o  If the SYN bit is set in these synchronized states, it may
    //             be either a legitimate new connection attempt (e.g., in the
    //             case of TIME-WAIT), an error where the connection should be
    //             reset, or the result of an attack attempt, as described in
    //             RFC 5961 [9].  For the TIME-WAIT state, new connections can
    //             be accepted if the Timestamp Option is used and meets
    //             expectations (per [40]).  For all other cases, RFC 5961
    //             provides a mitigation with applicability to some situations,
    //             though there are also alternatives that offer cryptographic
    //             protection (see Section 7).  RFC 5961 recommends that in
    //             these synchronized states, if the SYN bit is set,
    //             irrespective of the sequence number, TCP endpoints MUST send
    //             a "challenge ACK" to the remote peer:
    //
    //             <SEQ=SND.NXT><ACK=RCV.NXT><CTL=ACK>
    //
    //          o  After sending the acknowledgment, TCP implementations MUST
    //             drop the unacceptable segment and stop processing further.
    //             Note that RFC 5961 and Errata ID 4772 [99] contain
    //             additional ACK throttling notes for an implementation.
    //
    //          o  For implementations that do not follow RFC 5961, the
    //             original behavior described in RFC 793 follows in this
    //             paragraph.  If the SYN is in the window it is an error: send
    //             a reset, any outstanding RECEIVEs and SEND should receive
    //             "reset" responses, all segment queues should be flushed, the
    //             user should also receive an unsolicited general "connection
    //             reset" signal, enter the CLOSED state, delete the TCB, and
    //             return.
    //
    //          o  If the SYN is not in the window, this step would not be
    //             reached and an ACK would have been sent in the first step
    //             (sequence number check).
    //
    //       Fifth, check the ACK field:
    //
    //       -  if the ACK bit is off, drop the segment and return
    //
    //       -  if the ACK bit is on,
    //
    //          o  RFC 5961 [9], Section 5 describes a potential blind data
    //             injection attack, and mitigation that implementations MAY
    //             choose to include (MAY-12).  TCP stacks that implement RFC
    //             5961 MUST add an input check that the ACK value is
    //             acceptable only if it is in the range of ((SND.UNA -
    //             MAX.SND.WND) =< SEG.ACK =< SND.NXT).  All incoming segments
    //             whose ACK value doesn't satisfy the above condition MUST be
    //             discarded and an ACK sent back.  The new state variable
    //             MAX.SND.WND is defined as the largest window that the local
    //             sender has ever received from its peer (subject to window
    //             scaling) or may be hard-coded to a maximum permissible
    //             window value.  When the ACK value is acceptable, the per-
    //             state processing below applies:
    //
    //          o  SYN-RECEIVED STATE
    //
    //             +  If SND.UNA < SEG.ACK =< SND.NXT, then enter ESTABLISHED
    //                state and continue processing with the variables below
    //                set to:
    //
    //                   SND.WND <- SEG.WND
    //
    //                   SND.WL1 <- SEG.SEQ
    //
    //                   SND.WL2 <- SEG.ACK
    //
    //             +  If the segment acknowledgment is not acceptable, form a
    //                reset segment
    //
    //                   <SEQ=SEG.ACK><CTL=RST>
    //
    //             +  and send it.
    //
    //          o  ESTABLISHED STATE
    //
    //             +  If SND.UNA < SEG.ACK =< SND.NXT, then set SND.UNA <-
    //                SEG.ACK.  Any segments on the retransmission queue that
    //                are thereby entirely acknowledged are removed.  Users
    //                should receive positive acknowledgments for buffers that
    //                have been SENT and fully acknowledged (i.e., SEND buffer
    //                should be returned with "ok" response).  If the ACK is a
    //                duplicate (SEG.ACK =< SND.UNA), it can be ignored.  If
    //                the ACK acks something not yet sent (SEG.ACK > SND.NXT),
    //                then send an ACK, drop the segment, and return.
    //
    //             +  If SND.UNA =< SEG.ACK =< SND.NXT, the send window should
    //                be updated.  If (SND.WL1 < SEG.SEQ or (SND.WL1 = SEG.SEQ
    //                and SND.WL2 =< SEG.ACK)), set SND.WND <- SEG.WND, set
    //                SND.WL1 <- SEG.SEQ, and set SND.WL2 <- SEG.ACK.
    //
    //             +  Note that SND.WND is an offset from SND.UNA, that SND.WL1
    //                records the sequence number of the last segment used to
    //                update SND.WND, and that SND.WL2 records the
    //                acknowledgment number of the last segment used to update
    //                SND.WND.  The check here prevents using old segments to
    //                update the window.
    //
    //          o  FIN-WAIT-1 STATE
    //
    //             +  In addition to the processing for the ESTABLISHED state,
    //                if the FIN segment is now acknowledged, then enter FIN-
    //                WAIT-2 and continue processing in that state.
    //
    //          o  FIN-WAIT-2 STATE
    //
    //             +  In addition to the processing for the ESTABLISHED state,
    //                if the retransmission queue is empty, the user's CLOSE
    //                can be acknowledged ("ok") but do not delete the TCB.
    //
    //          o  CLOSE-WAIT STATE
    //
    //             +  Do the same processing as for the ESTABLISHED state.
    //
    //          o  CLOSING STATE
    //
    //             +  In addition to the processing for the ESTABLISHED state,
    //                if the ACK acknowledges our FIN, then enter the TIME-WAIT
    //                state; otherwise, ignore the segment.
    //
    //          o  LAST-ACK STATE
    //
    //             +  The only thing that can arrive in this state is an
    //                acknowledgment of our FIN.  If our FIN is now
    //                acknowledged, delete the TCB, enter the CLOSED state, and
    //                return.
    //
    //          o  TIME-WAIT STATE
    //
    //             +  The only thing that can arrive in this state is a
    //                retransmission of the remote FIN.  Acknowledge it, and
    //                restart the 2 MSL timeout.
    //
    //       Sixth, check the URG bit:
    //
    //       -  ESTABLISHED STATE
    //
    //       -  FIN-WAIT-1 STATE
    //
    //       -  FIN-WAIT-2 STATE
    //
    //          o  If the URG bit is set, RCV.UP <- max(RCV.UP,SEG.UP), and
    //             signal the user that the remote side has urgent data if the
    //             urgent pointer (RCV.UP) is in advance of the data consumed.
    //             If the user has already been signaled (or is still in the
    //             "urgent mode") for this continuous sequence of urgent data,
    //             do not signal the user again.
    //
    //       -  CLOSE-WAIT STATE
    //
    //       -  CLOSING STATE
    //
    //       -  LAST-ACK STATE
    //
    //       -  TIME-WAIT STATE
    //
    //          o  This should not occur since a FIN has been received from the
    //             remote side.  Ignore the URG.
    //
    //       Seventh, process the segment text:
    //
    //       -  ESTABLISHED STATE
    //
    //       -  FIN-WAIT-1 STATE
    //
    //       -  FIN-WAIT-2 STATE
    //
    //          o  Once in the ESTABLISHED state, it is possible to deliver
    //             segment data to user RECEIVE buffers.  Data from segments
    //             can be moved into buffers until either the buffer is full or
    //             the segment is empty.  If the segment empties and carries a
    //             PUSH flag, then the user is informed, when the buffer is
    //             returned, that a PUSH has been received.
    //
    //          o  When the TCP endpoint takes responsibility for delivering
    //             the data to the user, it must also acknowledge the receipt
    //             of the data.
    //
    //          o  Once the TCP endpoint takes responsibility for the data, it
    //             advances RCV.NXT over the data accepted, and adjusts RCV.WND
    //             as appropriate to the current buffer availability.  The
    //             total of RCV.NXT and RCV.WND should not be reduced.
    //
    //          o  A TCP implementation MAY send an ACK segment acknowledging
    //             RCV.NXT when a valid segment arrives that is in the window
    //             but not at the left window edge (MAY-13).
    //
    //          o  Please note the window management suggestions in
    //             Section 3.8.
    //
    //          o  Send an acknowledgment of the form:
    //
    //             <SEQ=SND.NXT><ACK=RCV.NXT><CTL=ACK>
    //
    //          o  This acknowledgment should be piggybacked on a segment being
    //             transmitted if possible without incurring undue delay.
    //
    //       -  CLOSE-WAIT STATE
    //
    //       -  CLOSING STATE
    //
    //       -  LAST-ACK STATE
    //
    //       -  TIME-WAIT STATE
    //
    //          o  This should not occur since a FIN has been received from the
    //             remote side.  Ignore the segment text.
    //
    //       Eighth, check the FIN bit:
    //
    //       -  Do not process the FIN if the state is CLOSED, LISTEN, or SYN-
    //          SENT since the SEG.SEQ cannot be validated; drop the segment
    //          and return.
    //
    //       -  If the FIN bit is set, signal the user "connection closing" and
    //          return any pending RECEIVEs with same message, advance RCV.NXT
    //          over the FIN, and send an acknowledgment for the FIN.  Note
    //          that FIN implies PUSH for any segment text not yet delivered to
    //          the user.
    //
    //          o  SYN-RECEIVED STATE
    //
    //          o  ESTABLISHED STATE
    //
    //             +  Enter the CLOSE-WAIT state.
    //
    //          o  FIN-WAIT-1 STATE
    //
    //             +  If our FIN has been ACKed (perhaps in this segment), then
    //                enter TIME-WAIT, start the time-wait timer, turn off the
    //                other timers; otherwise, enter the CLOSING state.
    //
    //          o  FIN-WAIT-2 STATE
    //
    //             +  Enter the TIME-WAIT state.  Start the time-wait timer,
    //                turn off the other timers.
    //
    //          o  CLOSE-WAIT STATE
    //
    //             +  Remain in the CLOSE-WAIT state.
    //
    //          o  CLOSING STATE
    //
    //             +  Remain in the CLOSING state.
    //
    //          o  LAST-ACK STATE
    //
    //             +  Remain in the LAST-ACK state.
    //
    //          o  TIME-WAIT STATE
    //
    //             +  Remain in the TIME-WAIT state.  Restart the 2 MSL time-
    //                wait timeout.
    //
    //       and return.



    return CHITCP_OK;
}


/*                                                           */
/*     Any additional functions you need should go here      */
/*                                                           */
