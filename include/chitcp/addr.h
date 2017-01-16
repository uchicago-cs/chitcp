/*
 *  chiTCP - A simple, testable TCP stack
 *
 *  sockaddr manipulations functions
 *
 *  These functions will work with both IPv4 and IPv6 addresses.
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

#ifndef CHITCP_ADDR_H_
#define CHITCP_ADDR_H_

#include <arpa/inet.h>


/*
 *  chitcp_get_addr_port - Gets the port from a sockaddr
 *
 *  addr: The address
 *
 *  Returns: The port in network order
 */
in_port_t chitcp_get_addr_port(struct sockaddr *addr);


/*
 * chitcp_set_addr_port - Sets the port in a sockaddr
 *
 * addr: The address
 *
 * port: The port in network order
 *
 * Returns: Nothing.
 */
void chitcp_set_addr_port(struct sockaddr *addr, in_port_t port);


/*
 * chitcp_addr_cmp - Compares two addresses
 *
 * addr1, addr2: The addresses
 *
 * Returns: 0 if the addresses are identical (IP address and TCP port)
 *          Non-zero value otherwise.
 */
int chitcp_addr_cmp(struct sockaddr *addr1, struct sockaddr *addr2);

int chitcp_addr_port_cmp(struct sockaddr *addr1, struct sockaddr *addr2);


/*
 * chitcp_addr_is_loopback - Checks whether an address is the loopback address
 *
 * addr: Address
 *
 * Returns: Non-zero value if address is the IPv4 or IPv6 loopback address.
 *          Zero otherwise.
 */
int chitcp_addr_is_loopback(struct sockaddr *addr);


/*
 * chitcp_addr_str - Creates string representation of address
 *
 * addr: Address
 *
 * buf: Pointer to buffer
 *
 * len: Length of buffer.
 *
 * Returns: Stores the string representation in buf (without exceeding
 *          it length) and returns a pointer to the buffer.
 *          Returns NULL if it was unable to create a string representation
 *          (e.g., if the provided address is invalid)
 */
char* chitcp_addr_str(struct sockaddr *addr, char *buf, int len);

void* chitcp_get_addr(struct sockaddr *addr);

int chitcp_addr_is_any(struct sockaddr *addr);

int chitcp_addr_set_any(struct sockaddr *addr);

int chitcp_addr_construct(char *host, char *port, struct sockaddr_in *addr);

#endif /* CHITCP_ADDR_H_ */
