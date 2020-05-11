/***********************************************************************
*
* if.c
*
* Implementation of user-space PPPoE redirector for Linux.
*
* Functions for opening a raw socket and reading/writing raw Ethernet frames.
*
* Copyright (c) 2013 - Adjacent Link, LLC, Bridgewater NJ
* Copyright (C) 2000 by Roaring Penguin Software Inc.
*
* This program may be distributed according to the terms of the GNU
* General Public License, version 2 or (at your option) any later version.
*
* LIC: GPL
*
***********************************************************************/

//static char const RCSID[] = "$Id: if.c,v 1.18 2006/01/03 03:05:06 dfs Exp $";

#include "pppoe.h"
#include "../rfc4938.h"
#include "pppoe_rfc4938_nbr.h"

#include <errno.h>
#include <memory.h>
#include <sys/ioctl.h>


/* Initialize frame types to RFC 2516 values.  Some broken peers apparently
   use different frame types... sigh... */

UINT16_t Eth_PPPOE_Discovery = ETH_PPPOE_DISCOVERY;
UINT16_t Eth_PPPOE_Session   = ETH_PPPOE_SESSION;

/**********************************************************************
*%FUNCTION: getEtherType
*%ARGUMENTS:
* packet -- a received PPPoE packet
*%RETURNS:
* ethernet packet type (see /usr/include/net/ethertypes.h)
*%DESCRIPTION:
* Checks the ethernet packet header to determine its type.
* We should only be receveing DISCOVERY and SESSION types if the BPF
* is set up correctly.  Logs an error if an unexpected type is received.
* Note that the ethernet type names come from "pppoe.h" and the packet
* packet structure names use the LINUX dialect to maintain consistency
* with the rest of this file.  See the BSD section of "pppoe.h" for
* translations of the data structure names.
***********************************************************************/
UINT16_t
getEtherType (PPPoEPacket * packet)
{
    UINT16_t type = (UINT16_t) ntohs (packet->eth_hdr.proto);

    if (type != Eth_PPPOE_Discovery && type != Eth_PPPOE_Session)
    {
        LOGGER(LOG_ERR, "Invalid pppoe ether type 0x%x", type);
    }

    return type;
}





int
send_session_packet_to_ac (PPPoEConnection * conn, PPPoEPacket * packet)
{
    return (send_packet_to_ac(conn, packet, Eth_PPPOE_Session));
}


int
send_discovery_packet_to_ac (PPPoEConnection * conn, PPPoEPacket * packet)
{
    return (send_packet_to_ac(conn, packet, Eth_PPPOE_Discovery));
}

