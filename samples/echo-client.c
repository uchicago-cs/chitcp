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

#include "chitcp/socket.h"
#include "chitcp/packet.h"
#include "chitcp/types.h"
#include "chitcp/addr.h"

const char* USAGE = "echo-client [-h HOSTNAME] [-p PORT] [-m MESSAGE | -f FILE] [-s]";

int sockSend(int socket, char *buffer, int length)
{
  int nbytes, nleft, nwritten = 0;

  nleft = length;

  while ( nleft > 0 )
  {
      if ( (nbytes = chisocket_send(socket, buffer, nleft, 0)) == -1 )
      {
          perror("Could not send message");
          return -1;
      }
      nleft  -= nbytes;
      buffer += nbytes;
      nwritten += nbytes;
  }
  return nwritten;
}

int main(int argc, char *argv[])
{
    int clientSocket;
    struct sockaddr_in serverAddr;
    char *host = "localhost", *port = "7";
    bool_t step = FALSE;
    char *msg = NULL;
    char *filename = NULL;
    char *buf = NULL;
    size_t buf_size;
    int opt;

    /* Use getopt to fetch the host and port */
    while ((opt = getopt(argc, argv, "h:p:m:f:s")) != -1)
        switch (opt)
        {
        case 'h':
            host = strdup(optarg);
            break;
        case 'p':
            port = strdup(optarg);
            break;
        case 'm':
            msg = strdup(optarg);
            break;
        case 'f':
            filename = strdup(optarg);
            break;
        case 's':
            step = TRUE;
            break;
        default:
            printf("Unknown option: -%c\n", opt);
            printf("%s\n", USAGE);
            exit(1);
        }

    if(msg && filename)
    {
        printf("You cannot specify both -m and -f.");
        printf("%s\n", USAGE);
    }

    if(step)
    {
        printf("Press any key to create the socket...");
        getchar();
    }

    clientSocket = chisocket_socket(PF_INET,       // Family: IPv4
                                    SOCK_STREAM,   // Type: Full-duplex stream (reliable)
                                    IPPROTO_TCP);  // Protocol: TCP;

    if(clientSocket == -1)
    {
        perror("Could not create socket");
        exit(-1);
    }

    if (chitcp_addr_construct(host, port, &serverAddr))
    {
        perror("Could not construct address");
        exit(-1);
    }

    if(step)
    {
        printf("Press any key to connect to the server...");
        getchar();
    }

    if (chisocket_connect(clientSocket, (struct sockaddr *) &serverAddr, sizeof(struct sockaddr_in)) == -1)
    {
        chisocket_close(clientSocket);
        perror("Could not connect to socket");
        exit(-1);
    }

    if(step && msg)
    {
        printf("Press any key to send message '%s'...", msg);
        getchar();
    }

    if(msg)
    {
        if (sockSend(clientSocket, msg, strlen(msg)) == -1)
        {
            chisocket_close(clientSocket);
            exit(-1);
        }
        printf("Message sent:  '%s'\n", msg);
    }
    else
    {
        int nread;

        buf_size = 128;
        buf = malloc(buf_size);
        while(1)
        {
            int nrecv;
            char *recv_buf, *recv_buf_ptr;

            printf("echo> ");
            nread = getline(&buf, &buf_size, stdin);
            printf("%i\n", nread);
            if(nread == -1)
                break;

            if ( sockSend(clientSocket, buf, nread) == -1 )
            {
                chisocket_close(clientSocket);
                exit(-1);
            }

            recv_buf = malloc(nread);
            recv_buf_ptr = recv_buf;
            nrecv = nread;

            while (nrecv != 0)
            {
                int nbytes = chisocket_recv(clientSocket, recv_buf_ptr, nrecv, 0);
                if(nbytes == 0)
                {
                    break;
                }
                else if (nbytes == -1)
                {
                    perror("Socket recv() failed");
                    chisocket_close(clientSocket);
                    exit(-1);
                }
                else
                {
                    recv_buf_ptr += nbytes;
                    nrecv -= nbytes;
                }
            }

            if (memcmp(buf, recv_buf, nread) != 0)
            {
                printf("Echo from server did not match");
            }
            else
                printf("%.*s", nread, recv_buf);
        }
    }

    if (step)
    {
        printf("Press any key to close connection...");
        getchar();
    }

    if (chisocket_close(clientSocket) == -1)
    {
        perror("Could not close socket");
        exit(-1);
    }

    printf("Connection closed.\n");

    if (step)
    {
        printf("Press any key to exit...");
        getchar();
    }

    return 0;
}
