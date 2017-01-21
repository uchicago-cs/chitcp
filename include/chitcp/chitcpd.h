/*
 *  chiTCP - A simple, testable TCP stack
 *
 *  Constants and type declarations for interfacing with
 *  the chiTCP daemon.
 *
 *  Used by libchitcp and chitcpd, but shouldn't be included
 *  by user applications.
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

#ifndef CHITCP_CHITCPD_H_
#define CHITCP_CHITCPD_H_

#include <stdlib.h>
#include <stdint.h>
#include "chitcp/types.h"
#include "chitcp/debug_api.h"

/* Default values */
#define GET_CHITCPD_PORT_STRING (getenv("CHITCPD_PORT")? getenv("CHITCPD_PORT"): "23300")
#define GET_CHITCPD_PORT (atoi(GET_CHITCPD_PORT_STRING))

/* Returns location of UNIX socket */
int chitcp_unix_socket(char* buf, int buflen);

/* Macro for getting a sockfd from a pointer in the socket table */
#define ptr_to_fd(si, entry) ((int) (((char *)entry - (char *)si->chisocket_table) / sizeof(chisocketentry_t)))

#endif /* CHITCP_CHITCPD_H_ */
