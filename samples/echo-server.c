/*
 *  chiTCP - A simple, testable TCP stack
 *
 *  Echo server using chisockets.
 *
 *  Not complete yet: can accept connections, but can't send/receive yet.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chitcp/socket.h"
#include "chitcp/types.h"
#include "chitcp/addr.h"
#include "chitcp/utils.h"

const char* USAGE = "echo-server [-p PORT] [-s] [-v]";

int main(int argc, char *argv[])
{
    int serverSocket;
    int clientSocket;
    int opt;
    char *port = NULL;
    uint16_t iport;
    bool_t step = FALSE;
    bool_t verbose = FALSE;

    struct sockaddr_in serverAddr, clientAddr;
    socklen_t sinSize = sizeof(struct sockaddr_in);
    memset(&serverAddr, 0, sizeof(serverAddr));
    char addr_str[100];

    /* Use getopt to fetch the host and port */
    while ((opt = getopt(argc, argv, "p:sv")) != -1)
        switch (opt)
        {
        case 'p':
            port = strdup(optarg);
            break;
        case 's':
            step = TRUE;
            break;
        case 'v':
            verbose = TRUE;
            break;
        default:
            printf("Unknown option: -%c\n", opt);
            printf("%s\n", USAGE);
            exit(1);
        }

    if(!port)
        iport = 7;
    else
        sscanf(port, "%hu", &iport);

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(iport); // The echo protocol
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(step)
    {
        printf("Press any key to create the socket...");
        getchar();
    }

    serverSocket = chisocket_socket(PF_INET,       // Family: IPv4
                                    SOCK_STREAM,   // Type: Full-duplex stream (reliable)
                                    IPPROTO_TCP);  // Protocol: TCP;

    if(serverSocket == -1)
    {
        perror("Could not create socket");
        exit(-1);
    }

    if(step)
    {
        printf("Press any key to bind the socket...");
        getchar();
    }

    if(chisocket_bind(serverSocket, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) == -1)
    {
        perror("Socket bind() failed");
        chisocket_close(serverSocket);
        exit(-1);
    }

    if(step)
    {
        printf("Press any key to make the socket listen...");
        getchar();
    }

    if(chisocket_listen(serverSocket, 5) == -1)
    {
        perror("Socket listen() failed");
        chisocket_close(serverSocket);
        exit(-1);
    }

    if(step)
    {
        printf("Press any key to accept a connection...");
        getchar();
    }
    else
    {
        printf("Waiting for a connection on port %u ...\n", ntohs(serverAddr.sin_port));
    }


    sinSize = sizeof(struct sockaddr_in);
    if( (clientSocket = chisocket_accept(serverSocket, (struct sockaddr *) &clientAddr, &sinSize)) == -1)
    {
        perror("Socket accept() failed");
        chisocket_close(serverSocket);
        exit(-1);
    }

    chitcp_addr_str((struct sockaddr *) &clientAddr, addr_str, sizeof(addr_str));
    printf("Got a connection from %s\n", addr_str);

    char buf[536 + 1];

    while(1)
    {
        int nbytes = chisocket_recv(clientSocket, buf, 536, 0);
        if(nbytes == 0)
        {
            break;
        }
        else if (nbytes == -1)
        {
            perror("Socket recv() failed");
            chisocket_close(clientSocket);
            chisocket_close(serverSocket);
            exit(-1);
        }
        else
        {
            if(verbose)
            {
                printf("Received: %.*s\n", nbytes, buf);
            }
            if (chitcp_socket_send(clientSocket, buf, nbytes) == -1)
            {
                chisocket_close(clientSocket);
                chisocket_close(serverSocket);
                exit(-1);
            }
        }
    }

    printf("Peer has closed connection.\n");
    if(step)
    {
        printf("Press any key to close active socket...");
        getchar();
    }

    if (chisocket_close(clientSocket) == -1)
    {
        perror("Could not close socket");
        exit(-1);
    }

    printf("Active socket closed.\n");

    if(step)
    {
        printf("Press any key to close passive socket...");
        getchar();
    }

    if (chisocket_close(serverSocket) == -1)
    {
        perror("Could not close passive socket");
        exit(-1);
    }

    printf("Passive socket closed.\n");

    if (step)
    {
        printf("Press any key to exit...");
        getchar();
    }

    return 0;
}
