/*-----------------------------------------------------------------------------/
 * project: rfc4938
 * file: pppoe_rfc4938.c
 * version: 1.0
 * date: October 21, 2007
 *
 * Copyright (c) 2013 - Adjacent Link, LLC, Bridgewater NJ
 * Copyright (C) 2007-2008 Cisco Systems, Inc.
 *
 * ===========================
 *
 * This file implements functions related to rfc4938, "PPP Over Ethernet (PPPoE)
 * Extensions for Credit Flow and Link Metrics" and "PPP Over Ethernet (PPPoE)
 * Extensions for Scaled Credits and Link Metrics"
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

#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include "pppoe_rfc4938.h"
#include "pppoe_rfc4938_nbr.h"

#include <signal.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include <sys/ioctl.h>

static UINT16_t padg_seq_num = 0;
static struct itimerval padg_timer;


/***********************************************************************
 *%FUNCTION: init_flow_control
 *%ARGUMENTS:
 * conn -- PPPoE connection
 * credit_scalar -- local credit scalar to advertise in PADR
 * rfc4938_debug -- debug level
 * my_port -- port to listen on
 * grant_amount -- grant_amount to set for PADR grant and 1 second grant
 *
 *%RETURNS:
 * Nothing
 *%DESCRIPTION:
 * Inits flow control related parameters
 ***********************************************************************/
void
pppoe_init_flow_control (PPPoEConnection * conn, UINT16_t credit_scalar, int rfc4938_debug,
                         UINT16_t my_port, UINT16_t parent_port, UINT32_t peer_pid,
                         UINT16_t grant_amount, UINT16_t timed_credits)
{
    int ret, z, flag;
    socklen_t len_inet;

    if (credit_scalar == 0)
    {
        conn->local_credit_scalar = RFC4938_CREDIT_SCALAR;
        conn->peer_credit_scalar = RFC4938_CREDIT_SCALAR;
        conn->mode = MODE_RFC4938_ONLY;
        conn->scalar_state = SCALAR_NOT_NEEDED;
    }
    else
    {
        conn->local_credit_scalar = credit_scalar;
        conn->mode = MODE_RFC4938_SCALING;
        conn->scalar_state = SCALAR_NEEDED;

        /* set the default credit to 64 for peer until we learn it */
        conn->peer_credit_scalar = RFC4938_CREDIT_SCALAR;
    }

    conn->local_credits = 0;
    conn->peer_credits  = 0;
    conn->credit_cache  = 0;
    conn->credits_pending_padc = 0;

    conn->grant_state       = PADC_RECEIVED;
    conn->padg_tries        = 0;
    conn->send_inband_grant = 0;

    /* setup credit grant amount */
    conn->grant_limit    = grant_amount;
    conn->timed_credits  = timed_credits;

    if(conn->timed_credits != 0)
    {
        if (pipe (conn->signalPipe) < 0)
        {
            fatalSys ("init_flow_control(): Could NOT create pipe", strerror(errno));

            return;
        }

        /* setup padg grant timer */
        signal (SIGALRM, pppoe_signal_handler);

        padg_timer.it_value.tv_sec = conn->timed_credits;
        padg_timer.it_value.tv_usec = 0;
        padg_timer.it_interval.tv_sec = conn->timed_credits;
        padg_timer.it_interval.tv_usec = 0;

        ret = setitimer (ITIMER_REAL, &padg_timer, NULL);

        if (ret)
        {
            fatalSys ("init_flow_control(): Could NOT set itimer", strerror(errno));
            return;
        }
    }

    /* setup neighbor ports */
    conn->peer_pid = peer_pid;
    conn->my_port = my_port;
    conn->parent_port = parent_port;

    /* setup debugs */
    if (rfc4938_debug >= 1)
    {
        pppoe_set_debug_mask (PPPOE_G_ERROR_DEBUG);
    }
    if (rfc4938_debug >= 2)
    {
        pppoe_set_debug_mask (PPPOE_G_EVENT_DEBUG);
    }
    if (rfc4938_debug >= 3)
    {
        pppoe_set_debug_mask (PPPOE_G_PACKET_DEBUG);
    }

    /*
     * Create a UDP socket
     */
    conn->udpIPCSocket = socket (AF_INET, SOCK_DGRAM, 0);

    if (conn->udpIPCSocket == -1)
    {
        fatalSys ("init_flow_control():Could NOT create UDP socket", strerror(errno));

        return;
    }

    /* setup socket to receive packets on */
    memset (&conn->my_saddr, 0, sizeof conn->my_saddr);
    conn->my_saddr.sin_family = AF_INET;
    conn->my_saddr.sin_port = htons (conn->my_port);
    conn->my_saddr.sin_addr.s_addr = INADDR_ANY;

    len_inet = sizeof (conn->my_saddr);

    z = bind (conn->udpIPCSocket, (struct sockaddr *) &conn->my_saddr, len_inet);

    if (z == -1)
    {
        while (z == -1 && errno == EADDRINUSE)
        {
            /* increment port and try again if it was already in use */
            conn->my_port = conn->my_port + 1;
            conn->my_saddr.sin_port = htons (conn->my_port);

            len_inet = sizeof (conn->my_saddr);

            z = bind (conn->udpIPCSocket, (struct sockaddr *) &conn->my_saddr, len_inet);
        }
    }

    if (z == -1)
    {
        fatalSys ("init_flow_control():Could NOT bind UDP socket", strerror(errno));

        return;
    }

    /* set socket to nonblocking for the select */
    ioctl (conn->udpIPCSocket, FIONBIO, &flag);

    LOGGER(LOG_INFO, "(%u,%hu): Succesfully bound to port %hu\n",
                       conn->peer_id, conn->sessionId, conn->my_port);


    /* Alert parent rfc4938 process which port has been bound to and our pid*/
    send_child_ready (conn);

    /*
     * If this is the first neighbor to come up, we need to contact their
     * rfc4938 daemon with a CTL_SESSION_START msg
     */
    if(0) //currently not used
    {
        if (conn->peer_pid == 0)
        {
            if(conn->p2p_mode)
            {
                LOGGER(LOG_INFO, "(%u,%hu): Peer pid is 0, we are first up,"
                                   " sending session_start to peer\n",
                                   conn->peer_id, conn->sessionId);

                send_session_start (conn);
            }
        }
        else
        {
            /*
             * We are not the first to come up, send a CTL_SESSION_START_READY
             * msg to the other pppoe neighbor
             */

            if(conn->p2p_mode)
            {
                LOGGER(LOG_INFO, "(%u,%hu): Peer pid is %u, we are second up,"
                                   " sending session_start_ready to peer\n", 
                                   conn->peer_id, conn->sessionId, conn->peer_pid);

                send_session_start_ready (conn);
            }
        }
    }
}

/***********************************************************************
 *%FUNCTION: pppoe_signal_handler
 * handles all pppoe signal
 ***********************************************************************/
void
pppoe_signal_handler (int signo)
{
    PPPoEConnection *conn = get_pppoe_conn ();

    if (conn != NULL)
    {
        if (signo == SIGALRM)
        {
            if (write (conn->signalPipe[PIPE_WR_FD], "G", 1) < 0)
            {
                fatalSys ("pppoe_signal_handler(): Could NOT write to pipe", strerror(errno));
            }
        }
        else if (signo == SIGINT || signo == SIGHUP)
        {
            if (write (conn->signalPipe[PIPE_WR_FD], "T", 1) < 0)
            {
                fatalSys ("pppoe_signal_handler(): Could NOT write to pipe", strerror(errno));
            }
        }
    }
}


void
handle_signal_event (PPPoEConnection * conn)
{
    char signalCode = 0;

    if (conn == NULL)
    {
        LOGGER(LOG_ERR, "NULL connection\n");
        return;
    }

    if (read (conn->signalPipe[PIPE_RD_FD], &signalCode, sizeof(signalCode)) != sizeof(signalCode))
    {
        fatalSys ("handle_signal_event(): Could NOT read from pipe", strerror(errno));

        return;
    }

    switch (signalCode)
    {
    /* grant */
    case 'G':
    {
        sendOutOfBandGrant (conn, conn->grant_limit);

        /* reset signal handler */
        signal (SIGALRM, pppoe_signal_handler);
    }
    break;

    /* terminate */
    case 'T':
    {
        sendPADTf (conn, "PPPoEClient: received terminate signal", 1);

        exit (EXIT_SUCCESS);
    }
    break;
    }
}


void
sendOutOfBandGrant (PPPoEConnection * conn, UINT16_t credits)
{
    /* check to see if we are waiting on a PADC */
    if (conn->grant_state == PADG_SENT)
    {
        time_t now = time(NULL);

        /* have we exceeded the time limit */
        if ((conn->padg_initial_send_time + MAX_PADC_WAIT_TIME) <= now)
        {
            /* terminate session */
            LOGGER(LOG_ERR, "(%u,%hu): Too many PADCs missed, sent %u PADG's, over interval %ld sec, sending PADT\n",
                               conn->peer_id, conn->sessionId, conn->padg_tries, now - conn->padg_initial_send_time) ;

            sendPADTandExit (conn, "PPPoEClient: Number of PADG retries exceeded", 1);
        }
        /* are we within the re-try window */
        else if (conn->padg_retry_send_time + PADG_RETRY_TIME > now)
        {
            LOGGER(LOG_PKT, "(%u,%hu): waiting for PADC seq 0x%04hx, add %hu credits to cache\n",
                                conn->peer_id, conn->sessionId, padg_seq_num, credits);

            BUMP_CREDITS(credits, &conn->credit_cache, conn->grant_limit);
        }
        else
        {
            LOGGER(LOG_PKT, "(%u,%hu): waiting for PADC, seq 0x%04hx, add %hu credits to cache,"
                                "resend PADG\n",
                                conn->peer_id, conn->sessionId, padg_seq_num, credits);

            BUMP_CREDITS(credits, &conn->credit_cache, conn->grant_limit);

            /* no, try again */
            sendPADG (conn, 0);
        }
    }
    else
    {
        LOGGER(LOG_PKT, "(%u,%hu): grant %hu, limit %hu, peer_credits %hu, local_credits %hu\n",
                            conn->peer_id, conn->sessionId,
                            credits, conn->grant_limit, conn->peer_credits,
                            conn->local_credits);

        sendPADG (conn, credits);

        BUMP_CREDITS(credits, &conn->credit_cache, conn->grant_limit);
    }
}

/***********************************************************************
 *%FUNCTION: sendPADG
 *%ARGUMENTS:
 * conn -- PPPoE connection
 * credits -- credits to grant in host order
 *
 *%RETURNS:
 * Nothing
 *%DESCRIPTION:
 * Sends a PADG packet
 ***********************************************************************/
void
sendPADG (PPPoEConnection * conn, UINT16_t grant_req)
{
    PPPoEPacket packet;
    PPPoETag creditTag;
    PPPoETag sequenceTag;
    unsigned char *cursor = packet.payload;

    UINT16_t plen = 0;
    UINT16_t fcn  = 0;
    UINT16_t bcn = conn->local_credits;

    /* Do nothing if no session established yet */
    if (!conn->sessionId)
    {
        LOGGER(LOG_INFO, "(%u): no session info, not sending PADG\n", conn->peer_id);

        return;
    }

    if (conn->grant_state == PADG_SENT && grant_req != 0)
    {
        /*
         * Our previous PADG has not been acknowledged.  No *new* credit grants can
         * be given until that grant is acknowledged.  We are going to send a grant with
         * 0 credits and the previous sequence number per the rfc to resolve this.
         */

        LOGGER(LOG_ERR, "(%u,%hu): grant_state is PADG_SENT, not sending PADG. "
                           " Waiting on seq_num 0x%04hx\n",
                           conn->peer_id, conn->sessionId, padg_seq_num);

        return;
    }

    memcpy (packet.eth_hdr.dest, conn->peerEth, PPPOE_ETH_ALEN);
    memcpy (packet.eth_hdr.source, conn->myEth, PPPOE_ETH_ALEN);

    packet.eth_hdr.proto = htons (Eth_PPPOE_Discovery);
    packet.pppoe_ver = 1;
    packet.pppoe_type = 1;
    packet.pppoe_code = CODE_PADG;
    packet.pppoe_session = htons (conn->sessionId);

    /* Add Sequence Number */
    if (conn->grant_state == PADC_RECEIVED)
    {
        padg_seq_num++;

        if(conn->credit_cache > 0)
        {
            UINT16_t current_limit = conn->grant_limit - conn->peer_credits;

            LOGGER(LOG_INFO, "(%u,%hu): cache_credits %hu, grant_req %hu, current limit %hu",
                               conn->peer_id, conn->sessionId,
                               conn->credit_cache, grant_req, current_limit);

            BUMP_CREDITS(conn->credit_cache, &grant_req, current_limit);

            conn->credit_cache = 0;
        }

        conn->credits_pending_padc = grant_req;

        conn->padg_retry_send_time = conn->padg_initial_send_time = time(NULL);
    }
    else
    {
        /*
         * since a grant has not been acknowldeged, we are going to send
         * a grant with zero credits and the same sequence number
         * per the rfc to try to get it acknowledged
         */
        LOGGER(LOG_INFO, "(%u,%hu): resending PADG with seq_num 0x%04hx\n",
                           conn->peer_id, conn->sessionId, padg_seq_num);


        conn->padg_retry_send_time = time(NULL);
    }

    add_sequence_tag (&sequenceTag, padg_seq_num);
    memcpy (cursor, &sequenceTag, sizeof (padg_seq_num) + TAG_HDR_SIZE);
    plen   += sizeof (padg_seq_num) + TAG_HDR_SIZE;
    cursor += sizeof (padg_seq_num) + TAG_HDR_SIZE;

    fcn = grant_req;

    /* Add credit Tag */
    add_credit_tag (&creditTag, fcn, bcn);
    memcpy (cursor, &creditTag, TAG_CREDITS_LENGTH + TAG_HDR_SIZE);
    plen   += TAG_CREDITS_LENGTH + TAG_HDR_SIZE;
    cursor += TAG_CREDITS_LENGTH + TAG_HDR_SIZE;

    conn->padg_tries++;

    packet.pppoe_length = htons (plen);

    send_discovery_packet_to_ac (conn, &packet);

    LOGGER(LOG_PKT, "(%u,%hu): sent PADG fcn:%hu (peer), pending_padc %hu, bcn:%hu (local), seq:0x%04hx, try %u\n",
                        conn->peer_id, conn->sessionId, fcn, conn->credits_pending_padc,
                        bcn, padg_seq_num, conn->padg_tries);

    /* set PADG_SENT state */
    conn->grant_state = PADG_SENT;
}


/***********************************************************************
 *%FUNCTION: sendPADC
 *%ARGUMENTS:
 * conn -- PPPoE connection
 * seq -- sequence number in host order
 *
 *%RETURNS:
 * Nothing
 *%DESCRIPTION:
 * Sends a PADC packet
 ***********************************************************************/
void
sendPADC (PPPoEConnection * conn, UINT16_t seq)
{
    PPPoEPacket packet;
    PPPoETag creditTag;
    PPPoETag sequenceTag;
    unsigned char *cursor = packet.payload;

    UINT16_t plen = 0;
    UINT16_t fcn = conn->peer_credits;
    UINT16_t bcn = conn->local_credits;

    /* Do nothing if no session established yet */
    if (!conn->sessionId)
    {
        LOGGER(LOG_INFO, "%u): no session info, not sending PADC\n", conn->peer_id);

        return;
    }

    memcpy (packet.eth_hdr.dest, conn->peerEth, PPPOE_ETH_ALEN);
    memcpy (packet.eth_hdr.source, conn->myEth, PPPOE_ETH_ALEN);

    packet.eth_hdr.proto = htons (Eth_PPPOE_Discovery);
    packet.pppoe_ver = 1;
    packet.pppoe_type = 1;
    packet.pppoe_code = CODE_PADC;
    packet.pppoe_session = htons (conn->sessionId);

    /* Add Sequence Number */
    add_sequence_tag (&sequenceTag, seq);
    memcpy (cursor, &sequenceTag, sizeof (seq) + TAG_HDR_SIZE);
    plen   += sizeof (seq) + TAG_HDR_SIZE;
    cursor += sizeof (seq) + TAG_HDR_SIZE;

    /* Add credit Tag */
    add_credit_tag (&creditTag, fcn, bcn);
    memcpy (cursor, &creditTag, TAG_CREDITS_LENGTH + TAG_HDR_SIZE);
    plen   += TAG_CREDITS_LENGTH + TAG_HDR_SIZE;
    cursor += TAG_CREDITS_LENGTH + TAG_HDR_SIZE;

    packet.pppoe_length = htons (plen);

    send_discovery_packet_to_ac (conn, &packet);

    LOGGER(LOG_PKT, "(%u,%hu): sent PADC fcn:%hu (peer), bcn:%hu (local), seq:0x%04hx\n",
                        conn->peer_id, conn->sessionId, fcn, bcn, seq);
}


/***********************************************************************
 *%FUNCTION: recvPADG
 *%ARGUMENTS:
 * conn -- PPPoE connection
 * packet -- PPPoE Packet
 *
 *%RETURNS:
 * Nothing
 *%DESCRIPTION:
 * Receives a PADG packet
 ***********************************************************************/
void
recvPADG (PPPoEConnection * conn, PPPoEPacket * packet)
{
    PPPoETag creditTag;
    PPPoETag sequenceTag;

    UINT16_t fcn;
    UINT16_t bcn;
    UINT16_t seq;

    /* find sequence tag */
    if (findTag (packet, TAG_RFC4938_SEQ_NUM, &sequenceTag) == NULL)
    {
        LOGGER(LOG_ERR, "(%u,%hu): no sequence tag in PADG packet\n",
                           conn->peer_id, conn->sessionId);
        return;
    }

    /* find credit tag */
    if (findTag (packet, TAG_RFC4938_CREDITS, &creditTag) == NULL)
    {
        LOGGER(LOG_ERR, "(%u,%hu): no credit tag in PADG packet\n",
                           conn->peer_id, conn->sessionId);
        return;
    }


    fcn = get_fcn_from_credit_tag (&creditTag);
    bcn = get_bcn_from_credit_tag (&creditTag);
    seq = get_seq_from_sequence_tag (&sequenceTag);

    LOGGER(LOG_PKT, "(%u,%hu): fcn:%hu, bcn:%hu, seq:0x%04hx\n",
                        conn->peer_id, conn->sessionId,
                        fcn, bcn, seq);

    /* add credits */
    handle_credit_grant(conn, fcn, bcn);

    /* send PADC in response */
    sendPADC (conn, seq);
}

/***********************************************************************
 *%FUNCTION: recvPADC
 *%ARGUMENTS:
 * conn -- PPPoE connection
 * packet -- PPPoE Packet
 *
 *%RETURNS:
 * Nothing
 *%DESCRIPTION:
 * Receives a PADC packet
 ***********************************************************************/
void
recvPADC (PPPoEConnection * conn, PPPoEPacket * packet)
{
    PPPoETag creditTag;
    PPPoETag sequenceTag;

    UINT16_t fcn;
    UINT16_t bcn;
    UINT16_t seq;

    /* find sequence tag */
    if (findTag (packet, TAG_RFC4938_SEQ_NUM, &sequenceTag) == NULL)
    {
        LOGGER(LOG_ERR, "(%u,%hu)s: no sequence tag in PADC packet, ignore\n",
                           conn->peer_id, conn->sessionId);
        return;
    }

    /* findTag to pull out credits */
    if (findTag (packet, TAG_RFC4938_CREDITS, &creditTag) == NULL)
    {
        LOGGER(LOG_ERR, "(%u,%hu): no credit tag in PADC packet, ignore\n",
                           conn->peer_id, conn->sessionId);
        return;
    }

    fcn = get_fcn_from_credit_tag (&creditTag);
    bcn = get_bcn_from_credit_tag (&creditTag);
    seq = get_seq_from_sequence_tag (&sequenceTag);

    LOGGER(LOG_PKT, "(%u,%hu): fcn:%hu, bcn:%hu, seq:0x%04hx\n",
                        conn->peer_id, conn->sessionId, fcn, bcn, seq);


    /* make sure this is an acknowledgement of the last padg that you sent */
    if (seq == padg_seq_num)
    {
        if(conn->grant_state == PADC_RECEIVED)
        {
            LOGGER(LOG_INFO, "(%u,%hu): ignore duplicate PADC sequence number 0x%04hx",
                               conn->peer_id, conn->sessionId, seq);
        }
        else
        {
            if(conn->credits_pending_padc > 0)
            {
                LOGGER(LOG_INFO, "(%u,%hu): adding %hu credits_pending_padc to %hu peer_credits",
                                   conn->peer_id, conn->sessionId, conn->credits_pending_padc, conn->peer_credits);

                conn->peer_credits += conn->credits_pending_padc;
                conn->credits_pending_padc = 0;
            }

            /* sync credits */
            sync_credit_grant(conn, fcn, bcn);

            /* set PADC_RECEIVED state */
            conn->grant_state = PADC_RECEIVED;
            conn->padg_tries = 0;
        }
    }
    else
    {
        LOGGER(LOG_INFO, "(%u,%hu): received PADC with incorrect"
                           " sequence number.  Expected 0x%04hx Received"
                           " 0x%04hx, NOT updating grant_state and padg_retries",
                           conn->peer_id, conn->sessionId, padg_seq_num, seq);
    }
}

/***********************************************************************
 *%FUNCTION: sendPADQ
 *%ARGUMENTS:
 * conn -- PPPoE connection
 * mdr -- Maximum Data Rate
 * mdr_scalar -- Maximum Data Rate scalar (0-kbps, 1-Mbps, 2-Gbps, 3-Tbps)
 * cdr -- Current Data Rate
 * cdr_scalar -- Current Data Rate scalar (0-kbps, 1-Mbps, 2-Gbps, 3-Tbps)
 * latency
 * resources
 * rlq -- Relative Link Quality
 * receive_only -- true (1) or false (0)
 *
 *%RETURNS:
 * Nothing
 *%DESCRIPTION:
 * Sends a PADQ packet
 ***********************************************************************/
void
sendPADQ (PPPoEConnection * conn, UINT16_t mdr, UINT8_t mdr_scalar,
          UINT16_t cdr, UINT8_t cdr_scalar, UINT16_t latency,
          UINT8_t resources, UINT8_t rlq, UINT8_t receive_only)
{
    PPPoEPacket packet;
    PPPoETag padqTag;

    UINT8_t *padq_cursor = padqTag.payload;
    UINT8_t *cursor = packet.payload;

    UINT16_t plen = 0;
    UINT16_t reserved = 0;
    UINT16_t temp = 0;

    /* Do nothing if no session established yet */
    if (!conn->sessionId)
    {
        LOGGER(LOG_INFO, "(%u): no session info, not sending PADQ\n", conn->peer_id);

        return;
    }

    memcpy (packet.eth_hdr.dest, conn->peerEth, PPPOE_ETH_ALEN);
    memcpy (packet.eth_hdr.source, conn->myEth, PPPOE_ETH_ALEN);

    packet.eth_hdr.proto = htons (Eth_PPPOE_Discovery);
    packet.pppoe_ver = 1;
    packet.pppoe_type = 1;
    packet.pppoe_code = CODE_PADQ;
    packet.pppoe_session = htons (conn->sessionId);

    padqTag.type = htons (TAG_RFC4938_METRICS);
    padqTag.length = htons (TAG_METRICS_LENGTH);

    if (receive_only > 1)
    {
        LOGGER(LOG_ERR, "(%u,%hu): receive_only value must be <= 1\n",
                           conn->peer_id, conn->sessionId);
        return;
    }

    if (conn->mode != MODE_RFC4938_ONLY)
    {
        if (mdr_scalar > 3)
        {
            LOGGER(LOG_ERR, "(%u,%hu): mdr_scalar value must be <= 3\n",
                               conn->peer_id, conn->sessionId);
            return;
        }
        if (cdr_scalar > 3)
        {
            LOGGER(LOG_ERR, "(%u,%hu): cdr_scalar value must be <= 3\n",
                               conn->peer_id, conn->sessionId);
            return;
        }

        reserved = (mdr_scalar << 3) | (cdr_scalar << 1) | receive_only;
    }
    else
    {
        reserved = receive_only;
    }

    /* copy reserved field into the padq */
    temp = htons (reserved);
    memcpy (padq_cursor, &temp, sizeof (reserved));
    padq_cursor += sizeof (reserved);

    /* copy rlq */
    memcpy (padq_cursor, &rlq, sizeof (rlq));
    padq_cursor += sizeof (rlq);

    /* copy resources */
    memcpy (padq_cursor, &resources, sizeof (resources));
    padq_cursor += sizeof (resources);

    /* copy latency */
    temp = htons (latency);
    memcpy (padq_cursor, &temp, sizeof (latency));
    padq_cursor += sizeof (latency);

    /* copy cdr */
    temp = htons (cdr);
    memcpy (padq_cursor, &temp, sizeof (cdr));
    padq_cursor += sizeof (cdr);

    /* copy mdr */
    temp = htons (mdr);
    memcpy (padq_cursor, &temp, sizeof (mdr));
    padq_cursor += sizeof (mdr);

    /* copy the tag into the packet */
    memcpy (cursor, &padqTag, TAG_METRICS_LENGTH + TAG_HDR_SIZE);
    plen   += TAG_METRICS_LENGTH + TAG_HDR_SIZE;
    cursor += TAG_METRICS_LENGTH + TAG_HDR_SIZE;

    packet.pppoe_length = htons (plen);

    send_discovery_packet_to_ac (conn, &packet);

    LOGGER(LOG_PKT, "(%u,%hu): sent PADQ packet with mdr:%hu cdr:%hu latency:%hu, resources %hhu, rlq %hhu",
                        conn->peer_id, conn->sessionId, mdr, cdr, latency, resources, rlq);
}


/***********************************************************************
 *%FUNCTION: recvPADQ
 *%ARGUMENTS:
 * conn -- PPPoE Connection
 * packet -- PPPoE Packet
 *%RETURNS:
 * Nothing
 *%DESCRIPTION:
 * Receives a PADQ packet
 ***********************************************************************/
void
recvPADQ (PPPoEConnection * conn, PPPoEPacket * packet)
{
    PPPoETag padqTag;

    /* find credit tag */
    if (findTag (packet, TAG_RFC4938_METRICS, &padqTag) == NULL)
    {
        LOGGER(LOG_ERR, "(%u,%hu): no credit tag in PADQ packet, ignore.\n",
                           conn->peer_id, conn->sessionId);
        return;
    }

    if (padqTag.length == 0)
    {
        LOGGER(LOG_PKT, "(%u,%hu): received PADQ query packet\n",
                            conn->peer_id, conn->sessionId);

        /*
         * Send PADQ in response.  Normally you would want to send PADQ data
         * that represents your current link, but since we are only providing
         * padq's with data from the user, we are going to send "dummy" values
         * here.
         */
    }
    else
    {

        /* ignore this PADQ */
        LOGGER(LOG_ERR, "(%u,%hu): received a PADQ packet from the server, ignore.\n",
                           conn->peer_id, conn->sessionId);
    }
}



/***********************************************************************
 *%FUNCTION: sendInBandGrant
 *%ARGUMENTS:
 * conn -- PPPoE Connection
 * packet -- PPPoE Packet
 * credits -- credits to grant in host order
 *
 *%RETURNS:
 * Amount of credits consumed by packet
 *%DESCRIPTION:
 * Receives an inband credit grant
 ***********************************************************************/
UINT16_t
sendInBandGrant (PPPoEConnection * conn, PPPoEPacket * packet, UINT16_t credits)
{
    PPPoETag creditTag;
    UINT8_t payload_data[PPPOE_ETH_DATA_LEN];
    UINT16_t len = ntohs (packet->pppoe_length);
    unsigned char *cursor = packet->payload;

    UINT16_t consumed_credits;
    UINT16_t fcn;
    UINT16_t bcn;

    /*
     * check to make sure the mtu would not be exceeded if the credit tag
     * were to be added
     */
    if (len + TAG_CREDITS_LENGTH + TAG_HDR_SIZE + PPPOE_OVERHEAD > MAX_PPPOE_MTU)
    {
        /* don't add tag */

        /* compute credits normally for this packet */
        UINT16_t required_credits = compute_local_credits (conn, packet);

        if(conn->local_credits >= required_credits)
        {
            conn->local_credits -= required_credits;
        }
        else
        {
            conn->local_credits = 0;
        }

        LOGGER(LOG_PKT, "(%u,%hu): Request will exceed MTU, NOT adding tag, done\n",
                            conn->peer_id, conn->sessionId);

        return 0;
    }


    /* don't send more than max credits */
    if (MAX_CREDITS - conn->peer_credits < credits)
    {
        credits = MAX_CREDITS - conn->peer_credits;
    }

    fcn = credits;
    bcn = conn->local_credits;

    /* copy the original payload to our tmp buffer */
    memcpy (&payload_data, packet->payload, len);

    /* insert credit tag into payload */
    add_credit_tag (&creditTag, fcn, bcn);
    memcpy (cursor, &creditTag, TAG_CREDITS_LENGTH + TAG_HDR_SIZE);
    cursor += TAG_CREDITS_LENGTH + TAG_HDR_SIZE;

    /* copy old packet over */
    memcpy (cursor, payload_data, len);

    /* update new packet length */
    packet->pppoe_length = htons (len + TAG_CREDITS_LENGTH + TAG_HDR_SIZE);

    /* add credits granted to peer credits */
    conn->peer_credits += credits;

    consumed_credits = compute_local_credits_with_inband (conn, packet);

    LOGGER(LOG_PKT, "(%u,%hu): peer_credits %hu, consumed_credits %hu, fcn:%hu, bcn:%hu\n",
     conn->peer_id, conn->sessionId, conn->peer_credits, consumed_credits, fcn, bcn);

    /* reset the inband_grant flag */
    conn->send_inband_grant = 0;

    return (consumed_credits);
}



/***********************************************************************
 *%FUNCTION: recvInBandGrant
 *%ARGUMENTS:
 * conn -- PPPoE Connection
 * packet -- PPPoE Packet
 *%RETURNS:
 * Nothing
 *%DESCRIPTION:
 * Receives an inband credit grant
 ***********************************************************************/
int
recvInBandGrant (PPPoEConnection * conn, PPPoEPacket * packet)
{
    PPPoETag creditTag;
    UINT16_t fcn;
    UINT16_t bcn;

    UINT16_t len = ntohs (packet->pppoe_length);

    /* Alignment is not guaranteed, so do this by hand... */
    UINT16_t tagType  = get_word_from_buff(packet->payload, 0);
    UINT16_t tagSize  = get_word_from_buff(packet->payload, 2) + TAG_HDR_SIZE;

    if (tagSize > len)
    {
        LOGGER(LOG_ERR, "(%u,%hu): invalid PPPoE tag length in"
                           " inband grant tag size %hu, tag type %hu, pkt len %hu, ignore.",
                           conn->peer_id, conn->sessionId, tagSize, tagType, len);
        return -1;
    }

    /* copy the tag into our local structure */
    memcpy (&creditTag, packet->payload, tagSize);

    fcn = get_fcn_from_credit_tag (&creditTag);
    bcn = get_bcn_from_credit_tag (&creditTag);

    LOGGER(LOG_PKT, "(%u,%hu): fcn:%hu, bcn:%hu\n",
                        conn->peer_id, conn->sessionId, fcn, bcn);

    /* add fcn credits */
    handle_inband_grant(conn, fcn, bcn);

    /* now slide the data up over the tag */
    memmove(packet->payload, packet->payload + tagSize, len - tagSize);

    /* adjust the packet length */
    packet->pppoe_length = htons(len - tagSize);

    return bcn;
}

/***********************************************************************
 *%FUNCTION: add_credit_tag
 *%ARGUMENTS:
 * tag -- tag to fill in
 * fcn -- fcn value to use in network order
 * bcn -- bcn value to use in network order
 *%RETURNS:
 * Nothing
 *%DESCRIPTION:
 * Adds a credit tag to a tag passed in
 ***********************************************************************/
void
add_credit_tag (PPPoETag * tag, UINT16_t fcn, UINT16_t bcn)
{
    tag->type   = htons (TAG_RFC4938_CREDITS);
    tag->length = htons (TAG_CREDITS_LENGTH);

    fcn = htons(fcn);
    bcn = htons(bcn);

    memcpy (tag->payload, &fcn, sizeof(fcn));
    memcpy (tag->payload + sizeof(fcn), &bcn, sizeof(bcn));
}


/***********************************************************************
 *%FUNCTION: add_sequence_tag
 *%ARGUMENTS:
 * tag -- tag to fill in
 * seq -- sequence number to use in network order
 *%RETURNS:
 * Nothing
 *%DESCRIPTION:
 * Adds a sequence tag to a tag passed in
 ***********************************************************************/
void
add_sequence_tag (PPPoETag * tag, UINT16_t seq)
{
    tag->type   = htons (TAG_RFC4938_SEQ_NUM);
    tag->length = htons (sizeof (seq));

    seq = htons(seq);

    memcpy (tag->payload, &seq, sizeof (seq));
}


/***********************************************************************
 *%FUNCTION: add_scalar_tag
 *%ARGUMENTS:
 * tag -- tag to fill in
 * scalar -- scalar value to use in network order
 *%RETURNS:
 * Nothing
 *%DESCRIPTION:
 * Adds a scalar tag to a tag passed in
 ***********************************************************************/
void
add_scalar_tag (PPPoETag * tag, UINT16_t scalar)
{
    tag->type   = htons (TAG_RFC4938_SCALAR);
    tag->length = htons (sizeof (scalar));

    scalar = htons(scalar);

    memcpy (tag->payload, &scalar, TAG_SCALAR_LENGTH);
}


/***********************************************************************
 *%FUNCTION: compute_local_credits
 *%ARGUMENTS:
 * conn -- PPPoE connection
 * packet -- PPPoE packet
 *%RETURNS:
 * UINT16_t -- credits consumed by packet
 *%DESCRIPTION:
 * Computes credit consumption of a local packet
 ***********************************************************************/
UINT16_t
compute_local_credits (PPPoEConnection * conn, PPPoEPacket * packet)
{
    UINT16_t credits = 0;

    /*
     * Credits are calculated based on ppp payload, therefore you must
     * subtract the PPP header out from the PPPoE payload
     */
    UINT16_t len = ntohs (packet->pppoe_length) - PPP_OVERHEAD;

    credits = len / conn->local_credit_scalar;

    if (len % conn->local_credit_scalar != 0)
    {
        credits++;
    }

    return (credits);
}


/***********************************************************************
 *%FUNCTION: compute_local_credits_with_inband
 *%ARGUMENTS:
 * conn -- PPPoE connection
 * packet -- PPPoE packet
 *%RETURNS:
 * UINT16_t -- credits consumed by packet
 *%DESCRIPTION:
 * Computes credit consumption of a local packet
 ***********************************************************************/
UINT16_t
compute_local_credits_with_inband (PPPoEConnection * conn, PPPoEPacket * packet)
{
    UINT16_t credits = 0;

    /*
     * Credits are calculated based on ppp payload, therefore you must
     * subtract the PPP header out from the PPPoE payload
     */
    UINT16_t len = ntohs (packet->pppoe_length) - PPP_OVERHEAD;

    credits = len / conn->local_credit_scalar;

    if (len % conn->local_credit_scalar != 0)
    {
        credits++;
    }

    return (credits);
}


/***********************************************************************
 *%FUNCTION: compute_peer_credits
 *%ARGUMENTS:
 * conn -- PPPoE connection
 * packet -- PPPoE packet
 *%RETURNS:
 * UINT16_t -- credits consumed by packet
 *%DESCRIPTION:
 * Computes credit consumption of a peer packet
 ***********************************************************************/
UINT16_t
compute_peer_credits (PPPoEConnection * conn, PPPoEPacket * packet)
{
    UINT16_t credits = 0;

    /*
     * Credits are calculated based on ppp payload, therefore you must
     * subtract the PPP header out from the PPPoE payload
     */

    if(conn->mode == MODE_RFC4938_ONLY)
    {
        return (credits);
    }
    else
    {
        UINT16_t len = ntohs (packet->pppoe_length) - PPP_OVERHEAD;

        credits = len / conn->peer_credit_scalar;

        if (len % conn->peer_credit_scalar != 0)
        {
            credits++;
        }

        return (credits);
    }
}

/***********************************************************************
 *%FUNCTION: compute_peer_credits_with_inband
 *%ARGUMENTS:
 * conn -- PPPoE connection
 * packet -- PPPoE packet
 *%RETURNS:
 * UINT16_t -- credits consumed by packet
 *%DESCRIPTION:
 * Computes credit consumption of a peer packet
 ***********************************************************************/
UINT16_t
compute_peer_credits_with_inband (PPPoEConnection * conn, PPPoEPacket * packet)
{
    UINT16_t credits = 0;

    /*
     * Credits are calculated based on ppp payload, therefore you must
     * subtract the PPP header out from the PPPoE payload
     */
    UINT16_t len = ntohs (packet->pppoe_length) - PPP_OVERHEAD - TAG_HDR_SIZE - TAG_CREDITS_LENGTH;

    credits = len / conn->peer_credit_scalar;

    if (len % conn->peer_credit_scalar != 0)
    {
        credits++;
    }

    return (credits);
}

/***********************************************************************
 *%FUNCTION: get_fcn_from_credit_tag
 *%ARGUMENTS:
 * tag -- tag to decode
 *%RETURNS:
 * UINT16_t -- fcn value
 *%DESCRIPTION:
 * decodes fcn value from a credit tag
 ***********************************************************************/
UINT16_t
get_fcn_from_credit_tag (PPPoETag * tag)
{
    return get_word_from_buff(tag->payload, 0);
}

/***********************************************************************
 *%FUNCTION: get_fcn_from_credit_tag
 *%ARGUMENTS:
 * tag -- tag to decode
 *%RETURNS:
 * UINT16_t -- fcn value
 *%DESCRIPTION:
 * decodes fcn value from a credit tag
 ***********************************************************************/
UINT16_t
get_bcn_from_credit_tag (PPPoETag * tag)
{
    return get_word_from_buff(tag->payload, 2);
}


/***********************************************************************
 *%FUNCTION: get_seq_from_sequence_tag
 *%ARGUMENTS:
 * tag -- tag to decode
 *%RETURNS:
 * UINT16_t -- seq value
 *%DESCRIPTION:
 * decodes sequence value from a sequence tag
 ***********************************************************************/
UINT16_t
get_seq_from_sequence_tag (PPPoETag * tag)
{
    return get_word_from_buff(tag->payload, 0);
}


UINT16_t
get_word_from_buff (UINT8_t * p, int i)
{
    twobyte_t u;

    u.byte[0] = p[i++];
    u.byte[1] = p[i++];

    return (ntohs(u.word));
}


void add_peer_credits(PPPoEConnection *conn, UINT16_t credits)
{
    if((conn->peer_credits + credits) < MAX_CREDITS)
    {
        conn->peer_credits += credits;
    }
    else
    {
        conn->peer_credits = MAX_CREDITS;
    }
}


void del_peer_credits(PPPoEConnection *conn, UINT16_t credits)
{
    if(conn->peer_credits > credits)
    {
        conn->peer_credits -= credits;
    }
    else
    {
        conn->peer_credits = 0;
    }
}
