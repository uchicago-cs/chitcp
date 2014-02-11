#include "chitcp/buffer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <check.h>

uint8_t numbers[16] = {10,20,30,40,50,60,70,80,90,100,110,120,130,140,150,160};

void* consumer_func1(void* args)
{
    int rc;
    uint8_t tmp[26];
    circular_buffer_t *buf = (circular_buffer_t *) args;

    rc = circular_buffer_read(buf, tmp, 8, BUFFER_BLOCKING);
    assert(rc == 8);
    assert(memcmp(numbers, tmp, 8) == 0);

    return NULL;
}

void* producer_func1(void* args)
{
    int rc;
    circular_buffer_t *buf = (circular_buffer_t *) args;

    rc = circular_buffer_write(buf, numbers, 8, BUFFER_BLOCKING);
    assert(rc == 8);

    return NULL;
}

START_TEST (test_buffer_writeread_exact)
{
    int rc;
    circular_buffer_t buf;
    uint8_t tmp[26];

    circular_buffer_init(&buf, 8);
    circular_buffer_set_seq_initial(&buf, 1000);

    rc = circular_buffer_write(&buf, numbers, 3, BUFFER_NONBLOCKING);
    ck_assert_int_eq(rc, 3);
    ck_assert_int_eq(circular_buffer_count(&buf), 3);

    rc = circular_buffer_read(&buf, tmp, 3, BUFFER_NONBLOCKING);
    ck_assert_int_eq(rc, 3);
    ck_assert_int_eq(circular_buffer_count(&buf), 0);
    ck_assert_int_eq(memcmp(numbers, tmp, 3), 0);

    circular_buffer_free(&buf);
}
END_TEST


START_TEST (test_buffer_writepeek_exact)
{
    int rc;
    circular_buffer_t buf;
    uint8_t tmp[26];

    circular_buffer_init(&buf, 8);
    circular_buffer_set_seq_initial(&buf, 1000);

    rc = circular_buffer_write(&buf, numbers, 3, BUFFER_NONBLOCKING);
    ck_assert_int_eq(rc, 3);
    ck_assert_int_eq(circular_buffer_count(&buf), 3);

    rc = circular_buffer_peek(&buf, tmp, 3, BUFFER_NONBLOCKING);
    ck_assert_int_eq(rc, 3);
    ck_assert_int_eq(circular_buffer_count(&buf), 3);
    ck_assert_int_eq(memcmp(numbers, tmp, 3), 0);

    circular_buffer_free(&buf);
}
END_TEST


START_TEST (test_buffer_writeread_more)
{
    int rc;
    circular_buffer_t buf;
    uint8_t tmp[26];

    circular_buffer_init(&buf, 8);
    circular_buffer_set_seq_initial(&buf, 1000);

    rc = circular_buffer_write(&buf, numbers, 3, BUFFER_NONBLOCKING);
    ck_assert_int_eq(rc, 3);
    ck_assert_int_eq(circular_buffer_count(&buf), 3);

    rc = circular_buffer_read(&buf, tmp, 100, BUFFER_NONBLOCKING);
    ck_assert_int_eq(rc, 3);
    ck_assert_int_eq(circular_buffer_count(&buf), 0);
    ck_assert_int_eq(memcmp(numbers, tmp, 3), 0);

    circular_buffer_free(&buf);
}
END_TEST


START_TEST (test_buffer_writepeek_more)
{
    int rc;
    circular_buffer_t buf;
    uint8_t tmp[26];

    circular_buffer_init(&buf, 8);
    circular_buffer_set_seq_initial(&buf, 1000);

    rc = circular_buffer_write(&buf, numbers, 3, BUFFER_NONBLOCKING);
    ck_assert_int_eq(rc, 3);
    ck_assert_int_eq(circular_buffer_count(&buf), 3);

    rc = circular_buffer_peek(&buf, tmp, 100, BUFFER_NONBLOCKING);
    ck_assert_int_eq(rc, 3);
    ck_assert_int_eq(circular_buffer_count(&buf), 3);
    ck_assert_int_eq(memcmp(numbers, tmp, 3), 0);

    circular_buffer_free(&buf);
}
END_TEST



START_TEST (test_buffer_multi_write)
{
    int rc;
    circular_buffer_t buf;
    uint8_t tmp[26];

    circular_buffer_init(&buf, 8);
    circular_buffer_set_seq_initial(&buf, 1000);

    rc = circular_buffer_write(&buf, numbers, 3, BUFFER_NONBLOCKING);
    assert(rc == 3);

    rc = circular_buffer_write(&buf, numbers+3, 3, BUFFER_NONBLOCKING);
    assert(rc == 3);

    rc = circular_buffer_read(&buf, tmp, 6, BUFFER_NONBLOCKING);
    assert(rc == 6);
    assert(memcmp(numbers, tmp, 6) == 0);

    circular_buffer_free(&buf);
}
END_TEST

START_TEST (test_buffer_wrap_1)
{
    int rc;
    circular_buffer_t buf;
    uint8_t tmp[26];

    circular_buffer_init(&buf, 8);
    circular_buffer_set_seq_initial(&buf, 1000);

    rc = circular_buffer_write(&buf, numbers, 3, BUFFER_NONBLOCKING);
    assert(rc == 3);

    rc = circular_buffer_read(&buf, tmp, 3, BUFFER_NONBLOCKING);
    assert(rc == 3);
    assert(memcmp(numbers, tmp, 3) == 0);

    rc = circular_buffer_write(&buf, numbers, 6, BUFFER_NONBLOCKING);
    assert(rc == 6);

    rc = circular_buffer_read(&buf, tmp, 6, BUFFER_NONBLOCKING);
    assert(rc == 6);
    assert(memcmp(numbers, tmp, 6) == 0);

    circular_buffer_free(&buf);
}
END_TEST

START_TEST (test_buffer_wrap_2)
{
    int rc;
    circular_buffer_t buf;
    uint8_t tmp[26];

    circular_buffer_init(&buf, 8);
    circular_buffer_set_seq_initial(&buf, 1000);

    rc = circular_buffer_write(&buf, numbers, 4, BUFFER_NONBLOCKING);
    assert(rc == 4);

    rc = circular_buffer_read(&buf, tmp, 2, BUFFER_NONBLOCKING);
    assert(rc == 2);
    assert(memcmp(numbers, tmp, 2) == 0);

    rc = circular_buffer_write(&buf, numbers+4, 6, BUFFER_NONBLOCKING);
    assert(rc == 6);

    rc = circular_buffer_read(&buf, tmp+2, 8, BUFFER_NONBLOCKING);
    assert(rc == 8);
    assert(memcmp(numbers, tmp, 10) == 0);

    circular_buffer_free(&buf);
}
END_TEST

START_TEST (test_buffer_concurrency_2threads)
{
    circular_buffer_t buf;

    pthread_t consumer_thread, producer_thread;

    circular_buffer_init(&buf, 8);
    circular_buffer_set_seq_initial(&buf, 1000);
    pthread_create(&consumer_thread, NULL, consumer_func1, &buf);
    pthread_create(&producer_thread, NULL, producer_func1, &buf);
    pthread_join(consumer_thread, NULL);
    pthread_join(producer_thread, NULL);
    circular_buffer_free(&buf);
}
END_TEST

Suite* make_buffer_suite (void)
{
  Suite *s = suite_create ("Circular Buffer");

  /* Core test case */
  TCase *tc_writeread = tcase_create ("Write then read");
  tcase_add_test (tc_writeread, test_buffer_writeread_exact);
  tcase_add_test (tc_writeread, test_buffer_writepeek_exact);
  tcase_add_test (tc_writeread, test_buffer_writeread_more);
  tcase_add_test (tc_writeread, test_buffer_writepeek_more);
  suite_add_tcase (s, tc_writeread);

  TCase *tc_multi = tcase_create ("Multiple write/reads");
  tcase_add_test (tc_multi, test_buffer_multi_write);
  suite_add_tcase (s, tc_multi);

  TCase *tc_wraparound = tcase_create ("Wraparound");
  tcase_add_test (tc_wraparound, test_buffer_wrap_1);
  tcase_add_test (tc_wraparound, test_buffer_wrap_2);
  suite_add_tcase (s, tc_wraparound);

  TCase *tc_concurrency = tcase_create ("Concurrency");
  tcase_add_test (tc_concurrency, test_buffer_concurrency_2threads);
  suite_add_tcase (s, tc_concurrency);

  return s;
}
