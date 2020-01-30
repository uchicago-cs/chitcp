/*
 *  chiTCP - A simple, testable TCP stack
 *
 *  A circular buffer data structure
 *
 *  This module provides a circular buffer data structure used for
 *  the TCP send and receive buffers. This buffer has the following
 *  characteristics:
 *
 *  - When the buffer is created, it is created with a specific
 *    capacity, and that capacity cannot be extended later on.
 *
 *  - The buffer is used to store a stream of bytes, with a known
 *    initial sequence number. For example, if the initial sequence
 *    number is 23300, the 0th position of the buffer contains
 *    byte 23300, the 1st position contains byte 23301, the 2nd
 *    position contains 23302, etc.
 *
 *    This also means that data is written at the end of the
 *    stream of bytes, and read from the start of the stream.
 *    For example, imagine we have an 8-byte buffer, and that the
 *    initial sequence number is 23300. The empty buffer would
 *    look like this:
 *
 *      0 : [   ]
 *      1 : [   ]
 *      2 : [   ]
 *      3 : [   ]
 *      4 : [   ]
 *      5 : [   ]
 *      6 : [   ]
 *      7 : [   ]
 *
 *    Now lets say we write three bytes (starting at sequence number
 *    23300):
 *
 *      0 : [ Byte 23300 ]
 *      1 : [ Byte 23301 ]
 *      2 : [ Byte 23302 ]
 *      3 : [   ]
 *      4 : [   ]
 *      5 : [   ]
 *      6 : [   ]
 *      7 : [   ]
 *
 *    And then write a further three bytes:
 *
 *      0 : [ Byte 23300 ]
 *      1 : [ Byte 23301 ]
 *      2 : [ Byte 23302 ]
 *      3 : [ Byte 23303 ]
 *      4 : [ Byte 23304 ]
 *      5 : [ Byte 23305 ]
 *      6 : [   ]
 *      7 : [   ]
 *
 *    So, data is always written after the *last* byte written
 *    to the buffer. You cannot write to arbitrary positions of the buffer.
 *
 *    Similarly, you cannot read arbitrary positions of the buffer;
 *    instead, reading from the buffer is always done from start
 *    of the stream of bytes. So, if we read four bytes from the buffer,
 *    we would extract Bytes 23300-23303 from the buffer, which would
 *    be left in this state:
 *
 *      0 : [   ]
 *      1 : [   ]
 *      2 : [   ]
 *      3 : [   ]
 *      4 : [ Byte 23304 ]
 *      5 : [ Byte 23305 ]
 *      6 : [   ]
 *      7 : [   ]
 *
 *
 *  - It is a circular buffer, with the end of the buffer connecting to
 *    the start of the buffer. If we wrote four bytes to the above buffer,
 *    we would have this:
 *
 *      0 : [ Byte 23308 ]
 *      1 : [ Byte 23309 ]
 *      2 : [   ]
 *      3 : [   ]
 *      4 : [ Byte 23304 ]
 *      5 : [ Byte 23305 ]
 *      6 : [ Byte 23306 ]
 *      7 : [ Byte 23307 ]
 *
 *    Note that this wrapping around doesn't mean you can overwrite data
 *    at the start of the data stream. In the above buffer, you would only
 *    be able to write an additional two bytes.
 *
 *  - It is a blocking buffer. If you want to write N bytes to the buffer
 *    (assuming that N is less than or equal to the maximum capacity of
 *    the buffer), the call to circular_buffer_write will block until
 *    it is possible to write all the data to the buffer. For example,
 *    if we wanted to write four bytes to the above buffer, we would
 *    block until (at least) bytes 23304 and 23305 had been read from
 *    the buffer.
 *
 *    Similarly, reading on an empty buffer will block until there is
 *    data to read from the buffer.
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
#include "chitcp/types.h"

#ifndef BUFFER_H_
#define BUFFER_H_

#define BUFFER_NONBLOCKING (0)
#define BUFFER_BLOCKING (1)

#include <pthread.h>

typedef struct circular_buffer
{
    uint8_t *data;

    uint32_t seq_initial;
    uint32_t seq_start;
    uint32_t seq_end;

    uint32_t start;
    uint32_t end;

    uint32_t count;

    bool_t closed;

    pthread_mutex_t lock;
    pthread_cond_t cv_notfull;
    pthread_cond_t cv_notempty;

    uint32_t maxsize;
} circular_buffer_t;


/*
 * circular_buffer_init - Initializes the buffer
 *
 * Creates an empty buffer.
 *
 * buf: circular_buffer_t struct
 *
 * maxsize: The maximum capacity of the buffer
 *
 * Returns:
 *  - CHITCP_OK: Buffer created correctly
 *  - CHITCP_ENOMEM: Could not allocate memory for buffer
 *
 */
int circular_buffer_init(circular_buffer_t *buf, uint32_t maxsize);


/*
 * circular_buffer_set_seq_initial - Set the initial sequence number
 *
 * buf: circular_buffer_t struct
 *
 * seq_initial: Initial sequence number
 *
 * Returns:
 *  - CHITCP_OK: Initial sequence number set correctly
 *
 */
int circular_buffer_set_seq_initial(circular_buffer_t *buf, uint32_t seq_initial);


/*
 * circular_buffer_read - Read data from the buffer
 *
 * Reads (and removes) at most "len" bytes from the
 * buffer, and stores them in "dst". If the buffer has
 * less that "len" bytes in it, those bytes are
 * returned immediately (the function does *not* block
 * until "len" bytes are available on the buffer)
 *
 * If the buffer is empty, and "blocking" is true,
 * the function will block until there is data in
 * the buffer. If the buffer is empty and "blocking"
 * is false, CHITCP_EWOULDBLOCK is returned.
 *
 * If the buffer is closed (using circular_buffer_close)
 * while the function is blocked, the function returns
 * zero immediately.
 *
 * buf: circular_buffer_t struct
 *
 * dst: Array to store data in.
 *
 * len: Maximum number of bytes to read.
 *
 * blocking: True if the function should block, False otherwise.
 *
 * Returns:
 *  - Number of bytes read, or
 *
 *  - 0 if the buffer was closed.
 *
 *  - CHITCP_EWOULDBLOCK: Non-blocking read requested, but function
 *    would have to block.
 *
 */
int circular_buffer_read(circular_buffer_t *buf, uint8_t *dst, uint32_t len, bool_t blocking);


/*
 * circular_buffer_peek - Peek data from the buffer
 *
 * Same as circular_buffer_read, but without removing the
 * data from the buffer.
 *
 */
int circular_buffer_peek(circular_buffer_t *buf, uint8_t *dst, uint32_t len, bool_t blocking);


/*
 * circular_buffer_peek_at - Peek data from the buffer starting at a specific
 *                           sequence number
 *
 * Same as circular_buffer_peek, but instead of reading starting at the first
 * available byte, it instead reads starting at a given sequence number "at", and
 * up to a maximum of "len" bytes.
 *
 * If the function is called with an invalid sequence number "at", it will
 * return CHITCP_EINVAL
 *
 * This is always a non-blocking call: Calling this function on an empty
 * buffer is treated as providing an invalid sequence number (and also
 * returns CHITCP_EINVAL)
 *
 */
int circular_buffer_peek_at(circular_buffer_t *buf, uint8_t *dst, uint32_t at, uint32_t len);

/*
 * circular_buffer_write - Write data into the buffer
 *
 * Writes from "dst" into the buffer. The number of bytes
 * written will be the maximum of "len" and the
 * maximum capacity of the buffer.
 *
 * If the buffer does not have enough space to complete
 * the operation, and "blocking" is true, it will write
 * as many bytes as necessary to fill the buffer and will
 * then block until space becomes available. If "blocking"
 * is false, CHITCP_EWOULDBLOCK is returned.
 *
 * If the buffer is closed (using circular_buffer_close)
 * while the function is blocked, the function returns
 * immediately (and returns the number of bytes written
 * before the buffer was closed).
 *
 * buf: circular_buffer_t struct
 *
 * dst: Array containing data to write.
 *
 * len: Maximum number of bytes to write.
 *
 * blocking: True if the function should block, False otherwise.
 *
 * Returns:
 *  - Number of bytes written
 *
 */
int circular_buffer_write(circular_buffer_t *buf, uint8_t *data, uint32_t len, bool_t blocking);


/*
 * circular_buffer_first - Get sequence number of first unread byte
 *
 * buf: circular_buffer_t struct
 *
 * Returns:
 *  - Sequence number
 *
 */
int circular_buffer_first(circular_buffer_t *buf);


/*
 * circular_buffer_next - Get sequence number of next available byte
 *
 * buf: circular_buffer_t struct
 *
 * Returns:
 *  - Sequence number
 *
 */
int circular_buffer_next(circular_buffer_t *buf);


/*
 * circular_buffer_capacity - Get maximum capacity of buffer
 *
 * buf: circular_buffer_t struct
 *
 * Returns:
 *  - Maximum capacity of buffer
 *
 */
int circular_buffer_capacity(circular_buffer_t *buf);


/*
 * circular_buffer_count - Get number of bytes in the buffer
 *
 * buf: circular_buffer_t struct
 *
 * Returns:
 *  - Number of bytes
 *
 */
int circular_buffer_count(circular_buffer_t *buf);

/*
 * circular_buffer_available - Get number of available bytes in buffer
 *
 * buf: circular_buffer_t struct
 *
 * Returns:
 *  - Maximum capacity of buffer
 *
 */
int circular_buffer_available(circular_buffer_t *buf);


/*
 * circular_buffer_close - Close the buffer
 *
 * When the buffer is closed, it will not accept any
 * more writes. All pending writes return immediately
 * (with circular_buffer_write returning the number
 * of bytes written before the buffer was closed)
 * and all pending reads return immediately (with
 * circular_buffer_read returning zero).
 *
 * buf: circular_buffer_t struct
 *
 * Returns:
 *  - CHITCP_OK: Buffer closed successfully
 *
 */
int circular_buffer_close(circular_buffer_t *buf);


/*
 * circular_buffer_free - Free buffer's resources
 *
 * buf: circular_buffer_t struct
 *
 * Returns:
 *  - CHITCP_OK: Resources freed successfully
 *
 */
int circular_buffer_free(circular_buffer_t *buf);


/*
 * circular_buffer_dump - Print the contents of the buffer
 *
 * buf: circular_buffer_t struct
 *
 * Returns:
 *  - CHITCP_OK: Contents printed successfully
 *
 */
int circular_buffer_dump(circular_buffer_t *buf);

#endif /* BUFFER_H_ */
