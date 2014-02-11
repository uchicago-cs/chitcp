#include <stdlib.h>
#include <check.h>


Suite* make_buffer_suite (void);


int main (void)
{
    SRunner *sr;
    int number_failed;

    sr = srunner_create (make_buffer_suite ());
    //srunner_add_suite (sr, make_list_suite ());

    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
