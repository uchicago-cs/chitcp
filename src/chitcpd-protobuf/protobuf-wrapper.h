/*
 *  chiTCP - A simple, testable TCP stack
 *
 *  Functions for sending and receiving messages to and from chitcpd.
 *
 */

/*
 *  Copyright (c) 2013-2014, The University of Chicago
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
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
 *    software without specific prior written permission.
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
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 */


#ifndef PROTOBUF_WRAPPER_H_
#define PROTOBUF_WRAPPER_H_

#include "chitcpd.pb-c.h"

/*
 * chitcpd_send_msg - Serialize and send a message to SOCKFD. If unsuccessful,
 *                    this function automatically closes SOCKFD.
 *
 * sockfd: Message destination
 * msg: Pointer to the message
 *
 * Returns:
 *   0 - Success
 *  -1 - Peer disconnected (sets errno to ECONNRESET)
 *  -2 - Unexpected error
 *
 */
int chitcpd_send_msg(int sockfd, const ChitcpdMsg *msg);

/*
 * chitcpd_recv_msg - Deserialize a message from SOCKFD. If unsuccessful,
 *                    this function automatically closes SOCKFD. The caller
 *                    must later free the message with
 *                    chitcpd_msg__free_unpacked(*msg).
 *
 * sockfd: Message source
 * msg: Location in which to store a pointer to the deserialized message
 *
 * Returns:
 *   0 - Success
 *  -1 - Peer disconnected (sets errno to ECONNRESET)
 *  -2 - Unexpected error
 *
 */
int chitcpd_recv_msg(int sockfd, ChitcpdMsg **msg);

/*
 * chitcpd_send_and_recv_msg - Combines the functionality of chitcpd_send_msg
 *                             and chitcpd_recv_msg.
 *
 * sockfd: Connection to peer
 * req: Pointer to the message to send
 * resp: Location in which to store a pointer to the received message
 *
 * Returns:
 *   0 - Success
 *  -1 - Peer disconnected (sets errno to ECONNRESET)
 *  -2 - Unexpected error
 *
 */
int chitcpd_send_and_recv_msg(int sockfd, const ChitcpdMsg *req, ChitcpdMsg **resp);


#endif /* PROTOBUF_WRAPPER_H */

