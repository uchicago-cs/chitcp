#include <stdio.h>
#include <stdbool.h>
#include <criterion/criterion.h>
#include <criterion/options.h>
#include <criterion/output.h>
#include <pthread.h>
#include <signal.h>

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
        result = !criterion_run_all_tests(tests);

    criterion_finalize(tests);
    return result;
}
