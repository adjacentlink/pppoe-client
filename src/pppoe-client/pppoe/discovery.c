/***********************************************************************
*
* discovery.c
*
* Perform PPPoE discovery
*
* Copyright (c) 2013 - Adjacent Link, LLC, Bridgewater NJ
* Copyright (C) 2007-2008 by Cisco Systems, Inc.
* Copyright (C) 1999 by Roaring Penguin Software Inc.
*
* LIC: GPL
*
* This file was modified on Feb 2008 by Cisco Systems, Inc.
***********************************************************************/

//static char const RCSID[] = "$Id: discovery.c,v 1.25 2006/01/03 03:20:38 dfs Exp $";

#include "pppoe.h"
#include "pppoe_rfc4938.h"
#include "pppoe_rfc4938_nbr.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <time.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <fcntl.h>

#include <signal.h>

static int waitOnDiscoverySocket(PPPoEConnection * conn, int timeout, PPPoEPacket * packet);


/**********************************************************************
*%FUNCTION: parseForHostUniq
*%ARGUMENTS:
* type -- tag type
* len -- tag length
* data -- tag data.
* extra -- user-supplied pointer.  This is assumed to be a pointer to int.
*%RETURNS:
* Nothing
*%DESCRIPTION:
* If a HostUnique tag is found which matches our PID, sets *extra to 1.
***********************************************************************/
static void
parseForHostUniq (UINT16_t type, UINT16_t len, unsigned char *data, void *extra)
{
    int *val = (int *) extra;

    if (type == TAG_HOST_UNIQ && len == sizeof (UINT32_t))
    {
        UINT32_t tmp;

        memcpy (&tmp, data, len);

        if (ntohl (tmp) == get_pppoe_conn()->host_id)
        {
            *val = 1;
        }
        else
        {
            LOGGER(LOG_PKT, "pkt id %u (0x%x) != our id %u (0x%x)\n",
                                htonl(tmp), htonl(tmp),
                                get_pppoe_conn()->host_id, get_pppoe_conn()->host_id);
        }
    }
}

/**********************************************************************
*%FUNCTION: packetIsForMe
*%ARGUMENTS:
* conn -- PPPoE connection info
* packet -- a received PPPoE packet
*%RETURNS:
* 1 if packet is for this PPPoE daemon; 0 otherwise.
*%DESCRIPTION:
* If we are using the Host-Unique tag, verifies that packet contains
* our unique identifier.
***********************************************************************/
static int
packetIsForMe (PPPoEConnection * conn, PPPoEPacket * packet)
{
    int forMe = 0;

    /* If packet is not directed to our MAC address, forget it */
    if (memcmp (packet->eth_hdr.dest, conn->myEth, PPPOE_ETH_ALEN))
    {
        LOGGER(LOG_PKT, "(%u,%hu): no, not out dst mac addr\n",
                            conn->peer_id, conn->sessionId);
        return 0;
    }

    /* If we're not using the Host-Unique tag, then accept the packet */
    if (!conn->useHostUniq)
    {
        return 1;
    }

    parseDiscoveryPacket (packet, parseForHostUniq, &forMe);

    return forMe;
}

/**********************************************************************
*%FUNCTION: parsePADOTags
*%ARGUMENTS:
* type -- tag type
* len -- tag length
* data -- tag data
* extra -- extra user data.  Should point to a PacketCriteria structure
*          which gets filled in according to selected AC name and service
*          name.
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Picks interesting tags out of a PADO packet
***********************************************************************/
static void
parsePADOTags (UINT16_t type, UINT16_t len, unsigned char *data, void *extra)
{
    struct PacketCriteria *pc = (struct PacketCriteria *) extra;
    PPPoEConnection *conn = pc->conn;

    switch (type)
    {
    case TAG_AC_NAME:
        pc->seenACName = 1;

        if (conn->acName && len == strlen (conn->acName) &&
                !strncmp ((char *) data, conn->acName, len))
        {
            pc->acNameOK = 1;
        }
        break;

    case TAG_SERVICE_NAME:
        pc->seenServiceName = 1;

        if (conn->serviceName && len == strlen (conn->serviceName) &&
                !strncmp ((char *) data, conn->serviceName, len))
        {
            pc->serviceNameOK = 1;
        }
        break;

    case TAG_AC_COOKIE:

        conn->cookie.type = htons (type);
        conn->cookie.length = htons (len);
        memcpy (conn->cookie.payload, data, len);
        break;

    case TAG_RELAY_SESSION_ID:
        conn->relayId.type = htons (type);
        conn->relayId.length = htons (len);
        memcpy (conn->relayId.payload, data, len);
        break;

    case TAG_SERVICE_NAME_ERROR:
        pktLogErrs ("PADO", type, len, data, extra);
        exit (1);
        break;

    case TAG_AC_SYSTEM_ERROR:
        pktLogErrs ("PADO", type, len, data, extra);
        exit (1);
        break;

    case TAG_GENERIC_ERROR:
        pktLogErrs ("PADO", type, len, data, extra);
        exit (1);
        break;
    }
}

/**********************************************************************
*%FUNCTION: parsePADSTags
*%ARGUMENTS:
* type -- tag type
* len -- tag length
* data -- tag data
* extra -- extra user data (pointer to PPPoEConnection structure)
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Picks interesting tags out of a PADS packet
***********************************************************************/
static void
parsePADSTags (UINT16_t type, UINT16_t len, unsigned char *data, void *extra)
{
    PPPoEConnection *conn = (PPPoEConnection *) extra;
    UINT16_t fcn;
    UINT16_t bcn;
    UINT16_t credit_scalar;

    switch (type)
    {
    case TAG_SERVICE_NAME:
        LOGGER(LOG_PKT, "(%u,%hu): PADS Service-Name: '%.*s'\n",
                            conn->peer_id, conn->sessionId, (int) len, data);
        break;

    case TAG_GENERIC_ERROR:
    case TAG_AC_SYSTEM_ERROR:
    case TAG_SERVICE_NAME_ERROR:
        pktLogErrs ("PADS", type, len, data, extra);
        conn->PADSHadError = 1;
        break;

    case TAG_RELAY_SESSION_ID:
        conn->relayId.type   = htons (type);
        conn->relayId.length = htons (len);
        memcpy (conn->relayId.payload, data, len);
        break;

    case TAG_RFC4938_CREDITS:
        fcn = get_word_from_buff(data, 0);
        bcn = get_word_from_buff(data, 2);

        LOGGER(LOG_PKT, "(%u,%hu): PADS: fcn:%hu, bcn:%hu\n",
                            conn->peer_id, conn->sessionId, fcn, bcn);

        /* add credits */
        handle_credit_grant(conn, fcn, bcn);

        break;

    case TAG_RFC4938_SCALAR:
        if (conn->mode == MODE_RFC4938_SCALING)
        {
            credit_scalar = get_word_from_buff(data, 0);
            conn->peer_credit_scalar = credit_scalar;
            conn->scalar_state = SCALAR_RECEIVED;

            LOGGER(LOG_PKT, "(%u,%hu): Received credit scalar:%hu, in PADS\n",
                                conn->peer_id, conn->sessionId, credit_scalar);
        }
        else
        {

            /*
             * Something is wrong. The user has requested a rfc4938 session only,
             * one without credit/metric scaling but the peer has sent a credit
             * scaling tag.  We're going to send a padt and let the user figure
             * it out.
             */

            LOGGER(LOG_PKT, "(%u,%hu): A scaling tag was detected in the PADS packet"
                                " but the session was RFC4938_ONLY\n",
                                conn->peer_id, conn->sessionId);

            sendPADTandExit (conn, "PPPoEClient: credit/scaling mismatch", 1);
        }

        break;
    }
}

/***********************************************************************
*%FUNCTION: sendPADI
*%ARGUMENTS:
* conn -- PPPoEConnection structure
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Sends a PADI packet
***********************************************************************/
static void
sendPADI (PPPoEConnection * conn)
{
    PPPoEPacket packet;
    unsigned char *cursor = packet.payload;
    PPPoETag *svc = (PPPoETag *) (&packet.payload);
    UINT16_t namelen = 0;
    UINT16_t plen;
    int omit_service_name = 0;

    if (conn->sessionId)
    {
        LOGGER(LOG_INFO, "(%u): already have session id %u, not sending PADI\n",
                           conn->peer_id, conn->sessionId);
        return;
    }

    if (conn->serviceName)
    {
        namelen = (UINT16_t) strlen (conn->serviceName);
        if (!strcmp (conn->serviceName, "NO-SERVICE-NAME-NON-RFC-COMPLIANT"))
        {
            omit_service_name = 1;
        }
    }

    memset(&packet, 0x0, sizeof(packet));

    /* Set destination to Ethernet broadcast address */
    memset (packet.eth_hdr.dest, 0xFF, PPPOE_ETH_ALEN);
    memcpy (packet.eth_hdr.source, conn->myEth, PPPOE_ETH_ALEN);

    packet.eth_hdr.proto = htons (Eth_PPPOE_Discovery);
    packet.pppoe_ver = 1;
    packet.pppoe_type = 1;
    packet.pppoe_code = CODE_PADI;
    packet.pppoe_session = 0;

    if (!omit_service_name)
    {
        plen = TAG_HDR_SIZE + namelen;
        CHECK_ROOM (cursor, packet.payload, plen);

        svc->type = TAG_SERVICE_NAME;
        svc->length = htons (namelen);

        if (conn->serviceName)
        {
            memcpy (svc->payload, conn->serviceName, strlen (conn->serviceName));
        }
        cursor += namelen + TAG_HDR_SIZE;
    }
    else
    {
        plen = 0;
    }

    /* If we're using Host-Uniq, copy it over */
    if (conn->useHostUniq)
    {
        PPPoETag hostUniq;
        UINT32_t id = htonl (conn->host_id);

        hostUniq.type   = htons (TAG_HOST_UNIQ);
        hostUniq.length = htons (sizeof (id));

        memcpy (hostUniq.payload, &id, sizeof (id));
        CHECK_ROOM (cursor, packet.payload, sizeof (id) + TAG_HDR_SIZE);

        memcpy (cursor, &hostUniq, sizeof (id) + TAG_HDR_SIZE);
        cursor += sizeof (id) + TAG_HDR_SIZE;
        plen   += sizeof (id) + TAG_HDR_SIZE;
    }

    LOGGER(LOG_PKT, "(%u,%hu): sending PADI len %d\n",
                        conn->peer_id, conn->sessionId, (int) (plen + ETH_PPPOE_OVERHEAD));

    packet.pppoe_length = htons (plen);

    send_discovery_packet_to_conn (conn, &packet);
}

/**********************************************************************
*%FUNCTION: waitForPADO
*%ARGUMENTS:
* conn -- PPPoEConnection structure
* timeout -- how long to wait (in seconds)
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Waits for a PADO packet and copies useful information
***********************************************************************/
static void
waitForPADO (PPPoEConnection * conn, int timeout)
{
    PPPoEPacket packet;

    struct PacketCriteria pc;
    pc.conn = conn;
    pc.acNameOK = (conn->acName) ? 0 : 1;
    pc.serviceNameOK = (conn->serviceName) ? 0 : 1;
    pc.seenACName = 0;
    pc.seenServiceName = 0;

    const int endtime = time(NULL) + timeout;

    memset(&packet, 0x0, sizeof(packet));
    do
    {
        int r;

        if(timeout > 0)
        {
            r = waitOnDiscoverySocket (conn, timeout, &packet);
        }
        else
        {
            LOGGER(LOG_INFO, "(%u,%hu, 0x%x): no more time, done\n",
                               conn->peer_id, conn->sessionId, conn->host_id);

            return;
        }

        if (r == 0)
        {
            LOGGER(LOG_PKT, "(%u,%hu, 0x%x): ignore frame, continue\n",
                                conn->peer_id, conn->sessionId, conn->host_id);

            timeout = endtime - time(NULL);

            continue;
        }
        else if (r == -2)
        {
            LOGGER(LOG_INFO, "(%u,%hu,0x%x): timed out, done\n",
                               conn->peer_id, conn->sessionId, conn->host_id);

            return; /* Timed out */
        }
        else if (r == -1)
        {
            LOGGER(LOG_PKT, "(%u,%hu): read error, terminate\n",
                                conn->peer_id, conn->sessionId);

            fatalSys ("select (PADO discovery)", strerror(errno));

            return;
        }

        /* Check length */
        if ((int) (ntohs (packet.pppoe_length) + ETH_PPPOE_OVERHEAD) > r)
        {
            LOGGER(LOG_ERR, "(%u,%hu): Bogus PPPoE length field (%hu)\n",
                               conn->peer_id, conn->sessionId, ntohs (packet.pppoe_length));


            timeout = endtime - time(NULL);

            continue;
        }

        /* If it's not for us, loop again */
        if (!packetIsForMe (conn, &packet))
        {
            LOGGER(LOG_PKT, "(%u,%hu,0x%x): Frame not for me, drop\n",
                                conn->peer_id, conn->sessionId, conn->host_id);

            timeout = endtime - time(NULL);

            continue;
        }

        if (packet.pppoe_code == CODE_PADO)
        {
            if (NOT_UNICAST (packet.eth_hdr.source))
            {
                LOGGER(LOG_ERR, "(%u,%hu): Ignoring PADO packet from "
                                   "non-unicast MAC address",
                                   conn->peer_id, conn->sessionId);

                timeout = endtime - time(NULL);

                continue;
            }

            parseDiscoveryPacket (&packet, parsePADOTags, &pc);

            if (!pc.seenACName)
            {
                LOGGER(LOG_ERR, "(%u,%hu): Ignoring PADO packet with no "
                                   "AC-Name tag", conn->peer_id, conn->sessionId);

                timeout = endtime - time(NULL);

                continue;
            }

            if (!pc.seenServiceName)
            {
                LOGGER(LOG_ERR, "(%u,%hu): Ignoring PADO packet with "
                                   "no Service-Name tag", conn->peer_id, conn->sessionId);

                timeout = endtime - time(NULL);

                continue;
            }

            conn->numPADOs++;

            if (pc.acNameOK && pc.serviceNameOK)
            {
                memcpy (conn->peerEth, packet.eth_hdr.source, PPPOE_ETH_ALEN);

                LOGGER(LOG_PKT, "(%u,%hu,0x%x):PADO received, AC-Ethernet-Address: %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
                                    conn->peer_id, conn->sessionId, conn->host_id,
                                    conn->peerEth[0], conn->peerEth[1], conn->peerEth[2],
                                    conn->peerEth[3], conn->peerEth[4], conn->peerEth[5]);

                conn->discoveryState = STATE_RECEIVED_PADO;

                break;
            }
        }
    }
    while (conn->discoveryState != STATE_RECEIVED_PADO);
}

/***********************************************************************
*%FUNCTION: sendPADR
*%ARGUMENTS:
* conn -- PPPoE connection structur
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Sends a PADR packet
***********************************************************************/
static void
sendPADR (PPPoEConnection * conn)
{
    PPPoEPacket packet;
    PPPoETag *svc = (PPPoETag *) packet.payload;
    unsigned char *cursor = packet.payload;

    UINT16_t namelen = 0;
    UINT16_t plen;

    if (conn->sessionId)
    {
        LOGGER(LOG_INFO, "(%u): already have session id %u, not sending PADR\n",
                           conn->peer_id, conn->sessionId);
        return;
    }

    if (conn->serviceName)
    {
        namelen = (UINT16_t) strlen (conn->serviceName);
    }

    plen = TAG_HDR_SIZE + namelen;
    CHECK_ROOM (cursor, packet.payload, plen);

    memset(&packet, 0x0, sizeof(packet));

    memcpy (packet.eth_hdr.dest, conn->peerEth, PPPOE_ETH_ALEN);
    memcpy (packet.eth_hdr.source, conn->myEth, PPPOE_ETH_ALEN);

    packet.eth_hdr.proto = htons (Eth_PPPOE_Discovery);
    packet.pppoe_ver = 1;
    packet.pppoe_type = 1;
    packet.pppoe_code = CODE_PADR;
    packet.pppoe_session = 0;

    svc->type = TAG_SERVICE_NAME;
    svc->length = htons (namelen);
    if (conn->serviceName)
    {
        memcpy (svc->payload, conn->serviceName, namelen);
    }
    cursor += namelen + TAG_HDR_SIZE;

    /* If we're using Host-Uniq, copy it over */
    if (conn->useHostUniq)
    {
        PPPoETag hostUniq;
        UINT32_t id = htonl (conn->host_id);

        hostUniq.type   = htons (TAG_HOST_UNIQ);
        hostUniq.length = htons (sizeof (id));

        memcpy (hostUniq.payload, &id, sizeof (id));
        CHECK_ROOM (cursor, packet.payload, sizeof (id) + TAG_HDR_SIZE);

        memcpy (cursor, &hostUniq, sizeof (id) + TAG_HDR_SIZE);

        cursor += sizeof (id) + TAG_HDR_SIZE;
        plen   += sizeof (id) + TAG_HDR_SIZE;
    }

    /* Copy cookie and relay-ID if needed */
    if (conn->cookie.type)
    {
        CHECK_ROOM (cursor, packet.payload, ntohs (conn->cookie.length) + TAG_HDR_SIZE);

        memcpy (cursor, &conn->cookie, ntohs (conn->cookie.length) + TAG_HDR_SIZE);
        cursor += ntohs (conn->cookie.length) + TAG_HDR_SIZE;
        plen   += ntohs (conn->cookie.length) + TAG_HDR_SIZE;
    }

    if (conn->relayId.type)
    {
        CHECK_ROOM (cursor, packet.payload, ntohs (conn->relayId.length) + TAG_HDR_SIZE);

        memcpy (cursor, &conn->relayId, ntohs (conn->relayId.length) + TAG_HDR_SIZE);
        cursor += ntohs (conn->relayId.length) + TAG_HDR_SIZE;
        plen   += ntohs (conn->relayId.length) + TAG_HDR_SIZE;
    }

    /* add credit tag */
    PPPoETag creditTag;
    add_credit_tag (&creditTag, conn->grant_limit, 0);

    CHECK_ROOM (cursor, packet.payload, TAG_CREDITS_LENGTH + TAG_HDR_SIZE);

    memcpy (cursor, &creditTag, TAG_CREDITS_LENGTH + TAG_HDR_SIZE);
    cursor += TAG_CREDITS_LENGTH + TAG_HDR_SIZE;
    plen   += TAG_CREDITS_LENGTH + TAG_HDR_SIZE;

    LOGGER(LOG_PKT, "(%u,%hu): add credit tag, fcn:%hu, bcn:%hu, len is now  %d\n",
                        conn->peer_id, conn->sessionId,
                        conn->grant_limit, 0, (int) (plen + ETH_PPPOE_OVERHEAD));

    /* add credit scalar tag if requested */
    if (conn->mode == MODE_RFC4938_SCALING)
    {
        PPPoETag scalarTag;

        add_scalar_tag (&scalarTag, conn->local_credit_scalar);
        CHECK_ROOM (cursor, packet.payload, TAG_SCALAR_LENGTH + TAG_HDR_SIZE);

        memcpy (cursor, &scalarTag, TAG_SCALAR_LENGTH + TAG_HDR_SIZE);
        cursor += TAG_SCALAR_LENGTH + TAG_HDR_SIZE;
        plen   += TAG_SCALAR_LENGTH + TAG_HDR_SIZE;

        LOGGER(LOG_PKT, "(%u,%hu): add credit tag, credit scalar:%hu, len is now %d\n",
                            conn->peer_id, conn->sessionId,
                            conn->local_credit_scalar, (int) (plen + ETH_PPPOE_OVERHEAD));
    }

    packet.pppoe_length = htons (plen);

    send_discovery_packet_to_conn (conn, &packet);
}

/**********************************************************************
*%FUNCTION: waitForPADS
*%ARGUMENTS:
* conn -- PPPoE connection info
* timeout -- how long to wait (in seconds)
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Waits for a PADS packet and copies useful information
***********************************************************************/
static void
waitForPADS (PPPoEConnection * conn, int timeout)
{
    PPPoEPacket packet;

    const int endtime = time(NULL) + timeout;

    do
    {
        int r;

        memset(&packet, 0x0, sizeof(packet));

        if(timeout > 0)
        {
            r = waitOnDiscoverySocket (conn, timeout, &packet);
        }
        else
        {
            LOGGER(LOG_INFO, "(%u,%hu, 0x%x): no more time, done\n",
                               conn->peer_id, conn->sessionId, conn->host_id);

            return;
        }

        if (r == 0)
        {
            LOGGER(LOG_PKT, "(%u,%hu, 0x%x): ignore frame, continue\n",
                                conn->peer_id, conn->sessionId, conn->host_id);

            timeout = endtime - time(NULL);

            continue;
        }
        else if (r == -2)
        {
            LOGGER(LOG_INFO, "(%u,%hu): timed out, done\n", conn->peer_id, conn->sessionId);

            return; /* Timed out */
        }
        else if (r < 0)
        {
            LOGGER(LOG_PKT, "(%u,%hu): read error, terminate\n", conn->peer_id, conn->sessionId);

            fatalSys ("select (PADS discovery)", strerror(errno));

            return;
        }

        /* Check length */
        if ((int) (ntohs (packet.pppoe_length) + ETH_PPPOE_OVERHEAD) != r)
        {
            LOGGER(LOG_ERR, "(%u,%hu): Bogus PPPoE length field (%hu) != %d\n", 
                               conn->peer_id, conn->sessionId, ntohs (packet.pppoe_length), r);

            timeout = endtime - time(NULL);

            continue;
        }

        /* If it's not from the AC, it's not for me */
        if (memcmp (packet.eth_hdr.source, conn->peerEth, PPPOE_ETH_ALEN))
        {
            LOGGER(LOG_PKT, "(%u,%hu,0x%x): Frame not from the AC, drop\n",
                                conn->peer_id, conn->sessionId, conn->host_id);


            timeout = endtime - time(NULL);

            continue;
        }

        /* If it's not for us, loop again */
        if (!packetIsForMe (conn, &packet))
        {
            LOGGER(LOG_PKT, "(%u,%hu,0x%x): Frame not for me, drop\n",
                                conn->peer_id, conn->sessionId, conn->host_id);

            timeout = endtime - time(NULL);

            continue;
        }

        /* Is it PADS?  */
        if (packet.pppoe_code == CODE_PADS)
        {
            /* Parse for goodies */
            conn->PADSHadError = 0;

            parseDiscoveryPacket (&packet, parsePADSTags, conn);

            if (!conn->PADSHadError)
            {
                conn->discoveryState = STATE_SESSION;
            }

            if (conn->scalar_state == SCALAR_NEEDED)
            {
                LOGGER(LOG_PKT, "(%u,%hu,0x%x): PADS did NOT have scalar info, falling back to default\n",
                                    conn->peer_id, conn->sessionId, conn->host_id);

                conn->local_credit_scalar = RFC4938_CREDIT_SCALAR;

                /*
                 * Time to bail.  We sent a credit scalar, but we did not received
                 * a scalar back, this is not compliant with the spec.
                 */

                /* rp_fatal ("Did not receive credit scalar tag when one was sent.\n"); */
            }
        }
    }
    while (conn->discoveryState != STATE_SESSION);

    /* save in host byte order */
    conn->sessionId = ntohs (packet.pppoe_session);

    LOGGER(LOG_INFO, "(%u,%hu,0x%x): PADS received, PPP session is %hu (0x%hx)\n",
                       conn->peer_id, conn->sessionId, conn->host_id,
                       conn->sessionId, conn->sessionId);

    /* RFC 2516 says session id MUST NOT be zero or 0xFFFF */
    if (conn->sessionId == 0 || conn->sessionId == 0xFFFF)
    {
        LOGGER(LOG_ERR, "(%u,%hu)): Access concentrator used a session value of"
                           " %hx -- the AC is violating RFC 2516\n", 
                           conn->peer_id, conn->sessionId, conn->sessionId);
    }

    /* Alert parent rfc4938 process of our session id*/
    send_session_up (conn);
}

/**********************************************************************
*%FUNCTION: doDiscovery
*%ARGUMENTS:
* conn -- PPPoE connection info structure
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Performs the PPPoE discovery phase
***********************************************************************/
void
doDiscovery (PPPoEConnection * conn)
{
    int padiAttempts = 0;
    int padrAttempts = 0;

    int timeout = conn->discoveryTimeout;

    do
    {
        padiAttempts++;

        if (padiAttempts > MAX_PADI_ATTEMPTS)
        {
            rp_fatal ("Timeout waiting for PADO packets");
        }

        LOGGER(LOG_INFO, "(%u,%hu,0x%x): try number %d, timeout is in %d sec",
                           conn->peer_id, conn->sessionId, conn->host_id,
                           padiAttempts, timeout);

        sendPADI (conn);

        conn->discoveryState = STATE_SENT_PADI;

        waitForPADO (conn, timeout);

        timeout *= 2;
    }
    while (conn->discoveryState == STATE_SENT_PADI);

    // reset
    timeout = conn->discoveryTimeout;

    do
    {
        padrAttempts++;

        if (padrAttempts > MAX_PADR_ATTEMPTS)
        {
            rp_fatal ("Timeout waiting for PADS packets");
        }

        LOGGER(LOG_INFO, "(%u,%hu,0x%x): try number %d, timeout is in %d sec",
                           conn->peer_id, conn->sessionId, conn->host_id,
                           padrAttempts, timeout);

        sendPADR (conn);

        conn->discoveryState = STATE_SENT_PADR;

        waitForPADS (conn, timeout);

        timeout *= 2;
    }
    while (conn->discoveryState == STATE_SENT_PADR);

    /* We're done. */
    conn->discoveryState = STATE_SESSION;

    return;
}


static int waitOnDiscoverySocket(PPPoEConnection * conn, int timeout, PPPoEPacket * packet)
{
    struct timeval tv;
    fd_set readable;

    tv.tv_sec  = timeout;
    tv.tv_usec = 0;

    FD_ZERO (&readable);

    int max = conn->udpIPCSocket;

    FD_SET (conn->udpIPCSocket, &readable);

    LOGGER(LOG_PKT, "(%u,%hu): waiting for %d seconds", 
                        conn->peer_id, conn->sessionId, timeout);

    int r = select (max + 1, &readable, NULL, NULL, &tv);

    if(r > 0)
    {
        if(FD_ISSET (conn->udpIPCSocket, &readable))
        {
            int result = recv_packet_from_parent (conn, packet);

            LOGGER(LOG_PKT, "(%u,%hu): result %d", 
                                conn->peer_id, conn->sessionId, result);

            return result;
        }
        else
        {
            LOGGER(LOG_ERR, "(%u,%hu): socket not raeady, error %s",
                               conn->peer_id, conn->sessionId, strerror(errno));

            return -1;
        }
    }
    else if(r == 0)
    {
        LOGGER(LOG_INFO, "(%u,%hu): timed out", conn->peer_id, conn->sessionId);

        return -2;
    }
    else
    {
        LOGGER(LOG_ERR, "(%u,%hu): error %s", 
                           conn->peer_id, conn->sessionId, strerror(errno));

        return -1;
    }
}

