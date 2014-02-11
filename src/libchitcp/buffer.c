/*
 *  chiTCP - A simple, testable TCP stack
 *
 *  Functions to utilize circular buffer structure
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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "chitcp/buffer.h"

int circular_buffer_init(circular_buffer_t *buf, uint32_t maxsize)
{
    buf->start = 0;
    buf->end = 0;
    buf->seq_initial = 0;
    buf->seq_start = 0;
    buf->seq_end = 0;
    buf->maxsize = maxsize;

    return CHITCP_OK;
}

int circular_buffer_set_seq_initial(circular_buffer_t *buf, uint32_t seq_initial)
{
    buf->seq_initial = seq_initial;
    buf->seq_start = seq_initial;
    buf->seq_end = seq_initial + buf->end;

    return CHITCP_OK;
}


int circular_buffer_write(circular_buffer_t *buf, uint8_t *data, uint32_t len, bool_t blocking)
{
    memcpy(buf->data + buf->end, data, len);
    buf->end += len;
    buf->seq_end += len;

    return len;
}


int circular_buffer_read(circular_buffer_t *buf, uint8_t *dst, uint32_t len, bool_t blocking)
{
    int toread;

    if(len < (buf->end - buf->start))
        toread = len;
    else
        toread = (buf->end - buf->start);

    if(dst)
        memcpy(dst, buf->data + buf->start, len);
    buf->start += toread;
    buf->seq_start += toread;

    return toread;
}

int circular_buffer_peek(circular_buffer_t *buf, uint8_t *dst, uint32_t len, bool_t blocking)
{
    int toread;

    if(len < (buf->end - buf->start))
        toread = len;
    else
        toread = (buf->end - buf->start);

    if(dst)
        memcpy(dst, buf->data + buf->start, len);

    return toread;
}

int circular_buffer_first(circular_buffer_t *buf)
{
    return buf->seq_start;
}

int circular_buffer_next(circular_buffer_t *buf)
{
    return buf->seq_end;
}

int circular_buffer_capacity(circular_buffer_t *buf)
{
    return buf->maxsize;
}

int circular_buffer_count(circular_buffer_t *buf)
{
    return buf->end - buf->start;
}

int circular_buffer_available(circular_buffer_t *buf)
{
    return buf->maxsize - (buf->end - buf->start);
}

int circular_buffer_dump(circular_buffer_t *buf)
{
    printf("# # # # # # # # # # # # # # # # #\n");

    printf("maxsize: %i\n", buf->maxsize);
    printf("count: %i\n", buf->end - buf->start);

    printf("start: %i\n", buf->start);
    printf("end: %i\n", buf->end);

    for(int i=0; i<buf->maxsize; i++)
    {
        printf("data[%i] = %i", i, buf->data[i]);
        if(i==buf->start)
            printf("  <<< START");
        if(i==buf->end)
            printf("  <<< END");
        printf("\n");
    }
    printf(" # # # # # # # # # # # # # # # # #\n");

    return CHITCP_OK;
}

int circular_buffer_close(circular_buffer_t *buf)
{
    return CHITCP_OK;
}

int circular_buffer_free(circular_buffer_t *buf)
{
    return CHITCP_OK;
}

