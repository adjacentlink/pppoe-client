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
* buf -- buffer to dump
* len -- length of data
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Dumps buffer to fp in an easy-to-read format
***********************************************************************/
void
dumpHex (FILE * fp, unsigned char const *buf, int len)
{
    int i;
    int base;

    if (!fp)
    {
        return;
    }

    /* do NOT dump PAP packets */
    if (len >= 2 && buf[0] == 0xC0 && buf[1] == 0x23)
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
                fprintf (fp, "%02x ", (unsigned) buf[i]);
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
                if (isprint (buf[i]))
                {
                    fprintf (fp, "%c", buf[i]);
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
* buf -- buffer to dump
* len -- length of data
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Prints buffer in an easy-to-read format
***********************************************************************/
void
printHex (unsigned char const *buf, int len)
{
    int i;
    int base;

    if (buf == NULL)
    {
        return;
    }

    if (len > BIGBUF)
    {
        return;
    }

    /* do NOT dump PAP packets */
    if (len >= 2 && buf[0] == 0xC0 && buf[1] == 0x23)
    {
        printf ("(PAP Authentication Frame -- Contents not dumped)\n");
        return;
    }

    for (base = 0; base < len; base += 16)
    {
        for (i = base; i < base + 16; i++)
        {
            if (i < len)
            {
                printf ("%02x ", (unsigned) buf[i]);
            }
            else
            {
                printf ("   ");
            }
        }
        printf ("  ");
        for (i = base; i < base + 16; i++)
        {
            if (i < len)
            {
                if (isprint (buf[i]))
                {
                    printf ("%c", buf[i]);
                }
                else
                {
                    printf (".");
                }
            }
            else
            {
                break;
            }
        }
        printf ("\n");
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

    dumpHex (fp, packet->payload, ntohs (packet->length));
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
    int len = ntohs (packet->length);

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

    if (type == Eth_PPPOE_Discovery)
    {
        printf ("Discovery (%x) ", (unsigned) type);
    }
    else if (type == Eth_PPPOE_Session)
    {
        printf ("Session (%x) ", (unsigned) type);
    }
    else
    {
        printf ("Unknown (%x) ", (unsigned) type);
    }

    switch (packet->code)
    {
    case CODE_PADI:
        printf ("PADI ");
        break;
    case CODE_PADO:
        printf ("PADO ");
        break;
    case CODE_PADR:
        printf ("PADR ");
        break;
    case CODE_PADS:
        printf ("PADS ");
        break;
    case CODE_PADT:
        printf ("PADT ");
        break;
    case CODE_PADM:
        printf ("PADM ");
        break;
    case CODE_PADC:
        printf ("PADC ");
        break;
    case CODE_PADG:
        printf ("PADG ");
        break;
    case CODE_PADQ:
        printf ("PADQ ");
        break;
    case CODE_PADN:
        printf ("PADN ");
        break;
    case CODE_SESS:
        printf ("SESS ");
        break;
    }

    printf ("sess-id %d length %d\n", (int) ntohs (packet->session), len);

    /* Ugly... I apologize... */
    printf ("SourceAddr %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx "
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

    printHex (packet->payload, ntohs (packet->length));
}




#endif /* DEBUGGING_ENABLED */
