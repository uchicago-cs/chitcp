cmake_minimum_required(VERSION 3.5.1)
project(chitcp C)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR})
set(CMAKE_C_STANDARD 11)
set(CMAKE_BUILD_TYPE Debug)

find_package(Protobuf-c REQUIRED)
find_package(Criterion REQUIRED)

file(GLOB LIB_HDRS "include/chitcp/*.h")
file(GLOB LIB_SRCS "src/libchitcp/*.c" "src/chitcpd-protobuf/protobuf-wrapper.c")
set(PROTOBUF_DIRS ${CMAKE_CURRENT_BINARY_DIR}/protobuf
        ${CMAKE_CURRENT_BINARY_DIR}/protobuf/src/chitcpd-protobuf
        src/chitcpd-protobuf)

include_directories(include)
add_definitions(-D_GNU_SOURCE)


# libchitcp
protobuf_generate_c(PROTO_SRCS PROTO_HDRS src/chitcpd-protobuf/chitcpd.proto)
add_library(chitcp SHARED ${LIB_SRCS} ${PROTO_SRCS} ${PROTO_HDRS})


target_include_directories(chitcp PRIVATE ${PROTOBUF_DIRS})
target_link_libraries(chitcp m)


# chitcpd
add_library(chitcpd STATIC
        src/chitcpd/server.c
        src/chitcpd/serverinfo.c
        src/chitcpd/handlers.c
        src/chitcpd/connection.c
        src/chitcpd/tcp_thread.c
        src/chitcpd/tcp.c
        src/chitcpd/breakpoint.c
        ${PROTO_SRCS}
        ${PROTO_HDRS}
        )
target_include_directories(chitcpd PRIVATE ${PROTOBUF_DIRS})
target_link_libraries(chitcpd pthread m)

add_executable(chitcpd-bin src/chitcpd/main.c)
target_include_directories(chitcpd-bin PRIVATE ${PROTOBUF_DIRS})
target_link_libraries(chitcpd-bin chitcp)
target_link_libraries(chitcpd-bin chitcpd)
target_link_libraries(chitcpd-bin ${PROTOBUF-C_LIBRARIES})

set_target_properties(chitcpd-bin
        PROPERTIES OUTPUT_NAME chitcpd)

# SAMPLES

set(SAMPLE_SOURCES echo-client.c echo-server.c simple-tester.c multitimer.c)
set(SAMPLE_LIBS chitcp ${PROTOBUF-C_LIBRARIES} pthread)

foreach(SAMPLE_SOURCE ${SAMPLE_SOURCES})
    string(REPLACE ".c" "" SAMPLE_BIN ${SAMPLE_SOURCE} )
    add_executable(${SAMPLE_BIN} samples/${SAMPLE_SOURCE} )
    target_link_libraries(${SAMPLE_BIN} ${SAMPLE_LIBS})
endforeach(SAMPLE_SOURCE ${SAMPLE_SOURCES})


# TESTS

set(TEST_LIBS chitcp ${CRITERION_LIBRARY} ${PROTOBUF-C_LIBRARIES} pthread)

# Buffer tests
add_executable(test-buffer tests/test_buffer.c)
target_link_libraries(test-buffer ${TEST_LIBS})

# TCP tests
add_executable(test-tcp
        tests/test_tcp.c
        tests/test_tcp_conn_init.c
        tests/test_tcp_conn_term.c
        tests/test_tcp_data_transfer.c
        tests/test_tcp_unreliable.c
        tests/test_tcp_persist.c
        tests/test_tcp_multitimer.c
        tests/fixtures.c)
target_include_directories(test-tcp PRIVATE src/chitcpd)
target_link_libraries(test-tcp ${TEST_LIBS} chitcpd)

add_custom_target(grade
        DEPENDS ./build/results.json
        COMMAND python3 ../tests/grade.py --report-file results.json)
