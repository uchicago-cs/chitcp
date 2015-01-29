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


/* Logging level. Set by default to print just errors */
static int loglevel = ERROR;


void chitcp_setloglevel(loglevel_t level)
{
    loglevel = level;
}


void chilog(loglevel_t level, char *fmt, ...)
{
    time_t t;
    char buf[80], *levelstr;
    va_list argptr;

    if(level > loglevel)
        return;

    t = time(NULL);
    strftime(buf,80,"%Y-%m-%d %H:%M:%S",localtime(&t));

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
    printf("[%s] %6s %s ", buf, levelstr, threadname);
    va_start(argptr, fmt);
    vprintf(fmt, argptr);
    printf("\n");
    funlockfile(stdout);
    va_end(argptr);
    fflush(stdout);
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
