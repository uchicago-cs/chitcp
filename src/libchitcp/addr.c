/*
 *  chiTCP - A simple, testable TCP stack
 *
 *  sockaddr manipulations functions
 *
 *  see chitcp/addr.h for descriptions of functions, parameters, and return values.
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
#include <assert.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>


#include "chitcp/types.h"
#include "chitcp/log.h"
#include "chitcp/packet.h"


in_port_t chitcp_get_addr_port(struct sockaddr *addr)
{
    if (!(addr->sa_family == AF_INET || addr->sa_family == AF_INET6))
        return -1; /* I assume this is an in_port_t that indicates error? */

    if(addr->sa_family == AF_INET)
    {
        struct sockaddr_in *sin = (struct sockaddr_in *) addr;

        return sin->sin_port;
    }
    else if(addr->sa_family == AF_INET6)
    {
        struct sockaddr_in6 *sin = (struct sockaddr_in6 *) addr;

        return sin->sin6_port;
    }

    return 0;
}


void chitcp_set_addr_port(struct sockaddr *addr, in_port_t port)
{
    assert(addr->sa_family == AF_INET || addr->sa_family == AF_INET6);

    if(addr->sa_family == AF_INET)
    {
        struct sockaddr_in *sin = (struct sockaddr_in *) addr;

        sin->sin_port = port;
    }
    else if(addr->sa_family == AF_INET6)
    {
        struct sockaddr_in6 *sin = (struct sockaddr_in6 *) addr;

        sin->sin6_port = port;
    }
}


void* chitcp_get_addr(struct sockaddr *addr)
{
    assert(addr->sa_family == AF_INET || addr->sa_family == AF_INET6);

    if(addr->sa_family == AF_INET)
    {
        struct sockaddr_in *sin = (struct sockaddr_in *) addr;

        return &sin->sin_addr;
    }
    else if(addr->sa_family == AF_INET6)
    {
        struct sockaddr_in6 *sin = (struct sockaddr_in6 *) addr;

        return &sin->sin6_addr;
    }

    return NULL;
}


int chitcp_addr_cmp(struct sockaddr *addr1, struct sockaddr *addr2)
{
    assert(addr1->sa_family == AF_INET || addr1->sa_family == AF_INET6);
    assert(addr2->sa_family == AF_INET || addr2->sa_family == AF_INET6);

    if(addr1->sa_family != addr2->sa_family)
    {
        return addr1->sa_family - addr2->sa_family;
    }
    else if(addr1->sa_family == AF_INET)
    {
        struct sockaddr_in *sin1 = (struct sockaddr_in *) addr1;
        struct sockaddr_in *sin2 = (struct sockaddr_in *) addr2;

        return sin1->sin_addr.s_addr - sin2->sin_addr.s_addr;
    }
    else if(addr1->sa_family == AF_INET6)
    {
        struct sockaddr_in6 *sin1 = (struct sockaddr_in6 *) addr1;
        struct sockaddr_in6 *sin2 = (struct sockaddr_in6 *) addr2;

        return memcmp(&sin1->sin6_addr, &sin2->sin6_addr, sizeof(sin1->sin6_addr));
    }

    return -1;
}


int chitcp_addr_port_cmp(struct sockaddr *addr1, struct sockaddr *addr2)
{
    assert(addr1->sa_family == AF_INET || addr1->sa_family == AF_INET6);
    assert(addr2->sa_family == AF_INET || addr2->sa_family == AF_INET6);

    if(addr1->sa_family != addr2->sa_family)
    {
        return addr1->sa_family - addr2->sa_family;
    }
    else if(addr1->sa_family == AF_INET)
    {
        struct sockaddr_in *sin1 = (struct sockaddr_in *) addr1;
        struct sockaddr_in *sin2 = (struct sockaddr_in *) addr2;

        return sin1->sin_port - sin2->sin_port;
    }
    else if(addr1->sa_family == AF_INET6)
    {
        struct sockaddr_in6 *sin1 = (struct sockaddr_in6 *) addr1;
        struct sockaddr_in6 *sin2 = (struct sockaddr_in6 *) addr2;

        return sin1->sin6_port - sin2->sin6_port;
    }

    return -1;
}


char* chitcp_addr_str(struct sockaddr *addr, char *buf, int len)
{
    char ip[INET6_ADDRSTRLEN];
    uint32_t port;

    inet_ntop(addr->sa_family, chitcp_get_addr(addr), ip, sizeof(ip));
    port = chitcp_get_addr_port(addr);
    port = chitcp_ntohs(port);

    snprintf(buf, len, "%s:%i", ip, port);

    return buf;
}


int chitcp_addr_is_loopback(struct sockaddr *addr)
{
    assert(addr->sa_family == AF_INET || addr->sa_family == AF_INET6);

    if(addr->sa_family == AF_INET)
    {
        struct sockaddr_in *sin = (struct sockaddr_in *) addr;

        return sin->sin_addr.s_addr == htonl(INADDR_LOOPBACK);
    }
    else if(addr->sa_family == AF_INET6)
    {
        struct sockaddr_in6 *sin = (struct sockaddr_in6 *) addr;

        return memcmp(&sin->sin6_addr, &in6addr_loopback, sizeof(in6addr_loopback)) == 0;
    }

    return -1;
}


int chitcp_addr_is_any(struct sockaddr *addr)
{
    assert(addr->sa_family == AF_INET || addr->sa_family == AF_INET6);

    if(addr->sa_family == AF_INET)
    {
        struct sockaddr_in *sin = (struct sockaddr_in *) addr;

        return sin->sin_addr.s_addr == htonl(INADDR_ANY);
    }
    else if(addr->sa_family == AF_INET6)
    {
        struct sockaddr_in6 *sin = (struct sockaddr_in6 *) addr;

        return memcmp(&sin->sin6_addr, &in6addr_any, sizeof(in6addr_any)) == 0;
    }

    return -1;
}


void chitcp_addr_set_any(struct sockaddr *addr)
{
    assert(addr->sa_family == AF_INET || addr->sa_family == AF_INET6);

    if(addr->sa_family == AF_INET)
    {
        struct sockaddr_in *sin = (struct sockaddr_in *) addr;

        sin->sin_addr.s_addr = htonl(INADDR_ANY);
    }
    else if(addr->sa_family == AF_INET6)
    {
        struct sockaddr_in6 *sin = (struct sockaddr_in6 *) addr;

        memcpy(&sin->sin6_addr, &in6addr_any, sizeof(in6addr_any));
    }
}


int chitcp_addr_construct(char *host, char *port, struct sockaddr_in *addr)
{
    struct hostent *he;
    unsigned iport;

    memset(addr, 0, sizeof(struct sockaddr_in));
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = inet_addr(host);
    if (addr->sin_addr.s_addr == -1)
    {
        he = gethostbyname(host);
        if (he == NULL || he->h_addrtype != AF_INET || he->h_length != 4)
        {
            perror("Invalid host");
            return -1;
        }
        memcpy((char *) &addr->sin_addr, he->h_addr, sizeof(addr->sin_addr));
    }
    sscanf(port, "%u", &iport);
    addr->sin_port = htons(iport);

    return 0;
}
