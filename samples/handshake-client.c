/*
 *  chiTCP - A simple, testable TCP stack
 *
 *  Echo client using chisockets.
 *
 *  Not complete yet: can create connections, but can't send/receive yet.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "chitcp/socket.h"
#include "chitcp/packet.h"
#include "chitcp/types.h"
#include "chitcp/addr.h"
#include "utils.h"

int main(int argc, char *argv[])
{
    int clientSocket;
    struct sockaddr_in serverAddr;
    char *host = "localhost", *port = "7";
    int opt;

    /* Use getopt to fetch the host and port */
    while ((opt = getopt(argc, argv, "h:p:")) != -1)
        switch (opt)
        {
        case 'h':
            host = strdup(optarg);
            break;
        case 'p':
            port = strdup(optarg);
            break;
        default:
            printf("Unknown option\n");
            exit(1);
        }

    clientSocket = chisocket_socket(PF_INET,       // Family: IPv4
                                    SOCK_STREAM,   // Type: Full-duplex stream (reliable)
                                    IPPROTO_TCP);  // Protocol: TCP;

    if (constructAddress(host, port, &serverAddr))
    {
        perror("Could not construct address");
        exit(-1);
    }

    if (chisocket_connect(clientSocket, (struct sockaddr *) &serverAddr, sizeof(struct sockaddr_in)) == -1)
    {
        chisocket_close(clientSocket);
        perror("Could not connect to socket");
        exit(-1);
    }

    //char *msg = "Hello, echo! abcdefghijklmnopqrstuvxyz0123456789";

    //chisocket_send(clientSocket, msg, strlen(msg), 0);

    printf("Connected to server. Press any key to close connection... ");
    getchar();
    printf("closing connection... ");
    //chisocket_close(clientSocket);
    printf("Closed!");

    return 0;
}
