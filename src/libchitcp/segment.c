/*
 *  chiTCP - A simple, testable TCP stack
 *
 *  Header and segment type declarations and functions
 *
 *  see chitcp/segment.h for descriptions of functions, parameters, and return values.
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

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <arpa/inet.h>

#include "chitcp/types.h"
#include "chitcp/segment.h"
#include "chitcp/log.h"

/* See segment.h */
uint16_t chitcp_ntohs(uint16_t netshort)
{
    return ntohs(netshort);
}

/* See segment.h */
uint16_t chitcp_htons(uint16_t hostshort)
{
    return htons(hostshort);
}

/* See segment.h */
uint32_t chitcp_ntohl(uint32_t netlong)
{
    return ntohl(netlong);
}

/* See segment.h */
uint32_t chitcp_htonl(uint32_t hostlong)
{
    return htonl(hostlong);
}

/* See segment.h */
int chitcp_tcp_segment_init(tcp_segment_t *segment, const uint8_t* payload, uint16_t payload_len)
{
    tcphdr_t *header;

    segment->length = TCP_HEADER_NOOPTIONS_SIZE + payload_len;
    segment->raw = calloc(segment->length, 1);
    header = (tcphdr_t*) segment->raw;

    // No TCP options
    header->doff = TCP_HEADER_NOOPTIONS_SIZE / sizeof(uint32_t);

    if (payload_len)
        memcpy(segment->raw + TCP_HEADER_NOOPTIONS_SIZE, payload, payload_len);

    return segment->length;
}

/* See segment.h */
void chitcp_tcp_segment_free(tcp_segment_t *segment)
{
    free((void *) segment->raw);
}



/* See segment.h */
int chitcp_segment_list_destroy(tcp_segment_list_t **pl)
{
    tcp_segment_list_t *elt, *tmp;

    DL_FOREACH_SAFE(*pl,elt,tmp)
    {
        free(elt->segment);
        DL_DELETE(*pl,elt);
        free(elt);
    }

    return CHITCP_OK;
}

/* See segment.h */
int chitcp_segment_list_prepend(tcp_segment_list_t **pl, tcp_segment_t *segment)
{
    tcp_segment_list_t *elt = calloc(1, sizeof(tcp_segment_list_t));

    elt->segment = segment;

    DL_PREPEND(*pl, elt);

    return CHITCP_OK;
}

/* See segment.h */
int chitcp_segment_list_append(tcp_segment_list_t **pl, tcp_segment_t *segment)
{
    tcp_segment_list_t *elt = calloc(1, sizeof(tcp_segment_list_t));

    elt->segment = segment;

    DL_APPEND(*pl, elt);

    return CHITCP_OK;
}

/* See segment.h */
int chitcp_segment_list_pop_head(tcp_segment_list_t **pl)
{
    tcp_segment_list_t *head = *pl;

    DL_DELETE(*pl, head);
    free(head);

    return CHITCP_OK;
}

/* See segment.h */
int chitcp_segment_list_size(tcp_segment_list_t *pl)
{
    int count = 0;
    tcp_segment_list_t *elt;

    DL_COUNT(pl, elt, count);

    return count;
}