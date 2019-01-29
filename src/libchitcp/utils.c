/*
 *  chiTCP - A simple, testable TCP stack
 *
 *  Helper functions
 *
 *  see chitcp/utils.h for descriptions of functions, parameters, and return values.
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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "chitcp/socket.h"
#include "chitcp/types.h"
#include "chitcp/packet.h"

const char *tcp_str(tcp_state_t state);

const char *tcp_state_names[] =
{
    "CLOSED",
    "LISTEN",
    "SYN_RCVD",
    "SYN_SENT",
    "ESTABLISHED",
    "FIN_WAIT_1",
    "FIN_WAIT_2",
    "CLOSE_WAIT",
    "CLOSING",
    "TIME_WAIT",
    "LAST_ACK"
};


/* See utils.h */
uint16_t cksum (const void *_data, int len)
{
      const uint8_t *data = _data;
      uint32_t sum;

      for (sum = 0;len >= 2; data += 2, len -= 2)
      {
        sum += data[0] << 8 | data[1];
      }

      if (len > 0)
      {
        sum += data[0] << 8;
      }

      while (sum > 0xffff)
      {
        sum = (sum >> 16) + (sum & 0xffff);
      }

      sum = htons (~sum);

      return sum ? sum : 0xffff;
}


int chitcp_socket_send(int socket, const void *buffer, int length)
{
  int nbytes, nleft, nwritten = 0;

  nleft = length;

  while ( nleft > 0 )
  {
      if ( (nbytes = chisocket_send(socket, buffer, nleft, 0)) == -1 )
      {
          perror("chisocket_send failed");
          return -1;
      }
      nleft  -= nbytes;
      buffer += nbytes;
      nwritten += nbytes;
  }
  return nwritten;
}

int chitcp_socket_recv(int socket, void *buffer, int length)
{
    int nrecv;
    char *recv_buf_ptr;

    recv_buf_ptr = buffer;
    nrecv = length;

    while (nrecv != 0)
    {
        int nbytes = chisocket_recv(socket, recv_buf_ptr, nrecv, 0);

        if(nbytes == 0)
        {
            nrecv -= nbytes;
            return length - nrecv;
        }
        else if (nbytes == -1)
        {
            perror("chisocket_recv() failed");
            return -1;
        }
        else
        {
            recv_buf_ptr += nbytes;
            nrecv -= nbytes;
        }
    }

    return length - nrecv;
}

int chitcp_unix_socket(char* buf, int buflen)
{
    char *env;
    env = getenv("CHITCPD_SOCK");

    if (env)
    {
        strncpy(buf, env, buflen);
    }
    else
    {
        char *login;
        login = getlogin();

        if(login)
            snprintf(buf, buflen, "/tmp/chitcpd.socket.%s", login);
        else
            strncpy(buf, "/tmp/chitcpd.socket", buflen);
    }

    return CHITCP_OK;
}

