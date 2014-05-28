/*-----------------------------------------------------------------------------/
 * project: rfc4938
 * file: rfc4938_transport.h
 * version: 1.0
 * date: April 11, 2013
 *
 * Copyright (c) 2013 - Adjacent Link, LLC, Bridgewater NJ
 *
 * ===========================
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *----------------------------------------------------------------------------*/

#ifndef RFC4938_TRANSPORT_H
#define RFC4938_TRANSPORT_H

#include <sys/types.h>
#include <unistd.h>

void rfc4938_transport_send(unsigned short dst, unsigned short credits, const void * p2buffer, int buflen);

int  rfc4938_transport_setup(const char *platform, const char *transport, unsigned long id);

void rfc4938_transport_enable_hellos(float fInterval);

void rfc4938_transport_cleanup(void);

void rfc4938_transport_neighbor_terminated(UINT32_t neighbor_id);

void rfc4938_transport_notify_fd_ready(fd_set fdready, int num);


#endif // RFC4938_TRANSPORT_H
