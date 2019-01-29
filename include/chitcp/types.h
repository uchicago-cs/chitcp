/*
 *  chiTCP - A simple, testable TCP stack
 *
 *  Data types
 *
 *  Defines several data types used throughout chiTCP.
 *  Typically does not have to be included directly by application.
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

#ifndef CHITCP_TYPES_H
#define CHITCP_TYPES_H

#include <stdint.h>
#include <netinet/in.h>


/* Return codes of internal functions.
 * Note: Socket functions use standard errno code
 * TODO: These are still used somewhat inconsistently throughout the code */
typedef enum {
    CHITCP_OK             =  0,  /* Function completed successfully */
    CHITCP_ENOMEM        = -1,  /* Could not allocate memory */
    CHITCP_ESOCKET         = -2,  /* Could not allocate socket */
    CHITCP_ETHREAD        = -3,  /* Could not create thread */
    CHITCP_ESYNC        = -4,  /* Error while using a synchronization primitive (mutex or condvar) */
    CHITCP_EINIT        = -5,  /* Could not initialize a data structure */
    CHITCP_EWOULDBLOCK  = -6,  /* Requested non-blocking mode on an operation that would block */
    CHITCP_EINVAL       = -7,  /* Invalid parameter value */
    CHITCP_ENOENT       = -8   /* A requested value was not found ("NO ENTity") */
} chitcp_retcode_t;


/* Boolean data type */

typedef int bool_t;

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#define UNIX_PATH_MAX (108)

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))


#define RET_ON_ERROR(f, r)   \
    {                               \
        if ((f) != 0)               \
        {                           \
            return r;               \
        }                           \
    }

#define PROPAGATE_ON_ERROR(f)       \
    {                               \
        int rc = (f);               \
        if (rc != 0)               \
        {                           \
            return rc;               \
        }                           \
    }


#define EXIT_ON_ERROR(f, msg)   \
    {                                  \
        if ((f) != 0)                  \
        {                              \
            fprintf(stderr, "%s: " msg "\n", __func__); \
            exit(-1);                  \
        }                              \
    }


/* Socket types */

typedef int chisocket_t;
typedef int socket_t;

typedef enum
{
    SOCKET_ACTIVE  = 1,
    SOCKET_PASSIVE = 2,
    SOCKET_UNINITIALIZED = 3,
} socket_type_t;


/* TCP states */

typedef enum {
    CLOSED,
    LISTEN,
    SYN_RCVD,
    SYN_SENT,
    ESTABLISHED,
    FIN_WAIT_1,
    FIN_WAIT_2,
    CLOSE_WAIT,
    CLOSING,
    TIME_WAIT,
    LAST_ACK,
} tcp_state_t;

#define IS_VALID_TCP_STATE(x) ((x) >= CLOSED && (x) <= LAST_ACK)

extern const char *tcp_state_names[];

inline const char *tcp_str(tcp_state_t state)
{
    return tcp_state_names[state];
}

#endif
