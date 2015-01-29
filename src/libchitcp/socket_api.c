/*
 *  chiTCP - A simple, testable TCP stack
 *
 *  chisocket functions
 *
 *  chiTCP equivalents of many of the POSIX socket
 *  functions (listen, bind, send, recv, etc.).
 *
 *  see chitcp/socket.h for descriptions of functions, parameters, and return values.
 *
 *    Note that, for the most part, these functions just do some basic checks and
 *    then call chitcpd_send_command() to send the command to the chiTCP daemon,
 *    which is the one that does the actual heavy lifting. See chitcpd's handlers.c
 *    file for the actual implementation of these functions.
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h> /* for malloc */
#include <errno.h>

#include "chitcp/socket.h"
#include "chitcp/types.h"
#include "daemon_api.h"

int chisocket_socket(int domain, int type, int protocol)
{
    ChitcpdMsg req = CHITCPD_MSG__INIT;
    ChitcpdSocketArgs sa = CHITCPD_SOCKET_ARGS__INIT;
    ChitcpdMsg *resp_p;
    int ret, error_code;
    int daemon_socket;
    int rc;

    daemon_socket = chitcpd_get_socket();
    if (daemon_socket < 0)
        CHITCPD_FAIL("Error when connecting to chiTCP daemon.");

    /* Create request */
    req.code = CHITCPD_MSG_CODE__SOCKET;
    req.socket_args = &sa;

    sa.domain = domain;
    sa.type = type;
    sa.protocol = protocol;

    rc = chitcpd_send_command(daemon_socket, &req, &resp_p);

    if(rc != CHITCP_OK)
        CHITCPD_FAIL("Error when communicating with chiTCP daemon.");

    /* Unpack response */
    assert(resp_p->resp != NULL);
    ret = resp_p->resp->ret;
    error_code = resp_p->resp->error_code;

    chitcpd_msg__free_unpacked(resp_p, NULL);

    ret = (error_code? -1 : ret);
    if(error_code) errno = error_code;

    return ret;
}

int chisocket_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    ChitcpdMsg req = CHITCPD_MSG__INIT;
    ChitcpdConnectArgs ca = CHITCPD_CONNECT_ARGS__INIT;
    ChitcpdMsg *resp_p;
    int ret, error_code;
    int daemon_socket;
    int rc;
    uint8_t addr_copy [addrlen]; /* for const-correctness */
    memcpy(addr_copy, addr, addrlen);

    daemon_socket = chitcpd_get_socket();
    if (daemon_socket < 0)
        CHITCPD_FAIL("Error when connecting to chiTCP daemon.");

    /* Create request */
    req.code = CHITCPD_MSG_CODE__CONNECT;
    req.connect_args = &ca;

    ca.sockfd = sockfd;
    ca.addr.data = addr_copy;
    ca.addr.len = addrlen;

    rc = chitcpd_send_command(daemon_socket, &req, &resp_p);

    if(rc != CHITCP_OK)
        CHITCPD_FAIL("Error when communicating with chiTCP daemon.");

    /* Unpack response */
    assert(resp_p->resp != NULL);
    ret = resp_p->resp->ret;
    error_code = resp_p->resp->error_code;

    chitcpd_msg__free_unpacked(resp_p, NULL);

    ret = (error_code? -1 : ret);
    if(error_code) errno = error_code;

    return ret;
}

int chisocket_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    ChitcpdMsg req = CHITCPD_MSG__INIT;
    ChitcpdBindArgs ba = CHITCPD_BIND_ARGS__INIT;
    ChitcpdMsg *resp_p;
    int ret, error_code;
    int daemon_socket;
    int rc;
    uint8_t addr_copy [addrlen]; /* for const correctness */
    memcpy(addr_copy, addr, addrlen);

    daemon_socket = chitcpd_get_socket();
    if (daemon_socket < 0)
        CHITCPD_FAIL("Error when connecting to chiTCP daemon.");

    /* Create request */
    req.code = CHITCPD_MSG_CODE__BIND;
    req.bind_args = &ba;

    ba.sockfd = sockfd;
    ba.addr.data = addr_copy;
    ba.addr.len = addrlen;

    rc = chitcpd_send_command(daemon_socket, &req, &resp_p);

    if(rc != CHITCP_OK)
        CHITCPD_FAIL("Error when communicating with chiTCP daemon.");

    /* Unpack response */
    assert(resp_p->resp != NULL);
    ret = resp_p->resp->ret;
    error_code = resp_p->resp->error_code;

    chitcpd_msg__free_unpacked(resp_p, NULL);

    ret = (error_code? -1 : ret);
    if(error_code) errno = error_code;

    return ret;
}

int chisocket_listen(int sockfd, int backlog)
{
    ChitcpdMsg req = CHITCPD_MSG__INIT;
    ChitcpdListenArgs la = CHITCPD_LISTEN_ARGS__INIT;
    ChitcpdMsg *resp_p;
    int daemon_socket;
    int rc, ret, error_code;

    daemon_socket = chitcpd_get_socket();
    if (daemon_socket < 0)
        CHITCPD_FAIL("Error when connecting to chiTCP daemon.");

    /* Create request */
    req.code = CHITCPD_MSG_CODE__LISTEN;
    req.listen_args = &la;

    la.sockfd = sockfd;
    la.backlog = backlog;

    rc = chitcpd_send_command(daemon_socket, &req, &resp_p);

    if(rc != CHITCP_OK)
        CHITCPD_FAIL("Error when communicating with chiTCP daemon.");

    /* Unpack response */
    assert(resp_p->resp != NULL);
    ret = resp_p->resp->ret;
    error_code = resp_p->resp->error_code;

    chitcpd_msg__free_unpacked(resp_p, NULL);

    ret = (error_code? -1 : ret);
    if(error_code) errno = error_code;

    return ret;
}

int chisocket_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    ChitcpdMsg req = CHITCPD_MSG__INIT;
    ChitcpdAcceptArgs aa = CHITCPD_ACCEPT_ARGS__INIT;
    ChitcpdMsg *resp_p;
    int daemon_socket;
    int rc, ret, error_code;
    int len; /* the minimum of ADDRLEN and the return length from chitcpd */

    daemon_socket = chitcpd_get_socket();
    if (daemon_socket < 0)
        CHITCPD_FAIL("Error when connecting to chiTCP daemon.");

    /* Create request */
    req.code = CHITCPD_MSG_CODE__ACCEPT;
    req.accept_args = &aa;

    aa.sockfd = sockfd;
    /* NOTE: we do not send addrlen to chitcpd; if truncation must be
     * performed, it is done below, in this function. */

    rc = chitcpd_send_command(daemon_socket, &req, &resp_p);

    if(rc != CHITCP_OK)
        CHITCPD_FAIL("Error when communicating with chiTCP daemon.");

    /* Unpack response */
    assert(resp_p->resp != NULL);
    ret = resp_p->resp->ret;
    error_code = resp_p->resp->error_code;

    if (!error_code)
    {
        assert(resp_p->resp->has_addr);

        len = (resp_p->resp->addr.len < *addrlen) ? resp_p->resp->addr.len : *addrlen;
        *addrlen = resp_p->resp->addr.len;
        memcpy(addr, resp_p->resp->addr.data, len);
    }

    chitcpd_msg__free_unpacked(resp_p, NULL);

    ret = (error_code? -1 : ret);
    if(error_code) errno = error_code;

    return ret;
}

int chisocket_close(int sockfd)
{
    ChitcpdMsg req = CHITCPD_MSG__INIT;
    ChitcpdCloseArgs ca = CHITCPD_CLOSE_ARGS__INIT;
    ChitcpdMsg *resp_p;
    int daemon_socket;
    int rc, ret, error_code;

    daemon_socket = chitcpd_get_socket();
    if (daemon_socket < 0)
        CHITCPD_FAIL("Error when connecting to chiTCP daemon.");

    req.code = CHITCPD_MSG_CODE__CLOSE;
    req.close_args = &ca;

    ca.sockfd = sockfd;

    rc = chitcpd_send_command(daemon_socket, &req, &resp_p);

    if(rc != CHITCP_OK)
        CHITCPD_FAIL("Error when communicating with chiTCP daemon.");

    /* Unpack response */
    assert(resp_p->resp != NULL);
    ret = resp_p->resp->ret;
    error_code = resp_p->resp->error_code;

    chitcpd_msg__free_unpacked(resp_p, NULL);

    ret = (error_code? -1 : ret);
    if(error_code) errno = error_code;

    return ret;
}

ssize_t chisocket_send(int sockfd, const void *buf, size_t buf_len, int flags)
{
    ChitcpdMsg req = CHITCPD_MSG__INIT;
    ChitcpdSendArgs sa = CHITCPD_SEND_ARGS__INIT;
    ChitcpdMsg *resp_p;
    int daemon_socket;
    int rc, ret, error_code;
    uint8_t *newbuf;

    daemon_socket = chitcpd_get_socket();
    if (daemon_socket < 0)
        CHITCPD_FAIL("Error when connecting to chiTCP daemon.");

    /* Copy the buf for const-correctness
     * (Irritatingly, there seems to be no way to tell the compiler that we
     * won't harm any sub-structures pointed to by the ChitcpdMsg.) */
    newbuf = malloc(buf_len);
    if (!newbuf)
    {
        errno = ENOMEM;
        return -1;
    }

    memcpy(newbuf, buf, buf_len);


    /* Create request */
    req.code = CHITCPD_MSG_CODE__SEND;
    req.send_args = &sa;

    sa.sockfd = sockfd;
    sa.buf.data = newbuf;
    sa.buf.len = buf_len;
    sa.flags = flags;

    rc = chitcpd_send_command(daemon_socket, &req, &resp_p);

    if(rc != CHITCP_OK)
        CHITCPD_FAIL("Error when communicating with chiTCP daemon.");

    /* Unpack response */
    assert(resp_p->resp != NULL);
    ret = resp_p->resp->ret;
    error_code = resp_p->resp->error_code;

    chitcpd_msg__free_unpacked(resp_p, NULL);

    ret = (error_code? -1 : ret);
    if(error_code) errno = error_code;

    free(newbuf);

    return ret;
}

ssize_t chisocket_recv(int sockfd, void *buf, size_t len, int flags)
{
    ChitcpdMsg req = CHITCPD_MSG__INIT;
    ChitcpdRecvArgs ra = CHITCPD_RECV_ARGS__INIT;
    ChitcpdMsg *resp_p;
    int daemon_socket;
    int rc, ret, error_code;

    daemon_socket = chitcpd_get_socket();
    if (daemon_socket < 0)
        CHITCPD_FAIL("Error when connecting to chiTCP daemon.");

    req.code = CHITCPD_MSG_CODE__RECV;
    req.recv_args = &ra;

    ra.sockfd = sockfd;
    ra.flags = flags;
    ra.len = len;

    rc = chitcpd_send_command(daemon_socket, &req, &resp_p);

    if(rc != CHITCP_OK)
        CHITCPD_FAIL("Error when communicating with chiTCP daemon.");

    /* Unpack response */
    assert(resp_p->resp != NULL);
    ret = resp_p->resp->ret;
    error_code = resp_p->resp->error_code;

    if (!error_code && ret != 0)
    {
        assert(resp_p->resp->has_buf
               && resp_p->resp->buf.len == ret
               && ret <= len);
        memcpy(buf, resp_p->resp->buf.data, ret);
    }

    chitcpd_msg__free_unpacked(resp_p, NULL);

    ret = (error_code? -1 : ret);
    if(error_code) errno = error_code;

    return ret;
}
