/*
 *  chiTCP - A simple, testable TCP stack
 *
 *  Functions to connect to the local chiTCP daemon from an application.
 *
 *  Typically won't be used by user applications, which should just use
 *  the functions defined in chitcp/socket.h (those functions, implemented
 *  in socket_api.c, are basically wrappers around the functions defined here).
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

#ifndef DAEMON_API_H_
#define DAEMON_API_H_

#include "protobuf-wrapper.h"

#define CHITCPD_FAIL(msg) { \
    fprintf(stderr, "%s: " msg "\n", __func__); \
    errno = EPROTO; \
    return -1; }

int chitcpd_get_socket();

/*
 * chitcpd_connect - Create a connection to the local chiTCP daemon
 *
 * Returns: The socket descriptor of the connection to the daemon.
 *          CHITCP_ESOCKET if there was an error connection to the daemon.
 */
int chitcpd_connect();

/*
 * chitcpd_send_command - Send a command to the local chiTCP daemon
 *
 * This function will use SOCKFD to send data to
 * the local chiTCP daemon, and assumes that a connection has already
 * been made on that socket.
 *
 * req: The chiTCP daemon request
 *
 * resp_p: Output parameter to return the response from the chiTCP daemon.
 *         The returned pointer should be freed by the caller using
 *         chitcpd_msg__free_unpacked(*resp_p, NULL).
 *
 * Returns:
 *   0 - Success
 *  -1 - Daemon disconnected
 *  -2 - Unexpected error
 *
 */
int chitcpd_send_command(int sockfd, const ChitcpdMsg *req, ChitcpdMsg **resp_p);

#endif /* DAEMON_API_H_ */
