#include <stdio.h>
#include <stdbool.h>
#include <criterion/criterion.h>
#include <criterion/options.h>
#include <criterion/output.h>
#include <pthread.h>
#include <signal.h>

/*
 * Checks whether the tests are running on Valgrind.
 *  Source: https://stackoverflow.com/questions/365458/how-can-i-detect-if-a-program-is-running-from-within-valgrind
 */
int running_in_valgrind(void)
{
    char *p = getenv ("LD_PRELOAD");
    if (p == NULL)
        return 0;
    return (strstr (p, "/valgrind/") != NULL ||
            strstr (p, "/vgpreload") != NULL);
}

int main(int argc, char *argv[]) {
    struct criterion_test_set *tests = criterion_initialize();

    /* SIGPIPE handling */
    sigset_t new;
    sigemptyset (&new);
    sigaddset(&new, SIGPIPE);
    if (pthread_sigmask(SIG_BLOCK, &new, NULL) != 0)
    {
        perror("Unable to mask SIGPIPE");
        exit(-1);
    }

    criterion_options.jobs = 1;
    criterion_add_output("json", "results.json");

    int result = 0;
    if (criterion_handle_args(argc, argv, true))
    {
        if (criterion_options.debug != CR_DBG_NONE || running_in_valgrind())
        {
            /* When running in debug mode or in Valgrind,
               disable all timeouts. Otherwise, tests will time
               out while debugging. On Valgrind, the timeout may
               prevent other errors from being detected.

               Apparently using criterion_options.timeout has
               no effect, because that's only meant to affect
               tests with no specified time out, so we have to
               iterate over every test and change its timeout. */
            FOREACH_SET(struct criterion_suite_set *s, tests->suites)
                    {
                        FOREACH_SET(struct criterion_test *test, s->tests)
                                {
                                    test->data->timeout = 0.0;
                                }
                    }
        }
        result = !criterion_run_all_tests(tests);
    }

    criterion_finalize(tests);
    return result;
}
