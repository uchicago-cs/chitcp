/*
 *  chiTCP - A simple, testable TCP stack
 *
 * debug breakpoint insertion functions
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

#ifndef BREAKPOINT_H_
#define BREAKPOINT_H_

#include "serverinfo.h"
#include "chitcp/debug_api.h"


int chitcpd_init_debug_connection(serverinfo_t *si, int sockfd, int event_flags, int client_socket);


/*
 * chitcpd_debug_breakpoint - An easy interface for adding a breakpoint to
 *      the daemon. All arguments except SI will be forwarded to the client's
 *      debug monitor, and the client will return a code describing which
 *      action to take (see debug_api.h).
 *
 * si           - server info structure
 * sockfd       - the socket that generated the debug event
 * event_flag   - the type of the debug event
 * new_sockfd   - if EVENT_FLAG is DBG_EVT_PENDING_CONNECTION, new_sockfd is
 *                  the fd for the newly created (active) socket
 *
 * Returns: DBG_RESP_NONE                                        - SOCKFD is invalid
 *          any chitcpd_debug_response (including DBG_RESP_NONE) - the return code from the user's debug_event_handler
 *          
 */
enum chitcpd_debug_response chitcpd_debug_breakpoint(serverinfo_t *si, int sockfd, int event_flag, int new_sockfd);

/*
 * When closing a chisocketentry_t,
 * call this function to make sure that its debug_monitor is destroyed
 * once no chisockets refer to it.
 */
void chitcpd_debug_detach_monitor(chisocketentry_t *entry);

#endif /* BREAKPOINT_H_ */
