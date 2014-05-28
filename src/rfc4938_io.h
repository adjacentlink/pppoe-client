/*-----------------------------------------------------------------------------/
 * project: rfc4938
 * file: rfc4938.h
 * version: 1.0
 * date: October 4, 2007
 *
 * Copyright (c) 2013 - Adjacent Link, LLC, Bridgewater NJ
 * Copyright (C) 2007-2008 by Cisco Systems, Inc.
 *
 * ===========================
 * This is the header file for rfc4938.c and rfc4938ctl.c
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

#ifndef __RFC4938_IO_H__
#define __RFC4938_IO_H__

#include "pppoe_types.h"
#include "rfc4938_neighbor_manager.h"

#include <sys/types.h>
#include <unistd.h>

int rfc4938_io_send_udp_packet(char *ip, UINT16_t port, const void *p2buffer, int buflen);

void rfc4938_io_send_to_nbr(UINT32_t neighbor_id, UINT16_t credits, const void *p2buffer, int buflen);

void rfc4938_io_forward_to_child(UINT32_t neighbor_id, const void *p2buffer, int buflen, rfc4938_neighbor_element_t *nbr);

int rfc4938_io_send_to_child(UINT16_t port, const void *p2buffer, int buflen);

int rfc4938_io_send_frame_to_device(const void *p2buffer, int buflen, UINT16_t proto);

int rfc4938_io_listen_for_messages(void);

int rfc4938_io_get_messages(fd_set readable, int num_fd_ready);

void rfc4938_io_handle_signal_event();

int rfc4938_io_open_vif(const char *ifname,  UINT8_t *hwaddr);

int rfc4938_io_open_interface(char const *ifname, UINT16_t type, unsigned char *hwaddr);

int rfc4938_io_signal_pipe[2]; 

int rfc4938_io_get_udp_socket(int req_port, int * real_port);

extern int rfc4938_vif_fd; 

extern int rfc4938_eth_sfd;

extern int rfc4938_eth_dfd;

#endif
