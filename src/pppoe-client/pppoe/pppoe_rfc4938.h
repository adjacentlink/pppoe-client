/*-----------------------------------------------------------------------------/
 * project: rfc4938
 * file: pppoe_rfc4938.h
 * version: 1.0
 * date: October 4, 2007
 *
 * Copyright (c) 2013 - Adjacent Link, LLC, Bridgewater NJ
 * Copyright (C) 2007-2008, Cisco Systems, Inc.
 *
 * ===========================
 * This is the header file which implements functions related to
 * rfc4938, "PPP Over Ethernet (PPPoE) Extensions for Credit Flow and
 * Link Metrics" and "PPP Over Ethernet (PPPoE) Extensions for Scaled
 * Credits and Link Metrics"
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

#ifndef __PPPOE_RFC4938_H__
#define __PPPOE_RFC4938_H__

#include "pppoe.h"
#include "string.h"



#define INITIAL_GRANT          ( 1953 )
#define CREDIT_MISMATCH        ( 100 )
#define MAX_CREDITS            ( 0xffff )
#define RFC4938_CREDIT_SCALAR  ( 64 )
#define MAX_PADC_WAIT_TIME     ( 60 ) /* 60 seconds */
#define PADG_RETRY_TIME        ( 1 )  /* 1  seconds */

#define SCALING_KBPS           ( 0x00 )
#define SCALING_MBPS           ( 0x01 )
#define SCALING_GBPS           ( 0x10 )
#define SCALING_TBPS           ( 0x11 )

#define MAX_DEBUG_STRING       ( 2048 )

#define TRUE                   ( 1 )
#define FALSE                  ( 0 )

typedef union twobyte {
    UINT8_t  byte[2];
    UINT16_t word;
  } __attribute__((packed)) twobyte_t ;

/* function declarations */
extern void pppoe_signal_handler(int signo);

extern void pppoe_init_flow_control(PPPoEConnection *conn, UINT16_t credit_scalar, int rfc4938_debug,
                                    UINT16_t my_port, UINT16_t parent_port, UINT32_t peer_pid,
                                    UINT16_t grant_amount, UINT16_t timed_credits);


extern void handle_signal_event(PPPoEConnection *conn);


extern void sendPADG(PPPoEConnection *conn, UINT16_t credits);
extern void sendPADC(PPPoEConnection *conn, UINT16_t seq);
extern void recvPADG(PPPoEConnection *conn, PPPoEPacket *packet);
extern void recvPADC(PPPoEConnection *conn, PPPoEPacket *packet);
extern void sendPADQ(PPPoEConnection *conn, UINT16_t mdr, UINT8_t mdr_scalar, 
                     UINT16_t cdr, UINT8_t cdr_scalar, UINT16_t latency, 
                     UINT8_t resources, UINT8_t rlq, UINT8_t receive);
extern void recvPADQ(PPPoEConnection *conn, PPPoEPacket *packet);

extern void sendOutOfBandGrant(PPPoEConnection *conn, UINT16_t credits);
extern UINT16_t sendInBandGrant(PPPoEConnection *conn, PPPoEPacket *packet, UINT16_t credits);

extern int recvInBandGrant(PPPoEConnection *conn, PPPoEPacket *packet);

extern UINT16_t get_word_from_buff (UINT8_t * p, int offset);

extern void add_credit_tag(PPPoETag *tag, UINT16_t fcn, UINT16_t bcn);
extern void add_sequence_tag(PPPoETag *tag, UINT16_t seq);
extern void add_scalar_tag(PPPoETag *tag, UINT16_t scalar);

extern UINT16_t compute_local_credits (PPPoEConnection *conn, PPPoEPacket *packet);
extern UINT16_t compute_local_credits_with_inband (PPPoEConnection *conn, PPPoEPacket *packet);
extern UINT16_t compute_peer_credits (PPPoEConnection *conn, PPPoEPacket *packet);
extern UINT16_t compute_peer_credits_with_inband (PPPoEConnection *conn, PPPoEPacket *packet);

extern UINT16_t get_fcn_from_credit_tag(PPPoETag *tag);
extern UINT16_t get_bcn_from_credit_tag(PPPoETag *tag);
extern UINT16_t get_seq_from_sequence_tag(PPPoETag *tag);

extern void add_peer_credits(PPPoEConnection *conn, UINT16_t credits);
extern void del_peer_credits(PPPoEConnection *conn, UINT16_t credits);
#endif
