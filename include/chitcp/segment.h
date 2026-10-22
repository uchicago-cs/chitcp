/*
 *  chiTCP - A simple, testable TCP stack
 *
 *  Header and segment type declarations and functions
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

#ifndef CHITCP_SEGMENT_H_
#define CHITCP_SEGMENT_H_

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>
#include "utlist.h"


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
 *  TCP Segment
 *
 */

/* struct to contain a single TCP segment */
typedef struct tcp_segment
{
    uint8_t    *raw;
    size_t  length;
} tcp_segment_t;


/*
 * chitcp_tcp_segment_init - Initializes a tcp_segment_t struct with a payload.
 *
 * The TCP header is zeroed out, except for the data offset field, which
 * is initialized to 5 (i.e., TCP header with no options)
 *
 * segment: Pointer to unitialized tcp_segment_t variable.
 *
 * payload: Pointer to payload. The payload will be DEEP COPIED to the segment.
 *
 * payload_len: Size of the payload in number of bytes.
 *
 * Returns: the size in bytes of the TCP segment.
 */
int chitcp_tcp_segment_init(tcp_segment_t *segment, const uint8_t* payload, uint16_t payload_len);


/*
 * chitcp_tcp_segment_free - Frees up memory allocated for a TCP segment.
 *
 * This function only frees the memory allocated to store the raw contents of
 * the segment (header + payload). It does not free up the tcp_segment_t variable
 * itself. It is the responsibility of the calling function to do so.
 *
 * segment: Pointer to segment.
 *
 * Returns: nothing.
 */
void chitcp_tcp_segment_free(tcp_segment_t *segment);


/*
 *
 *  List of TCP Segments
 *
 */

/* Lists of TCP segments are manipulated using utlist functions.
 * We provide a few convenience functions for some common operations */

/* struct for linked list of TCP segments */
typedef struct tcp_segment_list
{
    tcp_segment_t *segment;
    struct tcp_segment_list *prev;
    struct tcp_segment_list *next;
} tcp_segment_list_t;


/*
 * chitcp_segment_list_prepend - Adds a segment to the head of a list of segments
 *
 * pl: Pointer to head pointer (this function will update the head pointer so
 *     that it points to the node that has just been added).
 *
 * segment: Pointer to segment to be added.
 *
 * Returns: Always returns CHITCP_OK.
 */
int chitcp_segment_list_prepend(tcp_segment_list_t **pl, tcp_segment_t *segment);


/*
 * chitcp_segment_list_append - Adds a segment to the tail of a list of segments
 *
 * pl: Pointer to head pointer (if the head pointer was initially NULL, this
 *     function will update it to point to the node that was just added).
 *
 * segment: Pointer to segment to be added.
 *
 * Returns: Always returns CHITCP_OK.
 */
int chitcp_segment_list_append(tcp_segment_list_t **pl, tcp_segment_t *segment);


/*
 * chitcp_segment_list_pop_head - Removes the segment at the head of the list.
 *
 * Note: This function doesn't return the segment, it just removes it.
 * The list node is freed, but the segment itself is not. So, you must
 * obtain a pointer to the segment (with (*pl)->segment) *before* calling
 * this function.
 *
 * pl: Pointer to head pointer (head pointer is updated to point to new head,
 *     or set to NULL if the list is empty after the segment is removed).
 *
 * Returns: Always returns CHITCP_OK.
 */
int chitcp_segment_list_pop_head(tcp_segment_list_t **pl);


/*
 * chitcp_segment_list_destroy - Frees the list and segments in it.
 *
 * pl: Pointer to head pointer
 *
 * Returns: Always returns CHITCP_OK.
 */
int chitcp_segment_list_destroy(tcp_segment_list_t **pl);


/*
 * chitcp_segment_list_size - Returns the size of the list
 *
 * pl: Head pointer
 *
 * Returns: Number of segments in the list
 */
int chitcp_segment_list_size(tcp_segment_list_t *pl);


/*
 *
 * Withheld segments
 *
 */

/* Struct to contain a withheld TCP segment */
typedef struct withheld_tcp_segment
{
    tcp_segment_t *segment;
    struct sockaddr_storage local_addr;
    struct sockaddr_storage remote_addr;
    bool duplicate;
    struct withheld_tcp_segment *prev;
    struct withheld_tcp_segment *next;
} withheld_tcp_segment_t;

typedef struct withheld_tcp_segment withheld_tcp_segment_list_t;


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
#define TCP_SEGMENT_HEADER(p) ((tcphdr_t*) (p)->raw)

/* Returns pointer to the start of the segment's payload */
#define TCP_PAYLOAD_START(p) ((p)->raw + (((tcphdr_t *) (p)->raw)->doff * sizeof(uint32_t)))

/* Returns length of TCP segment's payload */
#define TCP_PAYLOAD_LEN(p) ((p)->length - (((tcphdr_t *) (p)->raw)->doff * sizeof(uint32_t)))

/* Convenience macros to access the fields in the header,
 * using the nomenclature in RFC 9293 */

/* segment sequence number */
#define SEG_SEQ(p) (chitcp_ntohl(TCP_SEGMENT_HEADER(p)->seq))

/* segment acknowledgment number */
#define SEG_ACK(p) (chitcp_ntohl(TCP_SEGMENT_HEADER(p)->ack_seq))

/* segment length */
#define SEG_LEN(p) (TCP_SEGMENT_HEADER(p)->syn + TCP_SEGMENT_HEADER(p)->fin + TCP_PAYLOAD_LEN(p))

/* segment window */
#define SEG_WND(p) (chitcp_ntohs(TCP_SEGMENT_HEADER(p)->win))

/* segment urgent pointer */
#define SEG_UP(p) (chitcp_ntohs(TCP_SEGMENT_HEADER(p)->urp))


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


#endif /* SEGMENT_H_ */
