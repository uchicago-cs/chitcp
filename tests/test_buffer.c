#include "chitcp/buffer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <criterion/criterion.h>

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

Test(buffer, writeread_exact)
{
    int rc;
    circular_buffer_t buf;
    uint8_t tmp[26];

    circular_buffer_init(&buf, 8);
    circular_buffer_set_seq_initial(&buf, 1000);

    rc = circular_buffer_write(&buf, numbers, 3, BUFFER_NONBLOCKING);
    cr_assert_eq(rc, 3);
    cr_assert_eq(circular_buffer_count(&buf), 3);

    rc = circular_buffer_read(&buf, tmp, 3, BUFFER_NONBLOCKING);
    cr_assert_eq(rc, 3);
    cr_assert_eq(circular_buffer_count(&buf), 0);
    cr_assert_eq(memcmp(numbers, tmp, 3), 0);

    circular_buffer_free(&buf);
}


Test(buffer, writepeek_exact)
{
    int rc;
    circular_buffer_t buf;
    uint8_t tmp[26];

    circular_buffer_init(&buf, 8);
    circular_buffer_set_seq_initial(&buf, 1000);

    rc = circular_buffer_write(&buf, numbers, 3, BUFFER_NONBLOCKING);
    cr_assert_eq(rc, 3);
    cr_assert_eq(circular_buffer_count(&buf), 3);

    rc = circular_buffer_peek(&buf, tmp, 3, BUFFER_NONBLOCKING);
    cr_assert_eq(rc, 3);
    cr_assert_eq(circular_buffer_count(&buf), 3);
    cr_assert_eq(memcmp(numbers, tmp, 3), 0);

    circular_buffer_free(&buf);
}

Test(buffer, writepeekat_exact)
{
    int rc;
    circular_buffer_t buf;
    uint8_t tmp[26];

    circular_buffer_init(&buf, 8);
    circular_buffer_set_seq_initial(&buf, 1000);

    rc = circular_buffer_write(&buf, numbers, 6, BUFFER_NONBLOCKING);
    cr_assert_eq(rc, 6);
    cr_assert_eq(circular_buffer_count(&buf), 6);

    rc = circular_buffer_peek_at(&buf, tmp, 1000, 3);
    cr_assert_eq(rc, 3);
    cr_assert_eq(circular_buffer_count(&buf), 6);
    cr_assert_eq(memcmp(numbers, tmp, 3), 0);

    rc = circular_buffer_peek_at(&buf, tmp, 1003, 3);
    cr_assert_eq(rc, 3);
    cr_assert_eq(circular_buffer_count(&buf), 6);
    cr_assert_eq(memcmp(numbers + 3, tmp, 3), 0);

    circular_buffer_free(&buf);
}


Test(buffer, writeread_more)
{
    int rc;
    circular_buffer_t buf;
    uint8_t tmp[26];

    circular_buffer_init(&buf, 8);
    circular_buffer_set_seq_initial(&buf, 1000);

    rc = circular_buffer_write(&buf, numbers, 3, BUFFER_NONBLOCKING);
    cr_assert_eq(rc, 3);
    cr_assert_eq(circular_buffer_count(&buf), 3);

    rc = circular_buffer_read(&buf, tmp, 100, BUFFER_NONBLOCKING);
    cr_assert_eq(rc, 3);
    cr_assert_eq(circular_buffer_count(&buf), 0);
    cr_assert_eq(memcmp(numbers, tmp, 3), 0);

    circular_buffer_free(&buf);
}

Test(buffer, writepeek_more)
{
    int rc;
    circular_buffer_t buf;
    uint8_t tmp[26];

    circular_buffer_init(&buf, 8);
    circular_buffer_set_seq_initial(&buf, 1000);

    rc = circular_buffer_write(&buf, numbers, 3, BUFFER_NONBLOCKING);
    cr_assert_eq(rc, 3);
    cr_assert_eq(circular_buffer_count(&buf), 3);

    rc = circular_buffer_peek_at(&buf, tmp, 1000, 100);
    cr_assert_eq(rc, 3);
    cr_assert_eq(circular_buffer_count(&buf), 3);
    cr_assert_eq(memcmp(numbers, tmp, 3), 0);

    circular_buffer_free(&buf);
}

Test(buffer, writepeekat_more)
{
    int rc;
    circular_buffer_t buf;
    uint8_t tmp[26];

    circular_buffer_init(&buf, 8);
    circular_buffer_set_seq_initial(&buf, 1000);

    rc = circular_buffer_write(&buf, numbers, 6, BUFFER_NONBLOCKING);
    cr_assert_eq(rc, 6);
    cr_assert_eq(circular_buffer_count(&buf), 6);

    rc = circular_buffer_peek_at(&buf, tmp, 1000, 100);
    cr_assert_eq(rc, 6);
    cr_assert_eq(circular_buffer_count(&buf), 6);
    cr_assert_eq(memcmp(numbers, tmp, 6), 0);

    rc = circular_buffer_peek_at(&buf, tmp, 1003, 100);
    cr_assert_eq(rc, 3);
    cr_assert_eq(circular_buffer_count(&buf), 6);
    cr_assert_eq(memcmp(numbers + 3, tmp, 3), 0);

    circular_buffer_free(&buf);
}

Test(buffer, writepeekat_inval)
{
    int rc;
    circular_buffer_t buf;
    uint8_t tmp[26];

    circular_buffer_init(&buf, 8);
    circular_buffer_set_seq_initial(&buf, 1000);

    rc = circular_buffer_write(&buf, numbers, 6, BUFFER_NONBLOCKING);
    cr_assert_eq(rc, 6);
    cr_assert_eq(circular_buffer_count(&buf), 6);

    rc = circular_buffer_peek_at(&buf, tmp, 999, 3);
    cr_assert_eq(rc, CHITCP_EINVAL);
    cr_assert_eq(circular_buffer_count(&buf), 6);

    rc = circular_buffer_peek_at(&buf, tmp, 1007, 3);
    cr_assert_eq(rc, CHITCP_EINVAL);
    cr_assert_eq(circular_buffer_count(&buf), 6);

    circular_buffer_free(&buf);
}

Test(buffer, writepeekat_empty)
{
    int rc;
    circular_buffer_t buf;
    uint8_t tmp[26];

    circular_buffer_init(&buf, 8);
    circular_buffer_set_seq_initial(&buf, 1000);

    rc = circular_buffer_peek_at(&buf, tmp, 999, 3);
    cr_assert_eq(rc, CHITCP_EINVAL);
    cr_assert_eq(circular_buffer_count(&buf), 0);

    rc = circular_buffer_peek_at(&buf, tmp, 1000, 0);
    cr_assert_eq(rc, CHITCP_EINVAL);
    cr_assert_eq(circular_buffer_count(&buf), 0);

    rc = circular_buffer_peek_at(&buf, tmp, 1000, 3);
    cr_assert_eq(rc, CHITCP_EINVAL);
    cr_assert_eq(circular_buffer_count(&buf), 0);

    circular_buffer_free(&buf);
}

Test(buffer, multi_write)
{
    int rc;
    circular_buffer_t buf;
    uint8_t tmp[26];

    circular_buffer_init(&buf, 8);
    circular_buffer_set_seq_initial(&buf, 1000);

    rc = circular_buffer_write(&buf, numbers, 3, BUFFER_NONBLOCKING);
    cr_assert_eq(rc, 3);

    rc = circular_buffer_write(&buf, numbers+3, 3, BUFFER_NONBLOCKING);
    cr_assert_eq(rc, 3);

    rc = circular_buffer_read(&buf, tmp, 6, BUFFER_NONBLOCKING);
    cr_assert_eq(rc, 6);
    cr_assert_eq(memcmp(numbers, tmp, 6), 0);

    circular_buffer_free(&buf);
}

Test(buffer, wraparound_1)
{
    int rc;
    circular_buffer_t buf;
    uint8_t tmp[26];

    circular_buffer_init(&buf, 8);
    circular_buffer_set_seq_initial(&buf, 1000);

    rc = circular_buffer_write(&buf, numbers, 3, BUFFER_NONBLOCKING);
    cr_assert_eq(rc, 3);

    rc = circular_buffer_read(&buf, tmp, 3, BUFFER_NONBLOCKING);
    cr_assert_eq(rc, 3);
    cr_assert_eq(memcmp(numbers, tmp, 3), 0);

    rc = circular_buffer_write(&buf, numbers, 6, BUFFER_NONBLOCKING);
    cr_assert_eq(rc, 6);

    rc = circular_buffer_read(&buf, tmp, 6, BUFFER_NONBLOCKING);
    cr_assert_eq(rc, 6);
    cr_assert_eq(memcmp(numbers, tmp, 6), 0);

    circular_buffer_free(&buf);
}


Test(buffer, wraparound_2)
{
    int rc;
    circular_buffer_t buf;
    uint8_t tmp[26];

    circular_buffer_init(&buf, 8);
    circular_buffer_set_seq_initial(&buf, 1000);

    rc = circular_buffer_write(&buf, numbers, 4, BUFFER_NONBLOCKING);
    cr_assert_eq(rc, 4);

    rc = circular_buffer_read(&buf, tmp, 2, BUFFER_NONBLOCKING);
    cr_assert_eq(rc, 2);
    cr_assert_eq(memcmp(numbers, tmp, 2), 0);

    rc = circular_buffer_write(&buf, numbers+4, 6, BUFFER_NONBLOCKING);
    cr_assert_eq(rc, 6);

    rc = circular_buffer_read(&buf, tmp+2, 8, BUFFER_NONBLOCKING);
    cr_assert_eq(rc, 8);
    cr_assert_eq(memcmp(numbers, tmp, 10), 0);

    circular_buffer_free(&buf);
}

Test(buffer, concurrency_1)
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
