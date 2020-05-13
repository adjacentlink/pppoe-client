/***********************************************************************
*
* common.c
*
* Implementation of user-space PPPoE redirector for Linux.
*
* Common functions used by PPPoE client and server
*
* Copyright (c) 2013 - Adjacent Link, LLC, Bridgewater NJ
* Copyright (C) 2007-2008 Cisco Systems, Inc.
* Copyright (C) 2000 by Roaring Penguin Software Inc.
*
* This program may be distributed according to the terms of the GNU
* General Public License, version 2 or (at your option) any later version.
*
* LIC: GPL
*
* This file was modified on Feb 2008 by Cisco Systems, Inc.
***********************************************************************/

// static char const RCSID[] = "$Id: common.c,v 1.21 2006/01/03 03:20:38 dfs Exp $";
/* For vsnprintf prototype */
// #define _ISOC99_SOURCE 1

/* For seteuid prototype */
// #define _BSD_SOURCE 1

#include "pppoe.h"

#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#include "pppoe_rfc4938_nbr.h"
#include "pppoe_rfc4938_debug.h"

/* Are we running SUID or SGID? */
int IsSetID = 0;

static int saved_uid = -2;
static int saved_gid = -2;

/**********************************************************************
*%FUNCTION: parseDiscoveryPacket
*%ARGUMENTS:
* packet -- the PPPoE discovery packet to parse
* func -- function called for each tag in the packet
* extra -- an opaque data pointer supplied to parsing function
*%RETURNS:
* 0 if everything went well; -1 if there was an error
*%DESCRIPTION:
* Parses a PPPoE discovery packet, calling "func" for each tag in the packet.
* "func" is passed the additional argument "extra".
***********************************************************************/
int
parseDiscoveryPacket (PPPoEPacket * packet, ParseFunc * func, void *extra)
{
    UINT16_t len = ntohs (packet->pppoe_length);
    unsigned char *curTag;
    UINT16_t tagType, tagLen;

    if (packet->pppoe_ver != 1)
    {
        LOGGER(LOG_ERR, "Invalid PPPoE version (%d)\n", (int) packet->pppoe_ver);
        return -1;
    }
    if (packet->pppoe_type != 1)
    {
        LOGGER(LOG_ERR, " Invalid PPPoE type (%d)\n", (int) packet->pppoe_type);
        return -1;
    }


    /* Step through the tags */
    curTag = packet->payload;
    while (curTag - packet->payload < len)
    {
        /* Alignment is not guaranteed, so do this by hand... */
        tagType = get_word_from_buff(curTag, 0);
        tagLen  = get_word_from_buff(curTag, 2);

        if (tagType == TAG_END_OF_LIST)
        {
            return 0;
        }
        if ((curTag - packet->payload) + tagLen + TAG_HDR_SIZE > len)
        {
            LOGGER(LOG_ERR, "Invalid PPPoE tag length (%u)\n", tagLen);
            return -1;
        }

        func (tagType, tagLen, curTag + TAG_HDR_SIZE, extra);

        curTag = curTag + TAG_HDR_SIZE + tagLen;
    }
    return 0;
}

/**********************************************************************
*%FUNCTION: findTag
*%ARGUMENTS:
* packet -- the PPPoE discovery packet to parse
* type -- the type of the tag to look for
* tag -- will be filled in with tag contents
*%RETURNS:
* A pointer to the tag if one of the specified type is found; NULL
* otherwise.
*%DESCRIPTION:
* Looks for a specific tag type.
***********************************************************************/
unsigned char *
findTag (PPPoEPacket * packet, UINT16_t type, PPPoETag * tag)
{
    UINT16_t len = ntohs (packet->pppoe_length);
    unsigned char *curTag;
    UINT16_t tagType, tagLen;

    if (packet->pppoe_ver != 1)
    {
        LOGGER(LOG_ERR, "Invalid PPPoE version (%d)", (int) packet->pppoe_ver);
        return NULL;
    }
    if (packet->pppoe_type != 1)
    {
        LOGGER(LOG_ERR, "Invalid PPPoE type (%d)", (int) packet->pppoe_type);
        return NULL;
    }

    /* Step through the tags */
    curTag = packet->payload;
    while (curTag - packet->payload < len)
    {
        /* Alignment is not guaranteed, so do this by hand... */
        tagType = get_word_from_buff(curTag, 0);
        tagLen  = get_word_from_buff(curTag, 2);

        if (tagType == TAG_END_OF_LIST)
        {
            return NULL;
        }
        if ((curTag - packet->payload) + tagLen + TAG_HDR_SIZE > len)
        {
            LOGGER(LOG_ERR, "Invalid PPPoE tag length (%u)", tagLen);
            return NULL;
        }
        if (tagType == type)
        {
            memcpy (tag, curTag, tagLen + TAG_HDR_SIZE);
            return curTag;
        }
        curTag = curTag + TAG_HDR_SIZE + tagLen;
    }
    return NULL;
}

/**********************************************************************
*%FUNCTION: switchToRealID
*%ARGUMENTS:
* None
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Sets effective user-ID and group-ID to real ones.  Aborts on failure
***********************************************************************/
void
switchToRealID (void)
{
    if (IsSetID)
    {
        if (saved_uid < 0)
        {
            saved_uid = geteuid ();
        }

        if (saved_gid < 0)
        {
            saved_gid = getegid ();
        }

        if (setegid (getgid ()) < 0)
        {
            printErr ("setgid failed");
            exit (EXIT_FAILURE);
        }

        if (seteuid (getuid ()) < 0)
        {
            printErr ("seteuid failed");
            exit (EXIT_FAILURE);
        }
    }
}

/**********************************************************************
*%FUNCTION: switchToEffectiveID
*%ARGUMENTS:
* None
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Sets effective user-ID and group-ID back to saved gid/uid
***********************************************************************/
void
switchToEffectiveID (void)
{
    if (IsSetID)
    {
        if (setegid (saved_gid) < 0)
        {
            printErr ("setgid failed");
            exit (EXIT_FAILURE);
        }
        if (seteuid (saved_uid) < 0)
        {
            printErr ("seteuid failed");
            exit (EXIT_FAILURE);
        }
    }
}

/**********************************************************************
*%FUNCTION: dropPrivs
*%ARGUMENTS:
* None
*%RETURNS:
* Nothing
*%DESCRIPTION:
* If effective ID is root, try to become "nobody".  If that fails and
* we're SUID, switch to real user-ID
***********************************************************************/
void
dropPrivs (void)
{
    struct passwd *pw = NULL;

    int ok = 0;

    if (geteuid () == 0)
    {
        pw = getpwnam ("nobody");

        if (pw)
        {
            if (setgid (pw->pw_gid) < 0)
            {
                ok++;
            }

            if (setuid (pw->pw_uid) < 0)
            {
                ok++;
            }
        }
    }

    if (ok < 2 && IsSetID)
    {
        setegid (getgid ());
        seteuid (getuid ());
    }
}

/**********************************************************************
*%FUNCTION: printErr
*%ARGUMENTS:
* str -- error message
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Prints a message to stderr and syslog.
***********************************************************************/
void
printErr (char const *str)
{
    LOGGER(LOG_ERR, "%s\n", str);
}


/**********************************************************************
*%FUNCTION: strDup
*%ARGUMENTS:
* str -- string to copy
*%RETURNS:
* A malloc'd copy of str.  Exits if malloc fails.
***********************************************************************/
char *
strDup (char const *str)
{
    char *copy = malloc (strlen (str) + 1);
    if (!copy)
    {
        rp_fatal ("strdup failed");
    }
    strcpy (copy, str);
    return copy;
}


/***********************************************************************
*%FUNCTION: sendPADT
*%ARGUMENTS:
* conn -- PPPoE connection
* msg -- if non-NULL, extra error message to include in PADT packet.
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Sends a PADT packet
***********************************************************************/
void
sendPADTandExit (PPPoEConnection * conn, char const *msg, int tellParent)
{
    PPPoEPacket packet;
    unsigned char *cursor = packet.payload;

    UINT16_t plen = 0;

    /* Do nothing if no session established yet */
    if (!conn->sessionId)
    {
        LOGGER(LOG_INFO, "(%u): no session id, not sending PADT\n", conn->peer_id);

        goto do_exit;
    }

    memset(&packet, 0x0, sizeof(packet));

    memcpy (packet.eth_hdr.dest, conn->peerEth, PPPOE_ETH_ALEN);
    memcpy (packet.eth_hdr.source, conn->myEth, PPPOE_ETH_ALEN);

    packet.eth_hdr.proto = htons (Eth_PPPOE_Discovery);
    packet.pppoe_ver = 1;
    packet.pppoe_type = 1;
    packet.pppoe_code = CODE_PADT;
    packet.pppoe_session = htons (conn->sessionId);

    /* If we're using Host-Uniq, copy it over */
    if (conn->useHostUniq)
    {
        PPPoETag hostUniq;

        UINT32_t id = htonl (conn->host_id);

        hostUniq.type   = htons (TAG_HOST_UNIQ);
        hostUniq.length = htons (sizeof (id));

        memcpy (hostUniq.payload, &id, sizeof (id));
        memcpy (cursor, &hostUniq, sizeof (id) + TAG_HDR_SIZE);

        cursor += sizeof (id) + TAG_HDR_SIZE;
        plen   += sizeof (id) + TAG_HDR_SIZE;
    }

    /* Copy error message */
    if (msg)
    {
        PPPoETag err;

        UINT16_t elen = strlen (msg);
        err.type   = htons (TAG_GENERIC_ERROR);
        err.length = htons (elen);

        strcpy ((char *) err.payload, msg);
        memcpy (cursor, &err, elen + TAG_HDR_SIZE);

        cursor += elen + TAG_HDR_SIZE;
        plen   += elen + TAG_HDR_SIZE;
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

    packet.pppoe_length = htons (plen);

    send_discovery_packet_to_conn (conn, &packet);

    LOGGER(LOG_PKT, "(%u,%hu,%d): sent PADT (%s)\n",
           conn->peer_id, conn->sessionId, conn->host_id, msg);

    if(tellParent)
    {
        LOGGER(LOG_INFO, "(%u): inform parent \n", conn->peer_id);
        /* Inform rfc4938 process that session is closing */
        send_child_session_terminated (conn);
    }

do_exit:
    LOGGER(LOG_INFO, "(%u,%hu): Host Id %u (0x%x) will terminate in 1 sec\n",
                       conn->peer_id, conn->sessionId, conn->host_id, conn->host_id);

    sleep(1);

    if(conn->udpIPCSocket)
    {
        close (conn->udpIPCSocket);
    }

    exit (EXIT_SUCCESS);
}

/***********************************************************************
*%FUNCTION: sendPADTf
*%ARGUMENTS:
* conn -- PPPoE connection
* msg -- printf-style format string
* args -- arguments for msg
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Sends a PADT packet with a formatted message
***********************************************************************/
void
sendPADTf (PPPoEConnection * conn, char const *fmt, ...)
{
    char msg[512] = {0};
    va_list ap;

    va_start (ap, fmt);
    vsnprintf (msg, sizeof (msg), fmt, ap);
    va_end (ap);
    msg[511] = 0;

    sendPADTandExit (conn, msg, 1);
}

/**********************************************************************
*%FUNCTION: pktLogErrs
*%ARGUMENTS:
* pkt -- packet type (a string)
* type -- tag type
* len -- tag length
* data -- tag data
* extra -- extra user data
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Logs error tags
***********************************************************************/
void
pktLogErrs (char const *pkt, UINT16_t type, UINT16_t len, unsigned char *data, void *extra __attribute__((unused)))
{
    char const *str;
    char const *fmt = "rfc4938pppoe(): %s: %s: %.*s";

    switch (type)
    {
    case TAG_SERVICE_NAME_ERROR:
        str = "Service-Name-Error";
        break;

    case TAG_AC_SYSTEM_ERROR:
        str = "System-Error";
        break;

    default:
        str = "Generic-Error";
    }

    LOGGER(LOG_ERR, fmt, pkt, str, (int) len, data);
}

/**********************************************************************
*%FUNCTION: parseLogErrs
*%ARGUMENTS:
* type -- tag type
* len -- tag length
* data -- tag data
* extra -- extra user data
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Picks error tags out of a packet and logs them.
***********************************************************************/
void
parseLogErrs (UINT16_t type, UINT16_t len, unsigned char *data, void *extra)
{
    pktLogErrs ("PADT", type, len, data, extra);
}



void sync_credit_grant(PPPoEConnection *conn, UINT16_t fcn, UINT16_t bcn)
{
    UINT16_t old_local = conn->local_credits;
    UINT16_t old_peer  = conn->peer_credits;

    /* check to make sure you don't exceed max credits */
    if ((fcn + conn->local_credits) > MAX_CREDITS)
    {
        conn->local_credits = MAX_CREDITS;
    }
    else
    {
        conn->local_credits = fcn;
    }

    // lets trust the router
    conn->peer_credits = bcn;

    LOGGER(LOG_PKT, "(%u,%hu): fcn:%hu, local old %hu, new %hu, bcn:%hu, peer old %hu, new %hu\n",
                        conn->peer_id, conn->sessionId,
                        fcn, old_local, conn->local_credits,
                        bcn, old_peer,  conn->peer_credits);
}



void handle_credit_grant(PPPoEConnection *conn, UINT16_t fcn, UINT16_t bcn)
{
    UINT16_t old_local = conn->local_credits;
    UINT16_t old_peer  = conn->peer_credits;

    /* check to make sure you don't exceed max credits */
    if ((fcn + conn->local_credits) > MAX_CREDITS)
    {
        conn->local_credits = MAX_CREDITS;
    }
    else
    {
        conn->local_credits += fcn;
    }

    // lets trust the router
    conn->peer_credits = bcn;

    LOGGER(LOG_PKT, "(%u,%hu): fcn:%hu, local old %hu, new %hu, bcn:%hu, peer old %hu, new %hu\n",
                        conn->peer_id, conn->sessionId,
                        fcn, old_local, conn->local_credits,
                        bcn, old_peer, conn->peer_credits);
}


void handle_inband_grant(PPPoEConnection *conn, UINT16_t fcn, UINT16_t bcn)
{
    UINT16_t old_local = conn->local_credits;

    /* check to make sure you don't exceed max credits */
    if ((fcn + conn->local_credits) > MAX_CREDITS)
    {
        conn->local_credits = MAX_CREDITS;
    }
    else
    {
        conn->local_credits += fcn;
    }

    LOGGER(LOG_PKT, "(%u,%hu): fcn:%hu + local_credits %hu = %hu, bcn:%hu peer_credits %hu\n",
                        conn->peer_id, conn->sessionId,
                        fcn, old_local, conn->local_credits, bcn, conn->peer_credits);
}
