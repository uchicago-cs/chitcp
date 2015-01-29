/*
 * check_chitcpd.c
 *
 *  Created on: Jan 11, 2014
 *      Author: borja
 */

#include <stdlib.h>
#include <check.h>

Suite* make_connection_init_suite (void);
Suite* make_data_transfer_suite (void);
Suite* make_connection_term_suite (void);
Suite *make_unreliable_connection_suite (void);

int main (void)
{
    SRunner *sr;
    int number_failed;

    sr = srunner_create ( make_connection_init_suite());
    srunner_add_suite (sr, make_data_transfer_suite());
    srunner_add_suite (sr, make_connection_term_suite());
    srunner_add_suite (sr, make_unreliable_connection_suite ());

    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
