/*-----------------------------------------------------------------------------/
 * project: rfc4938
 * file: rfc4938_parser.h
 * version: 1.0
 * date: October 21, 2007
 *
 * Copyright (c) 2013 - Adjacent Link, LLC, Bridgewater NJ
 * Copyright (C), 2007-2008 by Cisco Systems, Inc.
 *
 * ===========================
 *
 * These APIs are used to manage a pool of local port numbers
 * that are associated with client instances.  Port numbers
 * are allocated for use and freed when the client instance
 * is torn down.
 *
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
#ifndef __RFC4938_PARSER_H__
#define __RFC4938_PARSER_H__

#include "rfc4938_neighbor_manager.h"
#include "rfc4938_types.h"
#include "pppoe_types.h"

#include <unistd.h>

extern UINT32_t u32seqnum;

void rfc4938_parser_parse_upstream_packet (const void *recvbuf, int bufsize, UINT32_t neighbor_id);

void rfc4938_parser_parse_downstream_packet (const void *recvbuf, int bufsize, rfc4938_neighbor_element_t * nbr);

int rfc4938_parser_cli_initiate_session (UINT32_t neighbor_id, UINT16_t credit_scalar);

int rfc4938_parser_cli_terminate_session (UINT32_t neighbor_id, UINT16_t cmdSRC);

int rfc4938_parser_cli_padq_session (UINT32_t neighbor_id,
                UINT8_t receive_only,
                UINT8_t rlq,
                UINT8_t resources,
                UINT16_t latency,
                UINT16_t cdr_scale,
                UINT16_t current_data_rate,
                UINT16_t mdr_scale,
                UINT16_t max_data_rate);

int rfc4938_parser_cli_padg_session (UINT32_t neighbor_id, UINT16_t credits);

void rfc4938_parse_ppp_packet(const PPPoEPacket *packet, int framelen, const char * direction);

#endif
