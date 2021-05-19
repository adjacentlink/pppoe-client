/***********************************************************************
*
* debug.c
*
* Implementation of user-space PPPoE redirector for Linux.
*
* Functions for printing debugging information
*
* Copyright (c) 2013 - Adjacent Link, LLC, Bridgewater NJ
* Copyright (C) 2007-2008 by Cisco Systems, Inc.
* Copyright (C) 2000 by Roaring Penguin Software Inc.
*
* This program may be distributed according to the terms of the GNU
* General Public License, version 2 or (at your option) any later version.
*
* LIC: GPL
*
* This file was modified on Feb 2008 by Cisco Systems, Inc.
***********************************************************************/

// static char const RCSID[] = "$Id: debug.c,v 1.6 2006/01/03 03:05:06 dfs Exp $";

#include "pppoe.h"

#ifdef DEBUGGING_ENABLED

#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>

/**********************************************************************
*%FUNCTION: dumpHex
*%ARGUMENTS:
* fp -- file to dump to
* pkt_buf -- buffer to dump
* len -- length of data
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Dumps buffer to fp in an easy-to-read format
***********************************************************************/
void
dumpHex (FILE * fp, unsigned char const *pkt_buf, int len)
{
    int i;
    int base;

    if (!fp)
    {
        return;
    }

    /* do NOT dump PAP packets */
    if (len >= 2 && pkt_buf[0] == 0xC0 && pkt_buf[1] == 0x23)
    {
        fprintf (fp, "(PAP Authentication Frame -- Contents not dumped)\n");
        return;
    }

    for (base = 0; base < len; base += 16)
    {
        for (i = base; i < base + 16; i++)
        {
            if (i < len)
            {
                fprintf (fp, "%02x ", (unsigned) pkt_buf[i]);
            }
            else
            {
                fprintf (fp, "   ");
            }
        }
        fprintf (fp, "  ");
        for (i = base; i < base + 16; i++)
        {
            if (i < len)
            {
                if (isprint (pkt_buf[i]))
                {
                    fprintf (fp, "%c", pkt_buf[i]);
                }
                else
                {
                    fprintf (fp, ".");
                }
            }
            else
            {
                break;
            }
        }
        fprintf (fp, "\n");
    }
}

/**********************************************************************
*%FUNCTION: printHex
*%ARGUMENTS:
* pkt_buf -- buffer to dump
* len -- length of data
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Prints buffer in an easy-to-read format
***********************************************************************/
void
printHex (unsigned char const *pkt_buf, int pkt_len)
{
    int i;
    int base;

    int len = 0;
    char buff[512] = {0};

    if (pkt_buf == NULL)
    {
        return;
    }

    if (pkt_len > BIGBUF)
    {
        return;
    }

    /* do NOT dump PAP packets */
    if (pkt_len >= 2 && pkt_buf[0] == 0xC0 && pkt_buf[1] == 0x23)
    {
        len += snprintf(buff, sizeof(buff) - len, "(PAP Authentication Frame -- Contents not dumped)\n");
        return;
    }

    for (base = 0; base < pkt_len; base += 16)
    {
        for (i = base; i < base + 16; i++)
        {
            if (i < pkt_len)
            {
                len += snprintf(buff, sizeof(buff) - len, "%02x ", (unsigned) pkt_buf[i]);
            }
            else
            {
                len += snprintf(buff, sizeof(buff) - len, "   ");
            }
        }
        len += snprintf(buff, sizeof(buff) - len, "  ");
        for (i = base; i < base + 16; i++)
        {
            if (i < pkt_len)
            {
                if (isprint (pkt_buf[i]))
                {
                    len += snprintf(buff, sizeof(buff) - len, "%c", pkt_buf[i]);
                }
                else
                {
                    len += snprintf(buff, sizeof(buff) - len, ".");
                }
            }
            else
            {
                break;
            }
        }
        len += snprintf(buff, sizeof(buff) - len, "\n");
    }
}


/**********************************************************************
*%FUNCTION: dumpPacket
*%ARGUMENTS:
* fp -- file to dump to
* packet -- a PPPoE packet
* dir -- either SENT or RCVD
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Dumps the PPPoE packet to fp in an easy-to-read format
***********************************************************************/
void
dumpPacket (FILE * fp, PPPoEPacket * packet, char const *dir)
{
    int len = ntohs (packet->length);

    /* Sheesh... printing times is a pain... */
    struct timeval tv;
    time_t now;
    int millisec;
    struct tm *lt;
    char timebuf[256];

    UINT16_t type = getEtherType (packet);
    if (!fp)
    {
        return;
    }
    gettimeofday (&tv, NULL);
    now = (time_t) tv.tv_sec;
    millisec = tv.tv_usec / 1000;
    lt = localtime (&now);
    strftime (timebuf, 256, "%H:%M:%S", lt);
    fprintf (fp, "%s.%03d %s PPPoE ", timebuf, millisec, dir);

    if (type == Eth_PPPOE_Discovery)
    {
        fprintf (fp, "Discovery (%x) ", (unsigned) type);
    }
    else if (type == Eth_PPPOE_Session)
    {
        fprintf (fp, "Session (%x) ", (unsigned) type);
    }
    else
    {
        fprintf (fp, "Unknown (%x) ", (unsigned) type);
    }

    switch (packet->code)
    {
    case CODE_PADI:
        fprintf (fp, "PADI ");
        break;
    case CODE_PADO:
        fprintf (fp, "PADO ");
        break;
    case CODE_PADR:
        fprintf (fp, "PADR ");
        break;
    case CODE_PADS:
        fprintf (fp, "PADS ");
        break;
    case CODE_PADT:
        fprintf (fp, "PADT ");
        break;
    case CODE_PADQ:
        fprintf (fp, "PADQ ");
        break;
    case CODE_PADG:
        fprintf (fp, "PADG ");
        break;
    case CODE_PADC:
        fprintf (fp, "PADC ");
        break;
    case CODE_PADM:
        fprintf (fp, "PADM ");
        break;
    case CODE_PADN:
        fprintf (fp, "PADN ");
        break;
    case CODE_SESS:
        fprintf (fp, "SESS ");
        break;
    }

    fprintf (fp, "sess-id %d length %d\n", (int) ntohs (packet->session), len);

    /* Ugly... I apologize... */
    fprintf (fp,
             "SourceAddr %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx "
             "DestAddr %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
             packet->eth_hdr.source[0],
             packet->eth_hdr.source[1],
             packet->eth_hdr.source[2],
             packet->eth_hdr.source[3],
             packet->eth_hdr.source[4],
             packet->eth_hdr.source[5],
             packet->eth_hdr.dest[0],
             packet->eth_hdr.dest[1],
             packet->eth_hdr.dest[2],
             packet->eth_hdr.dest[3],
             packet->eth_hdr.dest[4],
             packet->eth_hdr.dest[5]);

#ifdef DUMP_HEX
    dumpHex (fp, packet->payload, ntohs (packet->length));
#endif
}


/**********************************************************************
*%FUNCTION: printPacket
*%ARGUMENTS:
* packet -- a PPPoE packet
* dir -- either SENT or RCVD
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Prints the PPPoE packet in an easy-to-read format
***********************************************************************/
void
printPacket (PPPoEPacket * packet, char const *dir)
{
#if 0
    /* Sheesh... printing times is a pain... */
    struct timeval tv;
    time_t now;
    int millisec;
    struct tm *lt;
    char timebuf[256];

    UINT16_t type = getEtherType (packet);
    gettimeofday (&tv, NULL);
    now = (time_t) tv.tv_sec;
    millisec = tv.tv_usec / 1000;
    lt = localtime (&now);
    strftime (timebuf, 256, "%H:%M:%S", lt);
    printf ("%s.%03d %s PPPoE ", timebuf, millisec, dir);
#endif

    char buff[512] = {0};

    int len = 0;
    if (type == Eth_PPPOE_Discovery)
    {
        len += snprintf(buff, sizeof(buff) - len, "Discovery (%x) ", (unsigned) type);
    }
    else if (type == Eth_PPPOE_Session)
    {
        len += snprintf(buff, sizeof(buff) - len, "Session (%x) ", (unsigned) type);
    }
    else
    {
        len += snprintf(buff, sizeof(buff) - len, "Unknown (%x) ", (unsigned) type);
    }

    switch (packet->code)
    {
    case CODE_PADI:
        len += snprintf(buff, sizeof(buff) - len, "PADI ");
        break;
    case CODE_PADO:
        len += snprintf(buff, sizeof(buff) - len, "PADO ");
        break;
    case CODE_PADR:
        len += snprintf(buff, sizeof(buff) - len, "PADR ");
        break;
    case CODE_PADS:
        len += snprintf(buff, sizeof(buff) - len, "PADS ");
        break;
    case CODE_PADT:
        len += snprintf(buff, sizeof(buff) - len, "PADT ");
        break;
    case CODE_PADM:
        len += snprintf(buff, sizeof(buff) - len, "PADM ");
        break;
    case CODE_PADC:
        len += snprintf(buff, sizeof(buff) - len, "PADC ");
        break;
    case CODE_PADG:
        len += snprintf(buff, sizeof(buff) - len, "PADG ");
        break;
    case CODE_PADQ:
        len += snprintf(buff, sizeof(buff) - len, "PADQ ");
        break;
    case CODE_PADN:
        len += snprintf(buff, sizeof(buff) - len, "PADN ");
        break;
    case CODE_SESS:
        len += snprintf(buff, sizeof(buff) - len, "SESS ");
        break;
    }

    len += snprintf(buff, sizeof(buff) - len, "sess-id %d length %d\n", (int) ntohs (packet->session), len);

    /* Ugly... I apologize... */
    len += snprintf(buff, sizeof(buff) - len, "SourceAddr %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx "
            "DestAddr %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
            packet->eth_hdr.source[0],
            packet->eth_hdr.source[1],
            packet->eth_hdr.source[2],
            packet->eth_hdr.source[3],
            packet->eth_hdr.source[4],
            packet->eth_hdr.source[5],
            packet->eth_hdr.dest[0],
            packet->eth_hdr.dest[1],
            packet->eth_hdr.dest[2],
            packet->eth_hdr.dest[3],
            packet->eth_hdr.dest[4],
            packet->eth_hdr.dest[5]);

    LOGGER(LOG_PKT("%s", buff);

    printHex (packet->payload, ntohs (packet->length));
}




#endif /* DEBUGGING_ENABLED */
