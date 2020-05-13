/***********************************************************************
*
* pppoe.c
*
* Implementation of user-space PPPoE redirector for Linux.
*
* Copyright (c) 2013 - Adjacent Link, LLC, Bridgewater NJ
* Copyright (C) 2007-2008 by Cisco Systems, Inc.
* Copyright (C) 2000-2006 by Roaring Penguin Software Inc.
*
* This program may be distributed according to the terms of the GNU
* General Public License, version 2 or (at your option) any later version.
*
* LIC: GPL
*
* This file was modified on Feb 2008 by Cisco Systems, Inc.
***********************************************************************/

//static char const RCSID[] = "$Id: pppoe.c,v 1.43 2006/02/23 15:40:42 dfs Exp $";

#include "pppoe.h"
#include "pppoe_rfc4938.h"
#include "pppoe_rfc4938_nbr.h"

#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/ioctl.h>

FILE * LoggerFp = NULL;

/* Default interface if no -I option given */
#define DEFAULT_IF "eth0"

/* Global variables -- options */
int optInactivityTimeout = 0;   /* Inactivity timeout */

PPPoEConnection *Connection = NULL;     /* Must be global -- used
                                           in signal handler */


static int handle_discovery_frame_from_conn (PPPoEConnection * conn, PPPoEPacket * packet, int len);

static void initSessionPacket(PPPoEConnection * conn, PPPoEPacket * packet);

int
consume_credits_and_send_frame_to_conn (PPPoEConnection * conn, PPPoEPacket * packet)
{
    UINT16_t consumed_credits = 0;

    UINT16_t required_credits = compute_local_credits (conn, packet);

    if (conn->local_credits < required_credits)
    {
        /*
         * Not enough credits to send packet.  A more complete implementation
         * may decide to queue this packet instead of dropping it.  We are going
         * to drop it. Now we just send it.
         */

        LOGGER(LOG_PKT, "(%u,%hu): req credits %hu, not enough local_credits %hu, send anyway\n",
                            conn->peer_id, conn->sessionId, required_credits, conn->local_credits);

        required_credits = conn->local_credits;
    }

    if (conn->send_inband_grant)
    {
        /* send an inband grant inside this packet */
        consumed_credits = sendInBandGrant (conn, packet, conn->grant_limit);
    }
    else
    {
        /* decrement local credits to send packet to peer */
        consumed_credits = required_credits;
    }

    if(conn->local_credits >= consumed_credits)
    {
        conn->local_credits -= consumed_credits;
    }
    else
    {
        conn->local_credits = 0;
    }

    LOGGER(LOG_PKT, "(%u,%hu): required_credits %hu, consumed_credits %hu, local_credits %hu\n",
                        conn->peer_id, conn->sessionId, required_credits, consumed_credits,
                        conn->local_credits);

    return send_session_packet_to_conn (conn, packet);
}


static int
handle_discovery_frame_from_conn (PPPoEConnection * conn, PPPoEPacket * packet, int len)
{
    /* Check length */
    if ((int) (ntohs (packet->pppoe_length) + ETH_PPPOE_OVERHEAD) != len)
    {
        LOGGER(LOG_ERR, "(%u,%hu): bogus PPPoE length field (%hu), drop packet\n",
                           conn->peer_id, conn->sessionId, ntohs (packet->pppoe_length));
        return -1;
    }

    /* check for our session */
    if (ntohs (packet->pppoe_session) != conn->sessionId)
    {
        LOGGER(LOG_INFO, "(%u,%hu): pkt session mismatch %hu != %hu \n",
                           conn->peer_id, conn->sessionId, ntohs (packet->pppoe_session), conn->sessionId);

        return 0;
    }

    LOGGER(LOG_PKT, "(%u,%hu): len %d\n", conn->peer_id, conn->sessionId, len);

    switch (packet->pppoe_code)
    {
    case (CODE_PADT):
        if (memcmp (packet->eth_hdr.dest, conn->myEth, PPPOE_ETH_ALEN))
        {
            LOGGER(LOG_PKT, "(%u,%hu): dst eth not for me, drop packet\n",
                                conn->peer_id, conn->sessionId);

            return 0;
        }

        if (memcmp (packet->eth_hdr.source, conn->peerEth, PPPOE_ETH_ALEN))
        {
            LOGGER(LOG_PKT, "(%u,%hu): src eth not from peer, drop packet\n",
                                conn->peer_id, conn->sessionId);

            return 0;
        }

        LOGGER(LOG_INFO, "(%u,%hu): Session %hu terminated -- received PADT from peer\n",
                           conn->peer_id, conn->sessionId, ntohs (packet->pppoe_session));

        parseDiscoveryPacket (packet, parseLogErrs, NULL);

        sendPADTandExit (conn, "PPPoEClient: Received PADT from peer", 1);

        return 0;

    case (CODE_PADG):
        recvPADG (conn, packet);

        return 1;

    case (CODE_PADC):
        recvPADC (conn, packet);

        return 1;

    case (CODE_PADQ):
        recvPADQ (conn, packet);

        return 1;

    default:
        return 0;
    }
}


static void initSessionPacket(PPPoEConnection * conn, PPPoEPacket * packet)
{
    if(conn == NULL)
    {
        LOGGER(LOG_ERR, "(%u,%hu): NULL connection\n",
                           conn->peer_id, conn->sessionId);

        return;
    }

    if(packet == NULL)
    {
        LOGGER(LOG_ERR, "(%u,%hu): NULL packet\n",
                           conn->peer_id, conn->sessionId);


        return;
    }

    memset(packet, 0x0, sizeof(*packet));

    memcpy (packet->eth_hdr.dest,   conn->peerEth, PPPOE_ETH_ALEN);
    memcpy (packet->eth_hdr.source, conn->myEth,   PPPOE_ETH_ALEN);

    packet->eth_hdr.proto = htons (Eth_PPPOE_Session);

    packet->pppoe_ver  = 1;
    packet->pppoe_type = 1;
    packet->pppoe_code = CODE_SESS;
    packet->pppoe_session = htons (conn->sessionId);
}


/**********************************************************************
*%FUNCTION: session
*%ARGUMENTS:
* conn -- PPPoE connection info
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Handles the "session" phase of PPPoE
***********************************************************************/
void
doSession (PPPoEConnection * conn)
{
    fd_set allReadable;
    PPPoEPacket packet;
    struct timeval tv;
    struct timeval *tvp = NULL;
    int num;

    initSessionPacket(conn, &packet);

    /* Drop privileges */
    dropPrivs ();

    int max_fd = -1;

    FD_ZERO (&allReadable);

    if (conn->signalPipe[PIPE_RD_FD] >= 0)
    {
        FD_SET (conn->signalPipe[PIPE_RD_FD], &allReadable);

        if (conn->signalPipe[PIPE_RD_FD] > max_fd)
        {
            max_fd = conn->signalPipe[PIPE_RD_FD];
        }

        LOGGER(LOG_INFO, "(%u,%hu): added signal pipe fd %d\n",
                           conn->peer_id, conn->sessionId,
                           conn->signalPipe[PIPE_RD_FD]);
    }


    FD_SET (conn->udpIPCSocket, &allReadable);

    if (conn->udpIPCSocket > max_fd)
    {
        max_fd = conn->udpIPCSocket;
    }

    LOGGER(LOG_INFO, "(%u,%hu): added ipc socket fd %d\n",
                       conn->peer_id, conn->sessionId,
                       conn->udpIPCSocket);

    while (1)
    {
        fd_set readable = allReadable;

        if (optInactivityTimeout > 0)
        {
            tv.tv_sec  = optInactivityTimeout;
            tv.tv_usec = 0;
            tvp = &tv;
        }

        LOGGER(LOG_PKT, "(%u,%hu): waiting for session data\n",
                           conn->peer_id, conn->sessionId);

        while (1)
        {
            num = select (max_fd + 1, &readable, NULL, NULL, tvp);

            if (num >= 0 || errno != EINTR)
            {
                break;
            }
        }

        if (num < 0)
        {
            fatalSys ("select (session)", strerror(errno));
        }

        if (num == 0)
        {
            /* Inactivity timeout */
            LOGGER(LOG_ERR, "(%u,%hu): Inactivity timeout on session %hu\n",
                               conn->peer_id, conn->sessionId, conn->sessionId);

            sendPADTandExit (conn, "PPPoEClient: Inactivity timeout", 1);
        }


        if (FD_ISSET (conn->udpIPCSocket, &readable))
        {
            LOGGER(LOG_PKT, "(%u,%hu): packet ready on IPC sock\n",
                                conn->peer_id, conn->sessionId);

            int result = recv_packet_from_parent (conn, &packet);

            if (result > 0)
            {
                LOGGER(LOG_PKT, "(%u,%hu): parse result %d, frame has discovery info \n",
                                    conn->peer_id, conn->sessionId, result);

                handle_discovery_frame_from_conn (conn, &packet, result);

                initSessionPacket(conn, &packet);
            }
        }

        if (conn->signalPipe[PIPE_RD_FD] >= 0)
        {
            if (FD_ISSET (conn->signalPipe[PIPE_RD_FD], &readable))
            {
                LOGGER(LOG_PKT, "(%u,%hu): msg ready on signal pipe\n",
                                    conn->peer_id, conn->sessionId);

                handle_signal_event (conn);
            }
        }
    }
}



/**********************************************************************
*%FUNCTION: usage
*%ARGUMENTS:
* argv0 -- program name
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Prints usage information and exits.
***********************************************************************/
void
usage (char const *argv0)
{
    fprintf (stderr, "Usage: %s [options]\n", argv0);
    fprintf (stderr, "Options:\n");
    fprintf (stderr, "   -I if_name     -- Specify interface (default %s.)\n", DEFAULT_IF);
    fprintf (stderr,
             "   -T timeout     -- Specify inactivity timeout in seconds.\n"
             "   -t timeout     -- Initial timeout for discovery packets in seconds\n"
             "   -V             -- Print version and exit.\n"
             "   -S name        -- Set desired service name.\n"
             "   -C name        -- Set desired access concentrator name.\n"
             "   -U             -- Use Host-Unique to allow multiple PPPoE sessions.\n"
             "   -p pidfile     -- Write process-ID to pidfile.\n"
             "   -f disc:sess   -- Set Ethernet frame types (hex).\n"
             "   -x             -- credit scaling factor.\n"
             "                      NOTE: a scaling factor of 0 specifies rfc4938 compliance only\n"
             "   -y             -- remote node id\n"
             "   -Y             -- local node id\n"
             "   -r             -- starting port to listen to\n"
             "   -R             -- neighbor pid\n"
             "   -c             -- parent process port\n"
             "   -z             -- RFC4938 debug level\n"
             "                      0: off 1(default): errors 2: events 3: packets\n"
             "   -g             -- Initial credit grant amount.\n"
             "   -G             -- use timed credits.\n"
             "   -E             -- our ethernet address.\n"
             "   -h             -- Print usage information.\n\n"
             "   -D logfile     -- logfile.\n\n"
             "PPPoE Version %s, Copyright (C) 2001-2006 Roaring Penguin Software Inc.\n"
             "\t                   Copyright (C) 2007-2008 by Cisco Systems, Inc.\n"
             "\t\n"
             "PPPoE comes with ABSOLUTELY NO WARRANTY.\n"
             "This is free software, and you are welcome to redistribute it under the terms\n"
             "of the GNU General Public License, version 2 or any later version.\n"
             "http://www.roaringpenguin.com\n"
             "This program was modified by Cisco Systems, Inc.\n"
             " to implement RFC4938 and draft-bberry-pppoe-scaled-credits-metrics-01", VERSION);

    exit (EXIT_SUCCESS);
}

/**********************************************************************
*%FUNCTION: main
*%ARGUMENTS:
* argc, argv -- count and values of command-line arguments
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Main program
***********************************************************************/
int
main (int argc, char *argv[])
{
    int opt;
    int n;
    unsigned char  m[6];          /* MAC address in -E option */
    FILE *pidfile;
    unsigned int discoveryType, sessionType;
    UINT16_t credit_scalar = 0;
    UINT16_t my_port = 0;
    UINT16_t parent_port = 0;
    UINT32_t peer_pid = 0;
    UINT16_t grant_amount = 0;
    UINT16_t timed_credits = 0;
    int rfc4938_debug = 0;

    PPPoEConnection conn;

    if (getuid () != geteuid () || getgid () != getegid ())
    {
        IsSetID = 1;
    }

    /* Initialize connection info */
    memset (&conn, 0, sizeof (conn));

    conn.signalPipe[PIPE_RD_FD] = -1;
    conn.signalPipe[PIPE_WR_FD] = -1;

    conn.discoveryTimeout = PADI_TIMEOUT;

    conn.p2p_mode = 1;
    conn.lcp_mode = 1;

    /* For signal handler */
    Connection = &conn;

    char const * options = "I:VT:hS:C:Up:f:t:y:Y:x:z:r:R:c:g:G:E:BLD:";

    while ((opt = getopt (argc, argv, options)) != -1)
    {
        switch (opt)
        {
        case 't':
            if (sscanf (optarg, "%d", &conn.discoveryTimeout) != 1)
            {
                fprintf (stderr, "Illegal argument to -t: Should be -t timeout\n");
                exit (EXIT_FAILURE);
            }
            if (conn.discoveryTimeout < 1)
            {
                conn.discoveryTimeout = 1;
            }
            break;

        case 'f':
            if (sscanf (optarg, "%x:%x", &discoveryType, &sessionType) != 2)
            {
                fprintf (stderr, "Illegal argument to -f: Should be disc:sess in hex\n");
                exit (EXIT_FAILURE);
            }
            Eth_PPPOE_Discovery = (UINT16_t) discoveryType;
            Eth_PPPOE_Session   = (UINT16_t) sessionType;
            break;

        case 'p':
            switchToRealID ();
            pidfile = fopen (optarg, "w");
            if (pidfile)
            {
                fprintf (pidfile, "%lu\n", (unsigned long) getpid ());
                fclose (pidfile);
            }
            switchToEffectiveID ();
            break;

        case 'S':
            SET_STRING (conn.serviceName, optarg);
            break;

        case 'C':
            SET_STRING (conn.acName, optarg);
            break;

        case 'E':
            n = sscanf (optarg, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
                        &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]);
            if (n != 6)
            {
                fprintf (stderr, "Illegal argument to -E: Should be xx:yy:zz:aa:bb:cc\n");
                exit (EXIT_FAILURE);
            }

            /* Copy MAC address of peer */
            for (n = 0; n < 6; n++)
            {
                conn.myEth[n] =  m[n];
            }

            break;

        case 'U':
            conn.useHostUniq = 1;
            break;

        case 'T':
            optInactivityTimeout = (int) strtol (optarg, NULL, 10);
            if (optInactivityTimeout < 0)
            {
                optInactivityTimeout = 0;
            }
            break;

        case 'I':
            SET_STRING (conn.ifName, optarg);
            break;

        case 'V':
            printf ("Roaring Penguin PPPoE RFC4938 + EMANE Version %s\n", VERSION);
            exit (EXIT_SUCCESS);

        case 'y':
            conn.peer_id = strtol (optarg, NULL, 10);
            break;

        case 'Y':
            conn.parent_id = strtol (optarg, NULL, 10);
            break;

        case 'g':
            grant_amount = strtoul (optarg, NULL, 10);
            break;

        case 'G':
            timed_credits = strtoul (optarg, NULL, 10);
            break;

        case 'x':
            credit_scalar = (UINT16_t) strtol (optarg, NULL, 10);
            break;

        case 'z':
            rfc4938_debug = (UINT16_t) strtol (optarg, NULL, 10);
            break;

        case 'r':
            my_port = (UINT16_t) strtol (optarg, NULL, 10);
            break;

        case 'R':
            peer_pid = (UINT32_t) strtol (optarg, NULL, 10);
            break;

        case 'c':
            parent_port = (UINT16_t) strtol (optarg, NULL, 10);
            break;

        case 'B':
            conn.p2p_mode = 0;
            break;

        case 'L':
            conn.enable_lcp_echo_reply = 1;
            break;

        case 'h':
            usage (argv[0]);
            break;

        case 'D':
            if((LoggerFp = fopen(optarg, "w")) == NULL)
            {
                fprintf(stderr, "could not open child log file '%s', %s\n", optarg, strerror(errno));
            }
            else
            {
              LOGGER(LOG_INFO, "XXXXXXXXX BEGIN XXXXXXXX");
            }
        break;

        default:
            usage (argv[0]);
        }
    }

#ifdef HAVE_SYSLOG_H
    openlog("rfc4938pppoe", LOG_PID, LOG_DAEMON);
#endif

    /* set our host unique id */
    conn.host_id = getpid();

    /* peer device that implements rfc4938 client */
    if (!(conn.peer_id))
    {
        fprintf (stderr, "peer ip must be specified with -y\n");
        exit (EXIT_FAILURE);
    }

    /* local device that implements the rfc4938 client */
    if (!(conn.parent_id))
    {
        fprintf (stderr, "peer ip must be specified with -Y\n");
        exit (EXIT_FAILURE);
    }

    /* Pick a default interface name */
    if (!conn.ifName)
    {
        SET_STRING (conn.ifName, DEFAULT_IF);
    }


    /* Initialize rfc4938 values */
    pppoe_init_flow_control (&conn,
                             credit_scalar,
                             rfc4938_debug,
                             my_port,
                             parent_port,
                             peer_pid,
                             grant_amount,
                             timed_credits);

    LOGGER(LOG_INFO, "(%u,%hu): begin discovery phase ",
                       Connection->peer_id, Connection->sessionId);

    doDiscovery (&conn);

    LOGGER(LOG_INFO, "(%u,%hu): discovery  phase completed",
                       Connection->peer_id, Connection->sessionId);

    /* Set signal handlers */
    signal (SIGTERM, SIG_IGN);
    signal (SIGINT, pppoe_signal_handler);
    signal (SIGHUP, pppoe_signal_handler);

    LOGGER(LOG_INFO, "(%u,%hu): begin session phase ",
                       Connection->peer_id, Connection->sessionId);

    doSession (&conn);

    LOGGER(LOG_INFO, "(%u,%hu): session phase completed",
                       Connection->peer_id, Connection->sessionId);

    return 0;
}

/**********************************************************************
*%FUNCTION: fatalSys
*%ARGUMENTS:
* str -- error message
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Prints a message plus the errno value to stderr and syslog and exits.
***********************************************************************/
void
fatalSys (char const *str, char const * err)
{
    char buf[1024];

    sprintf (buf, "%.256s: Session %hu: %.256s", str, Connection->sessionId, err);
    printErr (buf);

    /* alert parent rfc4938 process we are terminating */

    sendPADTf (Connection, "PPPoEClient: System call error: %s", strerror (errno));

    exit (EXIT_FAILURE);
}

/**********************************************************************
*%FUNCTION: sysErr
*%ARGUMENTS:
* str -- error message
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Prints a message plus the errno value to syslog.
***********************************************************************/
void
sysErr (char const *str)
{
    char buf[1024];
    sprintf (buf, "%.256s: %.256s", str, strerror (errno));
    printErr (buf);
}

/**********************************************************************
*%FUNCTION: rp_fatal
*%ARGUMENTS:
* str -- error message
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Prints a message to stderr and syslog and exits.
***********************************************************************/
void
rp_fatal (char const *str)
{
    LOGGER(LOG_ERR, "(%u,%hu): rp_fatal", Connection->peer_id, Connection->sessionId);

    printErr (str);

    sendPADTf (Connection, "PPPoEClient: Session %hu: %.256s", Connection->sessionId, str);

    exit (EXIT_FAILURE);
}

int
handle_session_frame_from_conn (PPPoEConnection * conn, PPPoEPacket * packet, int len)
{
    /* Check length */
    if ((int) (ntohs (packet->pppoe_length) + ETH_PPPOE_OVERHEAD) != len)
    {
        LOGGER(LOG_ERR, "(%u,%hu): bogus PPPoE length field (%hu), drop packet\n",
                           conn->peer_id, conn->sessionId, ntohs (packet->pppoe_length));
        return -1;
    }

    /* Sanity check */
    if (packet->pppoe_code != CODE_SESS)
    {
        LOGGER(LOG_ERR, "(%u,%hu): unexpected packet code %hhu, drop packet\n",
                           conn->peer_id, conn->sessionId, packet->pppoe_code);
        return -1;
    }

    if (packet->pppoe_ver != 1)
    {
        LOGGER(LOG_ERR, "(%u,%hu): unexpected packet version %hhu, drop packet\n",
                           conn->peer_id, conn->sessionId, packet->pppoe_ver);
        return -1;
    }

    if (packet->pppoe_type != 1)
    {
        LOGGER(LOG_ERR, "(%u,%hu): unexpected packet type %hhu, drop packet\n",
                           conn->peer_id, conn->sessionId, packet->pppoe_type);
        return -1;
    }

    if (memcmp (packet->eth_hdr.dest, conn->myEth, PPPOE_ETH_ALEN))
    {
        LOGGER(LOG_INFO, "(%u,%hu): dst eth not for me, drop packet\n",
                           conn->peer_id, conn->sessionId);
        return 0;
    }

    if (memcmp (packet->eth_hdr.source, conn->peerEth, PPPOE_ETH_ALEN))
    {
        LOGGER(LOG_INFO, "(%u,%hu): src eth not from peer, drop packet\n",
                           conn->peer_id, conn->sessionId);

        return 0;
    }

    if (ntohs (packet->pppoe_session) != conn->sessionId)
    {
        LOGGER(LOG_INFO, "(%u,%hu): session %hu not for me, drop packet\n",
                           conn->peer_id, conn->sessionId, ntohs (packet->pppoe_session));

        return 0;
    }

    if (ntohs (packet->pppoe_session) != conn->sessionId)
    {
        LOGGER(LOG_ERR, "(%u,%hu): Session mismatch %hu != %hu, drop packet\n",
                           conn->peer_id, conn->sessionId,
                           ntohs (packet->pppoe_session), conn->sessionId);
        return 0;
    }

    /* check for an inband grant from the peer */
    UINT16_t consumed_credits = 0;

    UINT16_t old_peer_credits = conn->peer_credits;

    /* grab the first two byts of the payload looking for TAG_RFC4938_CREDITS */
    UINT16_t tagType = get_word_from_buff(packet->payload, 0);

    if (TAG_RFC4938_CREDITS == tagType)
    {
        UINT16_t required_credits = compute_peer_credits_with_inband (conn, packet);

        /* receive the grant, peer_credits are handled here */
        int bcn = recvInBandGrant (conn, packet);

        if(bcn < 0)
        {
            /* drop the packet */
            LOGGER(LOG_ERR, "(%u,%hu): invalid bcn, drop packet\n",
                               conn->peer_id, conn->sessionId);
            return -1;
        }

        if((conn->peer_credits - bcn) != required_credits)
        {
            LOGGER(LOG_PKT, "(%u,%hu): peer says bcn is %hu, but peer credits %hu "
                                "- cost %hu is %d using scalar %hu\n",
                                conn->peer_id, conn->sessionId,
                                bcn, conn->peer_credits, required_credits,
                                conn->peer_credits - required_credits, conn->local_credit_scalar);

            if(conn->peer_credits - bcn > 0)
            {
                LOGGER(LOG_PKT, "(%u,%hu): using router bcn\n",
                                    conn->peer_id, conn->sessionId);

                required_credits = conn->peer_credits - bcn;
            }
            else
            {
                LOGGER(LOG_PKT, "(%u,%hu): ignore router bcn\n",
                                    conn->peer_id, conn->sessionId);
            }
        }
        else
        {
            LOGGER(LOG_PKT, "(%u,%hu): bcn is %hu pkt requires %hu creidts using scalar %hu\n",
                                conn->peer_id, conn->sessionId, bcn, required_credits, conn->local_credit_scalar);
        }


        /*
         * Check to make sure peer hasn't violated their credit allowance.
         * This also protects against roll-under
         */
        if (conn->peer_credits < required_credits)
        {
            /* drop the packet */
            LOGGER(LOG_ERR, "(%u,%hu): Peer exceeded their credit allowance in an "
                               "inband grant packet peer_credits %hu, required_credits %hu, drop packet\n",
                               conn->peer_id, conn->sessionId,
                               conn->peer_credits, required_credits);
            return -1;
        }

        /* save consumed credits */
        consumed_credits = required_credits;
    }
    else
    {
        /*
         * Check to make sure peer hasn't violated their credit allowance.
         * This also protects against roll-under
         */
        UINT16_t required_credits = compute_peer_credits (conn, packet);

        if (conn->peer_credits < required_credits)
        {
            /* drop the packet */
            LOGGER(LOG_ERR, "(%u,%hu): Peer exceeded their credit allowance "
                               "peer_credits %hu, required_credits %hu, drop packet\n",
                               conn->peer_id, conn->sessionId,
                               conn->peer_credits, required_credits);
            return -1;
        }

        /* save consumed credits */
        consumed_credits = required_credits;
    }

    /* decrement peer credits */
    del_peer_credits(conn, consumed_credits);

    LOGGER(LOG_PKT, "(%u,%hu): payload len %d, consumed_credits %hu, of %hu peer_credits = %hu using scalar %hu\n",
                        conn->peer_id, conn->sessionId, ntohs (packet->pppoe_length),
                        consumed_credits, old_peer_credits, conn->peer_credits, conn->peer_credit_scalar);

    /* handle session data */
    return handle_session_packet_to_peer (conn, packet, consumed_credits);
}


/**********************************************************************
*%FUNCTION: get_pppoe_conn()
*%ARGUMENTS:
*
*%RETURNS:
* PPPoEConnection
*%DESCRIPTION:
* Returns the global PPPoEConnection
***********************************************************************/
PPPoEConnection *
get_pppoe_conn ()
{
    return (Connection);
}

