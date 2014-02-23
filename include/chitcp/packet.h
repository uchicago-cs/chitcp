/*
 *  chiTCP - A simple, testable TCP stack
 *
 *  Header and packet type declarations and functions
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

#ifndef CHITCP_PACKET_H_
#define CHITCP_PACKET_H_

#include <stddef.h>
#include <stdint.h>


/*
 *
 *  ntoh/hton functions
 *
 */

/* These functions are just wrappers around the standard ntoh/hton
 * functions, but namespaced inside chitcp_.
 * Please refer to the manpages for ntohs, htons, ntohl, and htonl
 * for details on these functions. */
uint16_t chitcp_ntohs(uint16_t netshort);
uint16_t chitcp_htons(uint16_t hostshort);
uint32_t chitcp_ntohl(uint32_t netlong);
uint32_t chitcp_htonl(uint32_t hostlong);



/*
 *
 *  TCP Packet
 *
 */

/* struct to contain a single TCP packet */
typedef struct tcp_packet
{
    uint8_t    *raw;
    size_t  length;
} tcp_packet_t;


/*
 * chitcp_tcp_packet_create - Initializes a tcp_packet_t struct with a payload.
 *
 * The TCP header is zeroed out, except for the data offset field, which
 * is initialized to 5 (i.e., TCP header with no options)
 *
 * packet: Pointer to unitialized tcp_packet_t variable.
 *
 * payload: Pointer to payload. The payload will be DEEP COPIED to the packet.
 *
 * payload_len: Size of the payload in number of bytes.
 *
 * Returns: the size in bytes of the TCP packet.
 */
int  chitcp_tcp_packet_create(tcp_packet_t *packet, const uint8_t* payload, uint16_t payload_len);


/*
 * chitcp_tcp_packet_free - Frees up memory allocated for a TCP packet.
 *
 * This function only frees the memory allocated to store the raw contents of
 * the packet (header + payload). It does not free up the tcp_packet_t variable
 * itself. It is the responsibility of the calling function to do so.
 *
 * packet: Pointer to packet.
 *
 * Returns: nothing.
 */
void chitcp_tcp_packet_free(tcp_packet_t *packet);



/*
 *
 *  TCP Header
 *
 */

typedef uint32_t tcp_seq;

/* TCP header */
/* Based on header definition in Stanford's STCP and in
 * the Linux tcp.h header file */
typedef struct tcphdr
{
    uint16_t source;  /* source port */
    uint16_t dest;  /* destination port */
    tcp_seq  seq;    /* sequence number */
    tcp_seq  ack_seq;    /* acknowledgement number */
#if __BYTE_ORDER == __LITTLE_ENDIAN
    uint16_t  res1:4,   /* unused */
              doff:4,  /* data offset */
              fin:1,
              syn:1,
              rst:1,
              psh:1,
              ack:1,
              urg:1,
              ece:1,
              cwr:1;
#elif __BYTE_ORDER == __BIG_ENDIAN
    uint16_t  doff:4,  /* data offset */
              res1:4,   /* unused */
              cwr:1,
              ece:1,
              urg:1,
              ack:1,
              psh:1,
              rst:1,
              syn:1,
              fin:1;
#else
#error __BYTE_ORDER must be defined as __LITTLE_ENDIAN or __BIG_ENDIAN!
#endif
    uint16_t win;    /* window */
    uint16_t sum;    /* checksum */
    uint16_t urp;    /* urgent pointer */
} tcphdr_t;

/* Size in bytes of a TCP header with no options */
#define TCP_HEADER_NOOPTIONS_SIZE (sizeof(tcphdr_t))

/* Returns pointer to header */
#define TCP_PACKET_HEADER(p) ((tcphdr_t*) (p)->raw)

/* Returns pointer to the start of the packet's payload */
#define TCP_PAYLOAD_START(p) ((p)->raw + (((tcphdr_t *) (p)->raw)->doff * sizeof(uint32_t)))

/* Returns length of TCP packet's payload */
#define TCP_PAYLOAD_LEN(p) ((p)->length - (((tcphdr_t *) (p)->raw)->doff * sizeof(uint32_t)))

/* Convenience macros to access the fields in the header,
 * using the nomenclature in RFC 793 */

/* segment sequence number */
#define SEG_SEQ(p) (chitcp_ntohl(TCP_PACKET_HEADER(p)->seq))

/* segment acknowledgment number */
#define SEG_ACK(p) (chitcp_ntohl(TCP_PACKET_HEADER(p)->ack_seq))

/* segment length */
#define SEG_LEN(p) (TCP_PACKET_HEADER(p)->syn + TCP_PACKET_HEADER(p)->fin + TCP_PAYLOAD_LEN(p))

/* segment window */
#define SEG_WND(p) (chitcp_ntohs(TCP_PACKET_HEADER(p)->win))

/* segment urgent pointer */
#define SEG_UP(p) (chitcp_ntohs(TCP_PACKET_HEADER(p)->urp))


/*
 *
 *  chiTCP Header
 *
 */


/* chiTCP header definition */
typedef struct chitcphdr
{
    uint16_t payload_len;   /* Size of payload */
    uint8_t proto;            /* Protocol of payload */
    uint8_t flags;          /* Flags */
    uint32_t reserved1;
    uint32_t reserved2;
    uint32_t reserved3;
} chitcphdr_t;


/* Supported protocols */
typedef enum {
    CHITCP_PROTO_TCP             = 1,
    CHITCP_PROTO_RAW            = 2
} chitcp_proto_t;


#endif /* PACKET_H_ */
