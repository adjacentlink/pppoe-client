/*-----------------------------------------------------------------------------/
 * project: rfc4938
 * file: rfc4938_types.h
 * version: 1.0
 * date: October 21, 2007
 *
 * Copyright (c) 2013 - Adjacent Link, LLC, Bridgewater NJ
 * Copyright (C), 2007-2008 by cisco Systems, Inc.
 *
 * ===========================
 *
 * This file provides a set of typedefs used to promote
 * variable consistency.  It also includes the C standard
 * errno.h file and defines SUCCESS for return code
 * consistency.
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


#ifndef __RFC4938_TYPES_H__
#define __RFC4938_TYPES_H__



#include <errno.h>

#include "pppoe_types.h"

/* errno does not provide success */
#ifndef SUCCESS
#define SUCCESS 0
#endif


/* Ethernet frame types according to RFC 2516 */
#ifndef ETH_PPPOE_DISCOVERY
#define ETH_PPPOE_DISCOVERY 0x8863
#endif

#ifndef ETH_PPPOE_SESSION
#define ETH_PPPOE_SESSION   0x8864
#endif

#define PPPOE_ETH_DATA_LEN 1500
#define PPPOE_ETH_ALEN 6

struct pppoe_ethhdr
{
    unsigned char  dest[PPPOE_ETH_ALEN];
    unsigned char  source[PPPOE_ETH_ALEN];
    unsigned short proto;
} __attribute__((packed));


struct pppoehdr
{
#if   __BYTE_ORDER ==__LITTLE_ENDIAN
    unsigned int type:4;
    unsigned int ver:4;
#elif  __BYTE_ORDER ==__BIG_ENDIAN
    unsigned int ver:4;
    unsigned int type:4;
#else
# error "Please fix <bits/endian.h>"
#endif
    unsigned char  code;
    unsigned short session;
    unsigned short length;
} __attribute__((packed));
#define pppoe_type    pppoe_hdr.type
#define pppoe_ver     pppoe_hdr.ver
#define pppoe_code    pppoe_hdr.code
#define pppoe_session pppoe_hdr.session
#define pppoe_length  pppoe_hdr.length


/* A PPPoE Packet, including Ethernet headers */
typedef struct PPPoEPacketStruct
{
    struct pppoe_ethhdr eth_hdr;              /* Ethernet header */

    struct pppoehdr pppoe_hdr;             /* PPPoE headr */

    unsigned char  payload[PPPOE_ETH_DATA_LEN]; /* A bit of room to spare */
} __attribute__((packed)) PPPoEPacket;


typedef struct PPPHeaderStruct
{
    unsigned short type;       /* type */
    unsigned char  code;       /* code */
    unsigned char  id;             /* id */
    unsigned short length;         /* length */
    unsigned char data[0];         /* data */
} __attribute__((packed)) PPPHeader;

/* PPPoE Tag */

typedef struct PPPoETagStruct
{
    unsigned short type;              /* tag type */
    unsigned short length;            /* Length of payload */
    unsigned char  payload[PPPOE_ETH_DATA_LEN]; /* A LOT of room to spare */
} __attribute__((packed)) PPPoETag;


/* Header size of a PPPoE packet */
#define ETH_PPPOE_OVERHEAD (ETH_OVERHEAD + PPPOE_OVERHEAD)
#define MAX_PPPOE_PAYLOAD (PPPOE_ETH_DATA_LEN - PPPOE_OVERHEAD)
#define MAX_PPPOE_MTU (MAX_PPPOE_PAYLOAD - 2)


/* Header size of a PPPoE tag */
#define TAG_HDR_SIZE 4

/* PPP types */
#define PPP_LCP             0xc021
#define PPP_IPCP            0x8021


#define PPP_CONFIG_REQ     0x01
#define PPP_CONFIG_ACK     0x02
#define PPP_CONFIG_NAK     0x03
#define PPP_CONFIG_REJECT  0x04
#define PPP_TERMINATE_REQ  0x05
#define PPP_TERMINATE_ACK  0x06
#define PPP_CODE_REJECT    0x07
#define PPP_ECHO_REQ       0x09
#define PPP_ECHO_REPLY     0x0a

#define LCP_VENDOR_SPECIFIC  0
#define LCP_MAX_RECV_UNIT    1
#define LCP_AUTH_PROTOCOL    3
#define LCP_QUALITY_PROTOCOL 4
#define LCP_MAGIC_NUMBER     5

#define PPP_IPCP_OPT_IP_ADDRESSES   1
#define PPP_IPCP_OPT_COMPRESSION    2
#define PPP_IPCP_OPT_IP_ADDRESS     3
#define PPP_IPCP_OPT_MOBILE_IPV4    4

#define ETH_OVERHEAD   14
#define PPPOE_OVERHEAD  6
#define PPP_OVERHEAD    2

#define PIPE_RD_FD 0
#define PIPE_WR_FD 1
#endif


/* PPPoE codes */
#define CODE_PADI           ( 0x09 )
#define CODE_PADO           ( 0x07 )
#define CODE_PADR           ( 0x19 )
#define CODE_PADS           ( 0x65 )
#define CODE_PADT           ( 0xA7 )
#define CODE_PADG           ( 0x0A )
#define CODE_PADC           ( 0x0B )
#define CODE_PADQ           ( 0x0C )
#define CODE_SESS           ( 0x00 )

#define TAG_RFC4938_CREDITS    ( 0x0106 )
#define TAG_RFC4938_METRICS    ( 0x0107 )
#define TAG_RFC4938_SEQ_NUM    ( 0x0108 )
#define TAG_RFC4938_SCALAR     ( 0x0109 )

#define TAG_CREDITS_LENGTH     ( 0x4 )
#define TAG_METRICS_LENGTH     ( 0xa )
#define TAG_SEQ_LENGTH         ( 0x2 )
#define TAG_SCALAR_LENGTH      ( 0x2 )


