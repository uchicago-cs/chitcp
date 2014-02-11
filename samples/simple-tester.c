#include <stdio.h>
#include <stdlib.h>
#include "chitcp/tester.h"

int main(int argc, char *argv[])
{
    chitcp_tester_t t;

    EXIT_ON_ERROR(chitcp_tester_init(&t),
            "Could not initialize tester");

    EXIT_ON_ERROR(chitcp_tester_start(&t),
            "Could not start tester");

    EXIT_ON_ERROR(chitcp_tester_server_listen(&t),
            "Tester server won't listen()");

    EXIT_ON_ERROR(chitcp_tester_server_accept(&t),
            "Tester server won't accept()");

    EXIT_ON_ERROR(chitcp_tester_client_connect(&t),
            "Tester client won't connect()");

    getchar();
    chitcp_tester_client_close(&t);
    chitcp_tester_server_close(&t);
}
