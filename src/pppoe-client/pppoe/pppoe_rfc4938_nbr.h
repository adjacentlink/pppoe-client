/*-----------------------------------------------------------------------------/
 * project: rfc4938
 * file: pppoe_rfc4938_nbr.h
 * version: 1.0
 * date: October 4, 2007
 *
 * Copyright (c) 2013 - Adjacent Link, LLC, Bridgewater NJ
 * Copyright owner (c) 2007 by cisco Systems, Inc.
 *
 * ===========================
 * This is the header file for the functions to communicate with a neighbor
 * which also has the pppoe client side implmentation of rfc4938.
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

#ifndef __PPPOE_RFC4938_NBR_H__
#define __PPPOE_RFC4938_NBR_H__

#include "pppoe_rfc4938.h"
#include "../rfc4938_neighbor_manager.h"
#include "../rfc4938_messages.h"
#include "../rfc4938_types.h"


extern int  send_session_start(PPPoEConnection *conn);
extern int  send_session_start_ready(PPPoEConnection *conn);
extern int  send_child_ready(PPPoEConnection *conn);
extern int  send_child_session_up(PPPoEConnection *conn);
extern void send_child_session_terminated(PPPoEConnection *conn);
extern int  send_session_up (PPPoEConnection * conn);

extern int  handle_session_packet_to_peer(PPPoEConnection *conn, PPPoEPacket *packet, UINT16_t credits);
extern int  send_session_packet_to_peer(PPPoEConnection *conn, PPPoEPacket *packet, UINT16_t credits);
extern int  send_packet_to_conn(PPPoEConnection *conn, PPPoEPacket *packet, UINT16_t proto);
extern int  recv_packet_from_parent(PPPoEConnection *conn, PPPoEPacket *packet);

#endif
