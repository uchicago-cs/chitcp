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
    buf->data = malloc(maxsize);
    buf->start = 0;
    buf->end = 0;
    buf->count = 0;
    buf->seq_initial = 0;
    buf->seq_start = 0;
    buf->seq_end = 0;
    buf->maxsize = maxsize;
    buf->closed = FALSE;

    pthread_mutex_init(&buf->lock, NULL);
    pthread_cond_init(&buf->cv_notempty, NULL);
    pthread_cond_init(&buf->cv_notfull, NULL);

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
    if(len <= 0)
        return CHITCP_EINVAL;

    pthread_mutex_lock(&buf->lock);
    if(buf->count + len > buf->maxsize && !blocking)
    {
        pthread_mutex_unlock(&buf->lock);
        return CHITCP_EWOULDBLOCK;
    }

    int written = 0;

    /* We don't allow writes that are larger than the size of the buffer */
    if (len > buf->maxsize)
        len = buf->maxsize;

    while (written < len)
    {
        while(buf->count == buf->maxsize)
            pthread_cond_wait(&buf->cv_notfull, &buf->lock);

        if(buf->closed)
        {
            pthread_mutex_unlock(&buf->lock);
            return written;
        }

        int towrite;

        if (len - written < buf->maxsize - buf->count)
            towrite = len - written;
        else
            towrite = buf->maxsize - buf->count;

        if(buf->end + towrite > buf->maxsize)
        {
            int to_max = buf->maxsize - buf->end;
            int after_max = towrite - to_max;

            memcpy(buf->data + buf->end, data + written, to_max);
            memcpy(buf->data, data + written + to_max, after_max);

            buf->end = after_max;
        }
        else
        {
            memcpy(buf->data + buf->end, data + written, towrite);
            buf->end += towrite;
        }

        written += towrite;
        buf->count += towrite;
        buf->seq_end += towrite;
    }

    pthread_cond_signal(&buf->cv_notempty);
    pthread_mutex_unlock(&buf->lock);

    return written;
}

int __circular_buffer_read(circular_buffer_t *buf, uint8_t *dst, uint32_t len, uint32_t offset, bool_t blocking, bool_t peeking)
{
    if(len <= 0)
        return CHITCP_EINVAL;

    pthread_mutex_lock(&buf->lock);
    if(buf->count == 0 && !blocking)
    {
        pthread_mutex_unlock(&buf->lock);
        return CHITCP_EWOULDBLOCK;
    }

    while(buf->count == 0 && !buf->closed)
        pthread_cond_wait(&buf->cv_notempty, &buf->lock);

    if(buf->closed && buf->count == 0)
    {
        pthread_mutex_unlock(&buf->lock);
        return 0;
    }

    int toread;
    int start = (buf->start + offset) % buf->maxsize;

    /* We're not going to read more than the number
     * of bytes stored in the buffer */
    if(len < (buf->count - offset))
        toread = len;
    else
        toread = buf->count - offset;

    if(start + toread > buf->maxsize)
    {
        int to_max = buf->maxsize - start;
        int after_max = toread - to_max;

        if(dst)
        {
            memcpy(dst, buf->data + start, to_max);
            memcpy(dst+to_max, buf->data, after_max);
        }

        start = after_max;
    }
    else
    {
        if(dst)
            memcpy(dst, buf->data + start, toread);
        start += toread;
    }

    if(!peeking)
    {
        buf->start = start;
        buf->count -= toread;
        buf->seq_start += toread;
    }

    pthread_cond_signal(&buf->cv_notfull);
    pthread_mutex_unlock(&buf->lock);

    return toread;
}

int circular_buffer_read(circular_buffer_t *buf, uint8_t *dst, uint32_t len, bool_t blocking)
{
    return __circular_buffer_read(buf, dst, len, 0, blocking, FALSE);
}

int circular_buffer_peek(circular_buffer_t *buf, uint8_t *dst, uint32_t len, bool_t blocking)
{
    return __circular_buffer_read(buf, dst, len, 0, blocking, TRUE);
}

int circular_buffer_peek_at(circular_buffer_t *buf, uint8_t *dst, uint32_t at, uint32_t len)
{
    uint32_t offset;

    /* Check that the sequence number is valid */
    if (at < buf->seq_start || at >= buf->seq_end)
        return CHITCP_EINVAL;

    offset = at - buf->seq_start;

    return __circular_buffer_read(buf, dst, len, offset, FALSE, TRUE);
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
    return buf->count;
}

int circular_buffer_available(circular_buffer_t *buf)
{
    return buf->maxsize - buf->count;
}

int circular_buffer_dump(circular_buffer_t *buf)
{
    printf("# # # # # # # # # # # # # # # # #\n");

    printf("maxsize: %i\n", buf->maxsize);
    printf("count: %i\n", buf->count);

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
    pthread_mutex_lock(&buf->lock);
    buf->closed = TRUE;
    pthread_cond_broadcast(&buf->cv_notempty);
    pthread_cond_broadcast(&buf->cv_notfull);
    pthread_mutex_unlock(&buf->lock);

    return CHITCP_OK;
}

int circular_buffer_free(circular_buffer_t *buf)
{
    free(buf->data);
    pthread_mutex_destroy(&buf->lock);
    pthread_cond_destroy(&buf->cv_notfull);
    pthread_cond_destroy(&buf->cv_notempty);

    return CHITCP_OK;
}

