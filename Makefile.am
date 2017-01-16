
#
# Common
#
ACLOCAL_AMFLAGS = -I m4
AM_CFLAGS = -I$(srcdir)/include -I$(srcdir)/src/chitcpd-protobuf/ -I$(srcdir)/src/simclist/ \
            -g -g3 -Wall -std=gnu99 -ggdb -D_GNU_SOURCE
AM_LDFLAGS = -pthread -lrt

noinst_LTLIBRARIES = libchitcpd.la libsimclist.la


# Silence making even more by suppressing "Entering/Leaving" directory messages
# Use "make V=1" to reenable
PRINT_DIRECTORY_FLAGS_1=
PRINT_DIRECTORY_FLAGS_0=--no-print-directory
PRINT_DIRECTORY_FLAGS_=$(PRINT_DIRECTORY_FLAGS_$(AM_DEFAULT_VERBOSITY))
AM_MAKEFLAGS = $(PRINT_DIRECTORY_FLAGS_$(V))


#
# Protocol buffers
#
AM_V_PROTOC_C = $(am__v_PROTOC_C_$(V))
am__v_PROTOC_C_ = $(am__v_PROTOC_C_$(AM_DEFAULT_VERBOSITY))
am__v_PROTOC_C_0 = @echo "PROTOC_C" $@;

%.pb-c.c %.pb-c.h: %.proto
	$(AM_V_PROTOC_C)$(PROTOC_C) --proto_path=$(srcdir)/src/chitcpd-protobuf/ --c_out=$(builddir)/src/chitcpd-protobuf/ $^

MOSTLYCLEANFILES = \
                    $(srcdir)/src/chitcpd-protobuf/*.o \
                    $(srcdir)/src/chitcpd-protobuf/*.lo \
                    $(srcdir)/src/chitcpd-protobuf/*.pb-c.*

#
# simclist
#
libsimclist_la_SOURCES = src/simclist/simclist.c



#
# libchitcp
#
lib_LTLIBRARIES = libchitcp.la
libchitcp_la_SOURCES = \
                        src/chitcpd-protobuf/chitcpd.pb-c.c \
                        src/chitcpd-protobuf/protobuf-wrapper.c \
                        src/libchitcp/socket_api.c \
                        src/libchitcp/packet.c \
                        src/libchitcp/daemon_api.c \
                        src/libchitcp/log.c \
                        src/libchitcp/addr.c \
                        src/libchitcp/utils.c \
                        src/libchitcp/debug_api.c \
                        src/libchitcp/tester.c \
                        src/libchitcp/tester_peer.c \
                        src/libchitcp/buffer.c
libchitcp_la_CFLAGS = $(AM_CFLAGS) $(PROTOBUF_C_CFLAGS)
libchitcp_la_LIBADD = $(PROTOBUF_C_LIBS) libsimclist.la
libchitcp_la_DEPENDENCIES = libsimclist.la

#
# chitcpd
#
libchitcpd_la_SOURCES = \
                         src/simclist/simclist.c \
                         src/chitcpd/server.c \
                         src/chitcpd/serverinfo.c \
                         src/chitcpd/handlers.c \
                         src/chitcpd/connection.c \
                         src/chitcpd/tcp_thread.c \
                         src/chitcpd/tcp.c \
                         src/chitcpd/breakpoint.c

bin_PROGRAMS = chitcpd
chitcpd_SOURCES = src/chitcpd/main.c
chitcpd_LDADD = libsimclist.la libchitcpd.la libchitcp.la
chitcpd_DEPENDENCIES = libsimclist.la libchitcpd.la src/chitcpd-protobuf/chitcpd.pb-c.h


#
# samples
#
CHITCP_SAMPLES = samples/echo-server samples/echo-client samples/simple-tester
EXTRA_PROGRAMS = $(CHITCP_SAMPLES)
samples: $(CHITCP_SAMPLES)
 .PHONY: samples

samples_echo_server_SOURCES = \
                               samples/echo-server.c
samples_echo_server_LDADD = libchitcp.la

samples_echo_client_SOURCES = \
                               samples/echo-client.c
samples_echo_client_LDADD = libchitcp.la

samples_simple_tester_SOURCES = \
                               samples/simple-tester.c
samples_simple_tester_LDADD = libchitcp.la


#
# tests
#

tcptests: tests/test-tcp

# use the provided wrapper script to output things properly
LOG_COMPILER = $(top_srcdir)/build-aux/criterion-tap-test

# use the TAP log driver
LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
            $(top_srcdir)/build-aux/tap-driver.sh

TESTS = tests/test-buffer tests/test-daemon tests/test-tcp
check_PROGRAMS = $(TESTS)

tests_test_buffer_SOURCES = tests/test_buffer.c
tests_test_buffer_LDFLAGS = -lcriterion
tests_test_buffer_CFLAGS = $(AM_CFLAGS) $(CHECK_CFLAGS)
tests_test_buffer_LDADD = libchitcp.la $(CHECK_LIBS)

tests_test_daemon_SOURCES = tests/test_daemon.c
tests_test_daemon_LDFLAGS = -lcriterion
tests_test_daemon_CFLAGS = $(AM_CFLAGS) $(CHECK_CFLAGS) -I${srcdir}/src/chitcpd
tests_test_daemon_LDADD = libchitcp.la libchitcpd.la $(CHECK_LIBS)
tests_test_daemon_DEPENDENCIES = libchitcpd.la

tests_test_tcp_SOURCES = tests/test_tcp.c \
						 tests/test_tcp_conn_init.c \
						 tests/test_tcp_conn_term.c \
						 tests/test_tcp_data_transfer.c \
						 tests/test_tcp_unreliable.c \
                         tests/fixtures.c
tests_test_tcp_LDFLAGS = -lcriterion
tests_test_tcp_CFLAGS = $(AM_CFLAGS) $(CHECK_CFLAGS) -I${srcdir}/src/chitcpd
tests_test_tcp_LDADD = libchitcp.la libchitcpd.la $(CHECK_LIBS)
tests_test_tcp_DEPENDENCIES = libchitcpd.la
