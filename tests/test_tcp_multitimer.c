#include "chitcp/multitimer.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <criterion/criterion.h>
#include "fixtures.h"

#define NUM_TIMERS (10)
#define TIMER_IDX (5)

#define USLEEP_MILLISECOND (1000)
#define TIMER_DIFF_TOLERANCE (0.02)

/* Null callback function. Does nothing. */
void null_callback(multi_timer_t *mt, single_timer_t *timer, void *args)
{

}

/* Timing callback. Takes an array of timespecs, and set
 * item idx to the current time. */
struct callback_args
{
    struct timespec *timeouts;
    uint8_t idx;
};

void timing_callback(multi_timer_t *mt, single_timer_t *timer, void *args)
{
    struct callback_args *cargs = args;

    clock_gettime(CLOCK_REALTIME, &cargs->timeouts[cargs->idx]);
}



/* Create a multitimer with a single timer */
Test(multitimer, create_single_timer, .init = log_setup)
{
    multi_timer_t *mt = calloc(1, sizeof(multi_timer_t));
    int rc;

    rc = mt_init(mt, 1);
    cr_assert_eq(rc, CHITCP_OK);
}


/* Create and then destroy a multitimer with a single timer */
Test(multitimer, create_and_destroy_single_timer, .init = log_setup)
{
    multi_timer_t mt;
    int rc;

    rc = mt_init(&mt, 1);
    cr_assert_eq(rc, CHITCP_OK);

    rc = mt_free(&mt);
    cr_assert_eq(rc, CHITCP_OK);
}


/* Create a multitimer with multiple timers */
Test(multitimer, create_multiple_timers, .init = log_setup)
{
    multi_timer_t *mt = calloc(1, sizeof(multi_timer_t));
    int rc;

    rc = mt_init(mt, NUM_TIMERS);
    cr_assert_eq(rc, CHITCP_OK);
}


/* Create and destroy a multitimer with multiple timers */
Test(multitimer, create_and_destroy_multiple_timers, .init = log_setup)
{
    multi_timer_t mt;
    int rc;

    rc = mt_init(&mt, NUM_TIMERS);
    cr_assert_eq(rc, CHITCP_OK);

    rc = mt_free(&mt);
    cr_assert_eq(rc, CHITCP_OK);
}


/* Tests get_timer_by_id */
Test(multitimer, get_timer_by_id, .init = log_setup)
{
    multi_timer_t mt;
    single_timer_t *timer;
    int rc;

    rc = mt_init(&mt, NUM_TIMERS);
    cr_assert_eq(rc, CHITCP_OK);

    for(uint16_t i=0; i < NUM_TIMERS; i++)
    {
        rc = mt_get_timer_by_id(&mt, i, &timer);
        cr_assert_eq(rc, CHITCP_OK);

        cr_assert_eq(timer->id, i);
        cr_assert_eq(timer->active, false);
    }

    rc = mt_free(&mt);
    cr_assert_eq(rc, CHITCP_OK);
}


/* Tests that get_timer_by_id works correctly when given an invalid id */
Test(multitimer, get_timer_by_id_invalid_id, .init = log_setup)
{
    multi_timer_t mt;
    single_timer_t *timer;
    int rc;

    rc = mt_init(&mt, NUM_TIMERS);
    cr_assert_eq(rc, CHITCP_OK);

    rc = mt_get_timer_by_id(&mt, NUM_TIMERS, &timer);
    cr_assert_eq(rc, CHITCP_EINVAL);

    rc = mt_get_timer_by_id(&mt, NUM_TIMERS+1, &timer);
    cr_assert_eq(rc, CHITCP_EINVAL);

    rc = mt_free(&mt);
    cr_assert_eq(rc, CHITCP_OK);
}


/* Sets a single timer with a null callback and tests that it fired correctly */
Test(multitimer, set_single_timer_null_callback, .init = log_setup)
{
    multi_timer_t mt;
    single_timer_t *timer;
    int rc;

    rc = mt_init(&mt, NUM_TIMERS);
    cr_assert_eq(rc, CHITCP_OK);

    rc = mt_set_timer(&mt, TIMER_IDX, 50*MILLISECOND, null_callback, NULL);
    cr_assert_eq(rc, CHITCP_OK);

    usleep(100*USLEEP_MILLISECOND);

    rc = mt_get_timer_by_id(&mt, 5, &timer);
    cr_assert_eq(rc, CHITCP_OK);
    cr_assert_eq(timer->active, false);
    cr_assert_eq(timer->num_timeouts, 1);

    rc = mt_free(&mt);
    cr_assert_eq(rc, CHITCP_OK);
}

/* Sets and cancels a single timer with a null callback */
Test(multitimer, set_and_cancel_single_timer_null_callback, .init = log_setup, .timeout = 2.0)
{
    multi_timer_t mt;
    single_timer_t *timer;
    int rc;

    rc = mt_init(&mt, NUM_TIMERS);
    cr_assert_eq(rc, CHITCP_OK);

    rc = mt_set_timer(&mt, TIMER_IDX, 10*SECOND, null_callback, NULL);
    cr_assert_eq(rc, CHITCP_OK);

    usleep(50*USLEEP_MILLISECOND);

    rc = mt_cancel_timer(&mt, TIMER_IDX);
    cr_assert_eq(rc, CHITCP_OK);

    usleep(50*USLEEP_MILLISECOND);

    rc = mt_get_timer_by_id(&mt, TIMER_IDX, &timer);
    cr_assert_eq(rc, CHITCP_OK);
    cr_assert_eq(timer->active, false);
    cr_assert_eq(timer->num_timeouts, 0);

    rc = mt_free(&mt);
    cr_assert_eq(rc, CHITCP_OK);
}


/* Try cancelling an inactive timer*/
Test(multitimer, cancel_inactive_timer, .init = log_setup, .timeout = 2.0)
{
    multi_timer_t mt;
    single_timer_t *timer;
    int rc;

    rc = mt_init(&mt, NUM_TIMERS);
    cr_assert_eq(rc, CHITCP_OK);

    rc = mt_cancel_timer(&mt, TIMER_IDX);
    cr_assert_eq(rc, CHITCP_EINVAL);

    rc = mt_get_timer_by_id(&mt, TIMER_IDX, &timer);
    cr_assert_eq(rc, CHITCP_OK);
    cr_assert_eq(timer->active, false);
    cr_assert_eq(timer->num_timeouts, 0);

    rc = mt_free(&mt);
    cr_assert_eq(rc, CHITCP_OK);
}


/* Sets a single timer with a null callback and tests that it fired correctly.
 * Then, sets it again.*/
Test(multitimer, set_and_reset_single_timer_null_callback, .init = log_setup, .timeout = 2.0)
{
    multi_timer_t mt;
    single_timer_t *timer;
    int rc;

    rc = mt_init(&mt, NUM_TIMERS);
    cr_assert_eq(rc, CHITCP_OK);

    /* Set the timer */
    rc = mt_set_timer(&mt, TIMER_IDX, 50*MILLISECOND, null_callback, NULL);
    cr_assert_eq(rc, CHITCP_OK);

    usleep(100*USLEEP_MILLISECOND);

    rc = mt_get_timer_by_id(&mt, TIMER_IDX, &timer);
    cr_assert_eq(rc, CHITCP_OK);
    cr_assert_eq(timer->active, false);
    cr_assert_eq(timer->num_timeouts, 1);


    /* Set it again */
    rc = mt_set_timer(&mt, TIMER_IDX, 50*MILLISECOND, null_callback, NULL);
    cr_assert_eq(rc, CHITCP_OK);

    usleep(100*USLEEP_MILLISECOND);

    rc = mt_get_timer_by_id(&mt, TIMER_IDX, &timer);
    cr_assert_eq(rc, CHITCP_OK);
    cr_assert_eq(timer->active, false);
    cr_assert_eq(timer->num_timeouts, 2);


    rc = mt_free(&mt);
    cr_assert_eq(rc, CHITCP_OK);
}


/* Sets all the timers in a multitimer with a null callback and tests
 * that they fired correctly */
Test(multitimer, set_multiple_timers_null_callback, .init = log_setup, .timeout = 2.0)
{
    multi_timer_t mt;
    single_timer_t *timer;
    int rc;

    rc = mt_init(&mt, NUM_TIMERS);
    cr_assert_eq(rc, CHITCP_OK);

    for(uint16_t i=0; i < NUM_TIMERS; i++)
    {
        rc = mt_set_timer(&mt, i, 50*MILLISECOND * (i+1), null_callback, NULL);
        cr_assert_eq(rc, CHITCP_OK);
    }

    usleep((NUM_TIMERS+1)*50*USLEEP_MILLISECOND);

    for(uint16_t i=0; i < NUM_TIMERS; i++)
    {
        rc = mt_get_timer_by_id(&mt, i, &timer);
        cr_assert_eq(rc, CHITCP_OK);
        cr_assert_eq(timer->active, false);
        cr_assert_eq(timer->num_timeouts, 1);
    }

    rc = mt_free(&mt);
    cr_assert_eq(rc, CHITCP_OK);
}


/* Sets some (but not all) timers in a multitimer with a null callback
 * and tests that they fired correctly */
Test(multitimer, set_some_timers_null_callback, .init = log_setup, .timeout = 2.0)
{
    multi_timer_t mt;
    single_timer_t *timer;
    int rc;

    rc = mt_init(&mt, NUM_TIMERS);
    cr_assert_eq(rc, CHITCP_OK);

    for(uint16_t i=0; i < NUM_TIMERS; i+=2)
    {
        rc = mt_set_timer(&mt, i, 50*MILLISECOND * (i+1), null_callback, NULL);
        cr_assert_eq(rc, CHITCP_OK);
    }

    usleep((NUM_TIMERS+1)*50*USLEEP_MILLISECOND);

    for(uint16_t i=0; i < NUM_TIMERS; i++)
    {
        rc = mt_get_timer_by_id(&mt, i, &timer);
        cr_assert_eq(rc, CHITCP_OK);
        cr_assert_eq(timer->active, false);
        cr_assert_eq(timer->num_timeouts, i%2==0?1:0);
    }

    rc = mt_free(&mt);
    cr_assert_eq(rc, CHITCP_OK);
}

void check_timer_timeout(struct timespec* start_time, struct timespec* timeout_time, uint64_t expected_timeout)
{
    struct timespec timespec_diff;
    uint64_t actual_timeout, abs_diff;
    double rel_diff;
    int rc;

    rc = timespec_subtract(&timespec_diff, timeout_time, start_time);
    cr_assert_eq(rc, 0, "The timer recorded a timeout time that is earlier than the test start time");

    actual_timeout = timespec_diff.tv_sec * SECOND + timespec_diff.tv_nsec;

    abs_diff = abs(actual_timeout - expected_timeout);
    rel_diff = ((double) abs_diff) / expected_timeout;

    cr_assert_leq(rel_diff, TIMER_DIFF_TOLERANCE, "Expected timeout to be %lu, got %lu (rel diff %f > %f)",
                                                  expected_timeout, actual_timeout, rel_diff, TIMER_DIFF_TOLERANCE);
}

/* Sets a single timer with the timing callback and tests that it
 * fired at the correct time */
Test(multitimer, set_single_timer_test_timing, .init = log_setup, .timeout = 2.0)
{
    multi_timer_t mt;
    single_timer_t *timer;
    struct timespec start_time;
    int rc;

    struct timespec *timeouts = calloc(NUM_TIMERS, sizeof(struct timespec));
    struct callback_args *args = calloc(NUM_TIMERS, sizeof(struct callback_args));

    for(int i=0; i<NUM_TIMERS; i++)
    {
        args[i].timeouts = timeouts;
        args[i].idx = i;
    }

    clock_gettime(CLOCK_REALTIME, &start_time);

    rc = mt_init(&mt, NUM_TIMERS);
    cr_assert_eq(rc, CHITCP_OK);

    rc = mt_set_timer(&mt, TIMER_IDX, 50*MILLISECOND, timing_callback, &args[TIMER_IDX]);
    cr_assert_eq(rc, CHITCP_OK);

    usleep(100*USLEEP_MILLISECOND);

    rc = mt_get_timer_by_id(&mt, TIMER_IDX, &timer);
    cr_assert_eq(rc, CHITCP_OK);
    cr_assert_eq(timer->active, false);
    cr_assert_eq(timer->num_timeouts, 1);

    check_timer_timeout(&start_time, &timeouts[TIMER_IDX], 50*MILLISECOND);

    rc = mt_free(&mt);
    cr_assert_eq(rc, CHITCP_OK);
}


/* Sets a single timer with the timing callback and tests that it
 * fired at the correct time. Then, sets it again with a different
 * timeout and different parameters to the callback function*/
Test(multitimer, set_and_reset_single_timer_test_timing, .init = log_setup, .timeout = 2.0)
{
    multi_timer_t mt;
    single_timer_t *timer;
    struct timespec start_time, tmp;
    int rc;

    struct timespec *timeouts = calloc(NUM_TIMERS, sizeof(struct timespec));
    struct callback_args *args = calloc(NUM_TIMERS, sizeof(struct callback_args));

    for(int i=0; i<NUM_TIMERS; i++)
    {
        args[i].timeouts = timeouts;
        args[i].idx = i;
    }

    clock_gettime(CLOCK_REALTIME, &start_time);

    rc = mt_init(&mt, NUM_TIMERS);
    cr_assert_eq(rc, CHITCP_OK);

    /* Set the timer */
    rc = mt_set_timer(&mt, TIMER_IDX, 50*MILLISECOND, timing_callback, &args[TIMER_IDX]);
    cr_assert_eq(rc, CHITCP_OK);

    usleep(100*USLEEP_MILLISECOND);

    rc = mt_get_timer_by_id(&mt, TIMER_IDX, &timer);
    cr_assert_eq(rc, CHITCP_OK);
    cr_assert_eq(timer->active, false);
    cr_assert_eq(timer->num_timeouts, 1);

    check_timer_timeout(&start_time, &timeouts[TIMER_IDX], 50*MILLISECOND);

    /* Save the value of the timeout, to verify that the second firing
     * doesn't modify this value */
    tmp.tv_sec = timeouts[TIMER_IDX].tv_sec;
    tmp.tv_nsec = timeouts[TIMER_IDX].tv_nsec;

    /* We set the timer again, but we modify TIMER_IDX+1 in the timeouts array */
    clock_gettime(CLOCK_REALTIME, &start_time);
    rc = mt_set_timer(&mt, TIMER_IDX, 100*MILLISECOND, timing_callback, &args[TIMER_IDX+1]);
    cr_assert_eq(rc, CHITCP_OK);

    usleep(150*USLEEP_MILLISECOND);

    rc = mt_get_timer_by_id(&mt, TIMER_IDX, &timer);
    cr_assert_eq(rc, CHITCP_OK);
    cr_assert_eq(timer->active, false);
    cr_assert_eq(timer->num_timeouts, 2);

    cr_assert_eq(timeouts[TIMER_IDX].tv_sec, tmp.tv_sec, "Re-setting a timer seems to be using incorrect callback arguments.");
    cr_assert_eq(timeouts[TIMER_IDX].tv_nsec, tmp.tv_nsec, "Re-setting a timer seems to be using incorrect callback arguments.");

    check_timer_timeout(&start_time, &timeouts[TIMER_IDX+1], 100*MILLISECOND);

    rc = mt_free(&mt);
    cr_assert_eq(rc, CHITCP_OK);
}


/* Sets multiple timers with the timing callback and tests that they
 * fired at the correct time */
Test(multitimer, set_multiple_timer_test_timing, .init = log_setup, .timeout = 2.0)
{
    multi_timer_t mt;
    single_timer_t *timer;
    struct timespec start_time;
    int rc;

    struct timespec *timeouts = calloc(NUM_TIMERS, sizeof(struct timespec));
    struct callback_args *args = calloc(NUM_TIMERS, sizeof(struct callback_args));

    for(int i=0; i<NUM_TIMERS; i++)
    {
        args[i].timeouts = timeouts;
        args[i].idx = i;
    }

    clock_gettime(CLOCK_REALTIME, &start_time);

    rc = mt_init(&mt, NUM_TIMERS);
    cr_assert_eq(rc, CHITCP_OK);

    for(uint16_t i=0; i < NUM_TIMERS; i++)
    {
        rc = mt_set_timer(&mt, i, 50*MILLISECOND * (i+1), timing_callback, &args[i]);
        cr_assert_eq(rc, CHITCP_OK);
    }

    usleep((NUM_TIMERS+1)*50*USLEEP_MILLISECOND);

    for(uint16_t i=0; i < NUM_TIMERS; i++)
    {
        rc = mt_get_timer_by_id(&mt, i, &timer);
        cr_assert_eq(rc, CHITCP_OK);
        cr_assert_eq(timer->active, false);
        cr_assert_eq(timer->num_timeouts, 1);

        check_timer_timeout(&start_time, &timeouts[i], 50*MILLISECOND * (i+1));
    }

    rc = mt_free(&mt);
    cr_assert_eq(rc, CHITCP_OK);
}


/* Sets multiple timers with the timing callback and tests that they
 * fired at the correct time, except one of the timers will be cancelled.
 * In this test, the cancelled timer is not the next one that is set to expire*/
Test(multitimer, set_multiple_timer_one_cancel_test_timing, .init = log_setup, .timeout = 2.0)
{
    multi_timer_t mt;
    single_timer_t *timer;
    struct timespec start_time;
    int rc;

    struct timespec *timeouts = calloc(NUM_TIMERS, sizeof(struct timespec));
    struct callback_args *args = calloc(NUM_TIMERS, sizeof(struct callback_args));

    for(int i=0; i<NUM_TIMERS; i++)
    {
        args[i].timeouts = timeouts;
        args[i].idx = i;
    }

    clock_gettime(CLOCK_REALTIME, &start_time);

    rc = mt_init(&mt, NUM_TIMERS);
    cr_assert_eq(rc, CHITCP_OK);

    for(uint16_t i=0; i < NUM_TIMERS; i++)
    {
        rc = mt_set_timer(&mt, i, 50*MILLISECOND * (i+1), timing_callback, &args[i]);
        cr_assert_eq(rc, CHITCP_OK);
    }

    usleep(3*50*USLEEP_MILLISECOND);

    rc = mt_cancel_timer(&mt, TIMER_IDX);
    cr_assert_eq(rc, CHITCP_OK);

    usleep(8*50*USLEEP_MILLISECOND);

    for(uint16_t i=0; i < NUM_TIMERS; i++)
    {
        rc = mt_get_timer_by_id(&mt, i, &timer);
        cr_assert_eq(rc, CHITCP_OK);


        if(i==TIMER_IDX)
        {
            cr_assert_eq(timer->active, false, "Cancelled timer still appears active.", i);
            cr_assert_eq(timer->num_timeouts, 0, "Cancelled timer has a non-zero number of timeouts");
            cr_assert_eq(timeouts[TIMER_IDX].tv_sec, 0, "A timeout time was recorded for a cancelled timer");
            cr_assert_eq(timeouts[TIMER_IDX].tv_nsec, 0, "A timeout time was recorded for a cancelled timer");
        }
        else
        {
            cr_assert_eq(timer->num_timeouts, 1);
            cr_assert_eq(timer->active, false, "Timer %i is active, but it shouldn't be.", i);
            check_timer_timeout(&start_time, &timeouts[i], 50 * MILLISECOND * (i + 1));
        }
    }

    rc = mt_free(&mt);
    cr_assert_eq(rc, CHITCP_OK);
}



/* Sets multiple timers with the timing callback and tests that they
 * fired at the correct time, except one of the timers will be cancelled.
 * In this test, the cancelled timer is the next one that is set to expire*/
Test(multitimer, set_multiple_timer_next_cancel_test_timing, .init = log_setup, .timeout = 2.0)
{
    multi_timer_t mt;
    single_timer_t *timer;
    struct timespec start_time;
    int rc;

    struct timespec *timeouts = calloc(NUM_TIMERS, sizeof(struct timespec));
    struct callback_args *args = calloc(NUM_TIMERS, sizeof(struct callback_args));

    for(int i=0; i<NUM_TIMERS; i++)
    {
        args[i].timeouts = timeouts;
        args[i].idx = i;
    }

    clock_gettime(CLOCK_REALTIME, &start_time);

    rc = mt_init(&mt, NUM_TIMERS);
    cr_assert_eq(rc, CHITCP_OK);

    /* The first timer will time out at 100 milliseconds */
    for(uint16_t i=0; i < NUM_TIMERS; i++)
    {
        rc = mt_set_timer(&mt, i, 50*MILLISECOND * (i+2), timing_callback, &args[i]);
        cr_assert_eq(rc, CHITCP_OK);
    }

    usleep(50*USLEEP_MILLISECOND);

    /* Cancel the next timer */
    rc = mt_cancel_timer(&mt, 0);
    cr_assert_eq(rc, CHITCP_OK);


    /* Let the rest of the timers expire */
    usleep((NUM_TIMERS+1)*50*USLEEP_MILLISECOND);

    for(uint16_t i=0; i < NUM_TIMERS; i++)
    {
        rc = mt_get_timer_by_id(&mt, i, &timer);
        cr_assert_eq(rc, CHITCP_OK);


        if(i==0)
        {
            cr_assert_eq(timer->active, false, "Cancelled timer still appears active.", i);
            cr_assert_eq(timer->num_timeouts, 0, "Cancelled timer has a non-zero number of timeouts");
            cr_assert_eq(timeouts[0].tv_sec, 0, "A timeout time was recorded for a cancelled timer");
            cr_assert_eq(timeouts[0].tv_nsec, 0, "A timeout time was recorded for a cancelled timer");
        }
        else
        {
            cr_assert_eq(timer->num_timeouts, 1);
            cr_assert_eq(timer->active, false, "Timer %i is active, but it shouldn't be.", i);
            check_timer_timeout(&start_time, &timeouts[i], 50 * MILLISECOND * (i + 2));
        }
    }

    rc = mt_free(&mt);
    cr_assert_eq(rc, CHITCP_OK);
}



/* Sets multiple timers with the timing callback and cancels
 * all of them before they expire. The timers are cancelled in
 * the order they will expire */
Test(multitimer, set_multiple_timer_all_cancel_test_timing, .init = log_setup, .timeout = 1.0)
{
    multi_timer_t mt;
    single_timer_t *timer;
    struct timespec start_time;
    int rc;

    struct timespec *timeouts = calloc(NUM_TIMERS, sizeof(struct timespec));
    struct callback_args *args = calloc(NUM_TIMERS, sizeof(struct callback_args));

    for(int i=0; i<NUM_TIMERS; i++)
    {
        args[i].timeouts = timeouts;
        args[i].idx = i;
    }

    clock_gettime(CLOCK_REALTIME, &start_time);

    rc = mt_init(&mt, NUM_TIMERS);
    cr_assert_eq(rc, CHITCP_OK);

    for(uint16_t i=0; i < NUM_TIMERS; i++)
    {
        rc = mt_set_timer(&mt, i, 50*MILLISECOND * (i+2), timing_callback, &args[i]);
        cr_assert_eq(rc, CHITCP_OK);
    }

    usleep(50*USLEEP_MILLISECOND);

    for(uint16_t i=0; i < NUM_TIMERS; i++)
    {
        rc = mt_cancel_timer(&mt, i);
        cr_assert_eq(rc, CHITCP_OK);
    }

    usleep(50*USLEEP_MILLISECOND);

    for(uint16_t i=0; i < NUM_TIMERS; i++)
    {
        rc = mt_get_timer_by_id(&mt, i, &timer);
        cr_assert_eq(rc, CHITCP_OK);

        cr_assert_eq(timer->active, false, "Cancelled timer still appears active.", i);
        cr_assert_eq(timer->num_timeouts, 0, "Cancelled timer has a non-zero number of timeouts");
        cr_assert_eq(timeouts[i].tv_sec, 0, "A timeout time was recorded for a cancelled timer");
        cr_assert_eq(timeouts[i].tv_nsec, 0, "A timeout time was recorded for a cancelled timer");
    }

    rc = mt_free(&mt);
    cr_assert_eq(rc, CHITCP_OK);
}


/* Sets multiple timers with the timing callback and cancels
 * all of them before they expire. The timers are cancelled in
 * reverse order of expiration */
Test(multitimer, set_multiple_timer_all_reverse_cancel_test_timing, .init = log_setup, .timeout=1.0)
{
    multi_timer_t mt;
    single_timer_t *timer;
    struct timespec start_time;
    int rc;

    struct timespec *timeouts = calloc(NUM_TIMERS, sizeof(struct timespec));
    struct callback_args *args = calloc(NUM_TIMERS, sizeof(struct callback_args));

    for(int i=0; i<NUM_TIMERS; i++)
    {
        args[i].timeouts = timeouts;
        args[i].idx = i;
    }

    clock_gettime(CLOCK_REALTIME, &start_time);

    rc = mt_init(&mt, NUM_TIMERS);
    cr_assert_eq(rc, CHITCP_OK);

    for(uint16_t i=0; i < NUM_TIMERS; i++)
    {
        rc = mt_set_timer(&mt, i, 50*MILLISECOND * (i+2), timing_callback, &args[i]);
        cr_assert_eq(rc, CHITCP_OK);
    }

    usleep(50*USLEEP_MILLISECOND);

    for(uint16_t i=0; i < NUM_TIMERS; i++)
    {
        rc = mt_cancel_timer(&mt, NUM_TIMERS - (i+1));
        cr_assert_eq(rc, CHITCP_OK);
    }

    /* If the timers have been cancelled, no timers should fire in the next 200 milliseconds */
    usleep(200*USLEEP_MILLISECOND);

    for(uint16_t i=0; i < NUM_TIMERS; i++)
    {
        rc = mt_get_timer_by_id(&mt, i, &timer);
        cr_assert_eq(rc, CHITCP_OK);

        cr_assert_eq(timer->active, false, "Cancelled timer still appears active.", i);
        cr_assert_eq(timer->num_timeouts, 0, "Cancelled timer has a non-zero number of timeouts");
        cr_assert_eq(timeouts[i].tv_sec, 0, "A timeout time was recorded for a cancelled timer");
        cr_assert_eq(timeouts[i].tv_nsec, 0, "A timeout time was recorded for a cancelled timer");
    }

    rc = mt_free(&mt);
    cr_assert_eq(rc, CHITCP_OK);
}

