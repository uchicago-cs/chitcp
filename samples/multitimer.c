/*
 *  chiTCP - A simple, testable TCP stack
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "chitcp/multitimer.h"

void callback_func(multi_timer_t *mt, single_timer_t *timer, void *args)
{
    chilog(INFO, "TIMED OUT! %i", timer->id);
    chilog(INFO, "ACTIVE TIMERS");
    mt_chilog(INFO, mt, true);
}

int main(int argc, char *argv[])
{
    multi_timer_t mt;

    chitcp_setloglevel(INFO);

    mt_init(&mt, 4);

    mt_set_timer_name(&mt, 0, "Retransmission");
    mt_set_timer_name(&mt, 1, "Persist");
    mt_set_timer_name(&mt, 2, "Delayed ACK");
    mt_set_timer_name(&mt, 3, "2MSL");

    chilog(INFO, "ACTIVE TIMERS");
    mt_chilog(INFO, &mt, true);

    chilog(INFO, "Setting all timers except for 2MSL...");
    mt_set_timer(&mt, 0, SECOND * 2, callback_func, NULL);
    mt_set_timer(&mt, 1, SECOND * 0.5, callback_func, NULL);
    mt_set_timer(&mt, 2, SECOND * 1.2, callback_func, NULL);

    chilog(INFO, "ALL TIMERS");
    mt_chilog(INFO, &mt, false);

    chilog(INFO, "ACTIVE TIMERS");
    mt_chilog(INFO, &mt, true);

    chilog(INFO, "Sleeping for a few seconds...");
    sleep(4);

    chilog(INFO, "ALL TIMERS");
    mt_chilog(INFO, &mt, false);
    chilog(INFO, "ACTIVE TIMERS");
    mt_chilog(INFO, &mt, true);

    mt_free(&mt);

    return 0;
}
