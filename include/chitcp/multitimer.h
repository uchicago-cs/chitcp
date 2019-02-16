/*
 *  chiTCP - A simple, testable TCP stack
 *
 *  An API for managing multiple timers
 *
 */

/*
 *  Copyright (c) 2013-2019, The University of Chicago
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

#ifndef MULTITIMER_H_
#define MULTITIMER_H_

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include "chitcp/types.h"
#include "chitcp/utlist.h"
#include "chitcp/log.h"

#define MAX_TIMER_NAME_LEN (16)
#define SECOND      (1000000000L)
#define MILLISECOND (1000000L)
#define MICROSECOND (1000L)
#define NANOSECOND  (1L)


/* Forward declarations */
typedef struct single_timer single_timer_t;
typedef struct multi_timer multi_timer_t;

/* Function pointer typedef for timer callback function
 *
 * This callback function takes three parameters:
 * - The multitimer the timer belongs to
 * - The timer that timed out
 * - The parameters to the callback function (can be NULL to indicate
 *   there are no parameters)
 *
 * The callback function does not return anything. */
typedef void (*mt_callback_func)(multi_timer_t*, single_timer_t*, void*);

/* Represents a single timer. */
typedef struct single_timer
{
    /* DO NOT REMOVE OR RENAME ANY OF THESE FIELDS */

    /* Timer ID */
    uint16_t id;

    /* Name of the timer.
     * This field is used *only* for debugging purposes,
     * and should not be used to identify the timer.*/
    char name[MAX_TIMER_NAME_LEN + 1];

    /* Is the timer active? This field should be set to
     * true whenever the timer is waiting for a timeout
     * (and false otherwise) */
    bool active;

    /* How many times has this timer timed out? */
    uint64_t num_timeouts;
} single_timer_t;


/* A multitimer */
typedef struct multi_timer
{
    /* Add fields here */
} multi_timer_t;


/*
 * mt_init - Initializes the multitimer
 *
 * Creates a new multitimer with one or more timers.
 * The timers will be numbered 0, 1, 2, ..., num_timers-1
 *
 * mt: A pointer to enough memory for a multitimer_t struct
 *
 * num_timers: The number of timers
 *
 * Returns:
 *  - CHITCP_OK: multitimer created successfully
 *  - CHITCP_ENOMEM: Could not allocate memory for multitimer
 *  - CHITCP_EINIT: Could not initialize some part of the multitimer
 *  - CHITCP_ETHREAD: Could not create multitimer thread
 */
int mt_init(multi_timer_t *mt, uint16_t num_timers);


/*
 * mt_free - Frees the multitimer
 *
 * Stops the multitimer thread and frees all resources
 * used by the multitimer
 *
 * mt: Multitimer
 *
 * Returns:
 *  - CHITCP_OK: multitimer freed successfully
 */
int mt_free(multi_timer_t *mt);

/*
 * mt_get_timer_by_id - Retrieves a timer with a given identifier
 *
 * mt: Multitimer
 *
 * id: Identifier of the timer to get
 *
 * timer: (Output parameter) Must point to a single_timer_t*. The value
 *        of that single_timer_t* will be set to point to the timer
 *        with identified "id". Note that you should not create
 *        a copy of the timer.
 *
 * Returns:
 *  - CHITCP_OK: timer fetched correctly
 *  - CHITCP_EINVAL: Invalid timer identifier
 *
 */
int mt_get_timer_by_id(multi_timer_t *mt, uint16_t id, single_timer_t **timer);


/* mt_set_timer - Sets a timer
 *
 * When a timer is set, it will expire after some given time
 * (unless it is reset or cancelled in the interim). When it
 * does expire, a callback function will be called.
 *
 * mt: Multitimer
 *
 * id: Identifier of the timer to set
 *
 * timeout: Timeout in nanoseconds. The timer will expire
 *          after this number of nanoseconds have passed.
 *
 * callback: A callback function (see the mt_callback_func
 *           declaration for details on this function)
 *
 * callback_args: Parameters to the callback function (can be NULL)
 *
  * Returns:
 *  - CHITCP_OK: timer set correctly
 *  - CHITCP_EINVAL: Invalid timer identifier, or specified a timer
 *                   that is already active
 */
int mt_set_timer(multi_timer_t *mt, uint16_t id, uint64_t timeout, mt_callback_func callback, void* callback_args);


/* mt_cancel_timer - Sets a timer
 *
 * Cancels an active timer so it will no longer expire.
 *
 * mt: Multitimer
 *
 * id: Identifier of the timer to cancel
 *
 * Returns:
 *  - CHITCP_OK: timer set correctly
 *  - CHITCP_EINVAL: Invalid timer identifier, or specified a timer
 *                   that is not active
 */
int mt_cancel_timer(multi_timer_t *mt, uint16_t id);


/* mt_set_timer_name - Sets the name of a timer
 *
 * mt: Multitimer
 *
 * id: Identifier of the timer
 *
 * name: Name to set for the timer
 *
 * Returns:
 *  - CHITCP_OK: timer set correctly
 *  - CHITCP_EINVAL: Invalid timer identifier, or specified a timer
 *                   that is not active
 */
int mt_set_timer_name(multi_timer_t *mt, uint16_t id, const char *name);


/* mt_chilog - Prints all the timers in a multitimer using chilog
 *
 * level: chilog log level
 *
 * mt: Multitimer
 *
 * active_only: If true, print only the timers that are active
 *
 * Returns: Always returns CHITCP_OK
 */
int mt_chilog(loglevel_t level, multi_timer_t *mt, bool active_only);


/* timespec_subtract - Subtracts two timespecs
 *
 * Helper function adapted from https://www.gnu.org/software/libc/manual/html_node/Elapsed-Time.html
 *
 * Subtract timespec Y from timespec X (i.e., perform X-Y),
 * storing the result in RESULT.
 *
 * x, y: timespecs to subtract
 *
 * result: Output parameter
 *
 * Returns: 1 if the difference is negative, otherwise 0.
 */
int timespec_subtract(struct timespec *result, struct timespec *x, struct timespec *y);

#endif /* MULTITIMER_H_ */
