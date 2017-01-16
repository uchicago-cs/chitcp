/*
 *  chiTCP - A simple, testable TCP stack
 *
 *  Logging functions
 *
 *  see chitcp/log.h for descriptions of functions, parameters, and return values.
 *
 */

/*
 *  Copyright (c) 2013-2014, The University of Chicago
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 *  - Neither the name of The University of Chicago nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <pthread.h> /* for pthread_self */

#include "chitcp/log.h"
#include "chitcp/addr.h"


/* Logging level. Set by default to print just errors */
static int loglevel = ERROR;


void chitcp_setloglevel(loglevel_t level)
{
    loglevel = level;
}


void chilog(loglevel_t level, char *fmt, ...)
{
    struct timespec ts;
    struct tm *tm;
    char timefmt[64], buf[80], *levelstr;
    va_list argptr;

    if(level > loglevel)
        return;


    clock_gettime(CLOCK_REALTIME, &ts);
    if((tm = localtime(&ts.tv_sec)) != NULL)
    {
            strftime(timefmt, sizeof(timefmt), "%H:%M:%S.%%09u", tm);
            snprintf(buf, sizeof(buf), timefmt, ts.tv_nsec);
    }

    switch(level)
    {
    case CRITICAL:
        levelstr = "CRITIC";
        break;
    case ERROR:
        levelstr = "ERROR";
        break;
    case WARNING:
        levelstr = "WARN";
        break;
    case MINIMAL:
        levelstr = "MINIMAL";
        break;
    case INFO:
        levelstr = "INFO";
        break;
    case DEBUG:
        levelstr = "DEBUG";
        break;
    case TRACE:
        levelstr = "TRACE";
        break;
    default:
        levelstr = "UNKNOWN";
        break;
    }

    /* Get the thread's name. */
    char threadname[16];
    pthread_getname_np(pthread_self(), threadname, 16);

    flockfile(stdout);
    if (loglevel == MINIMAL)
        printf("[%s] %16s ", buf, threadname);
    else
        printf("[%s] %7s %16s ", buf, levelstr, threadname);
    va_start(argptr, fmt);
    vprintf(fmt, argptr);
    printf("\n");
    funlockfile(stdout);
    va_end(argptr);
    fflush(stdout);
}

static char* srcdst_str(struct sockaddr *src, struct sockaddr *dst, char *buf, int len)
{
    char ipsrc[INET6_ADDRSTRLEN], ipdst[INET6_ADDRSTRLEN];
    uint32_t portsrc, portdst;

    inet_ntop(src->sa_family, chitcp_get_addr(src), ipsrc, sizeof(ipsrc));
    portsrc = chitcp_get_addr_port(src);
    portsrc = chitcp_ntohs(portsrc);

    inet_ntop(dst->sa_family, chitcp_get_addr(dst), ipdst, sizeof(ipdst));
    portdst = chitcp_get_addr_port(dst);
    portdst = chitcp_ntohs(portdst);

    snprintf(buf, len, "%s.%i > %s.%i", ipsrc, portsrc, ipdst, portdst);

    return buf;
}

void chilog_tcp_minimal(struct sockaddr *src, struct sockaddr *dst, int sockfd, tcp_packet_t *packet, char* prefix)
{
    if(loglevel != MINIMAL)
        return;

    tcphdr_t *header = (tcphdr_t*) packet->raw;
    uint16_t payload_len = TCP_PAYLOAD_LEN(packet);

    char srcdst[256];
    char flags[9];
    char seqstr[32];
    char ackstr[32];

    srcdst_str(src, dst, srcdst, 255);

    snprintf(flags, 8, "%s%s%s%s%s%s%s%s",
            header->cwr? "W":"",
            header->ece? "E":"",
            header->urg? "U":"",
            header->psh? "P":"",
            header->rst? "R":"",
            header->syn? "S":"",
            header->fin? "F":"",
            header->ack? ".":""
    );

    if(flags[0] == '\0')
        strcpy(flags, "none");

    if(payload_len)
        snprintf(seqstr, 31, " seq %i:%i,", chitcp_ntohl(header->seq), chitcp_ntohl(header->seq) + payload_len);
    else
        snprintf(seqstr, 31, " seq %i,", chitcp_ntohl(header->seq));

    if(header->ack)
        snprintf(ackstr, 31, " ack %i,", chitcp_ntohl(header->ack_seq));
    else
        ackstr[0] = '\0';

    chilog(MINIMAL, "[S%i] %s %s: Flags [%s],%s%s win %i, length %i",
                    sockfd, prefix, srcdst, flags, seqstr, ackstr, chitcp_ntohs(header->win), payload_len);
}

void chilog_tcp(loglevel_t level, tcp_packet_t *packet, char prefix)
{
    if(level > loglevel)
        return;

    tcphdr_t *header = (tcphdr_t*) packet->raw;
    uint8_t *payload = TCP_PAYLOAD_START(packet);
    uint16_t payload_len = TCP_PAYLOAD_LEN(packet);

    flockfile(stdout);
    chilog(level, "   ######################################################################");

    chilog(level, "%c  Src: %i  Dest: %i  Seq: %i  Ack: %i  Doff: %i  Win: %i",
           prefix,
           chitcp_ntohs(header->source),
           chitcp_ntohs(header->dest),
           chitcp_ntohl(header->seq),
           chitcp_ntohl(header->ack_seq),
           header->doff,
           chitcp_ntohs(header->win));
    chilog(level, "%c  CWR: %i  ECE: %i  URG: %i  ACK: %i  PSH: %i  RST: %i  SYN: %i  FIN: %i",
           prefix,
           header->cwr,
           header->ece,
           header->urg,
           header->ack,
           header->psh,
           header->rst,
           header->syn,
           header->fin);

    if(payload_len > 0)
    {
        chilog(level, "%c  Payload (%i bytes):", prefix, payload_len);
        chilog_hex(level, payload, payload_len);
    }
    else
    {
        chilog(level, "%c  No Payload", prefix);
    }
    chilog(level, "   ######################################################################");
    funlockfile(stdout);
}


void chilog_chitcp(loglevel_t level, uint8_t *packet, char prefix)
{
    if(level > loglevel)
        return;

    chitcphdr_t *header = (chitcphdr_t*) packet;

    flockfile(stdout);
    chilog(level, "   ======================================================================");
    chilog(level, "%c  Payload length: %i", prefix, chitcp_ntohs(header->payload_len));
    chilog(level, "%c  Protocol: %i", prefix, header->proto);
    chilog(level, "   ======================================================================");
    funlockfile(stdout);
}


// Based on http://stackoverflow.com/questions/7775991/how-to-get-hexdump-of-a-structure-data
void chilog_hex (loglevel_t level, void *data, int len)
{
    int i;
    char buf[8];
    char ascii[17];
    char line[74];
    uint8_t *pc = data;

    line[0] = '\0';
    // Process every byte in the data.
    for (i = 0; i < len; i++)
    {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0)
        {
            // Just don't print ASCII for the zeroth line.
            if (i != 0)
            {
                chilog(level, "%s  %s", line, ascii);
                line[0] = '\0';
            }

            // Output the offset.
            sprintf(buf, "  %04x ", i);
            strcat(line, buf);
        }

        // Now the hex code for the specific character.
        sprintf(buf, " %02x", pc[i]);
        strcat(line, buf);

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))
            ascii[i % 16] = '.';
        else
            ascii[i % 16] = pc[i];
        ascii[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0)
    {
        strcat(line, "   ");
        i++;
    }

    // And print the final ASCII bit.
    chilog(level, "%s  %s", line, ascii);
}
