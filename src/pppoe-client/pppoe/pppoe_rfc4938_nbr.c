/*-----------------------------------------------------------------------------/
 * project: rfc4938
 * file: pppoe_rfc4938_nbr.c
 * version: 1.0
 * date: October 21, 2007
 *
 * Copyright (c) 2013-2014 - Adjacent Link, LLC, Bridgewater NJ
 * Copyright owner (c) 2007 by cisco Systems, Inc.
 *
 * ===========================
 *
 * This file implements functions in communicating with a neighbor
 * which also has the pppoe client side implmentation of rfc4938.
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

#include "pppoe.h"
#include "pppoe_rfc4938_nbr.h"

#include "../rfc4938.h"
#include "../rfc4938_messages.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>

static int pppoe_ctl_packet_parser (unsigned char *buff, int msglen, struct sockaddr_in *source_addr, PPPoEPacket * packet);


static int handle_session_start_ready (rfc4938_ctl_message_t * p2ctlmsg);

static int handle_session_stop (rfc4938_ctl_message_t * p2ctlmsg);

static int handle_session_data_from_peer (rfc4938_ctl_message_t * p2ctlmsg, PPPoEPacket * packet);

static int handle_frame_data (rfc4938_ctl_message_t * p2ctlmsg, PPPoEPacket * packet);

static int handle_session_padq (rfc4938_ctl_message_t * p2ctlmsg);

static int handle_session_padg (rfc4938_ctl_message_t * p2ctlmsg);


static int send_packet_to_peer (PPPoEConnection * conn, void *p2buffer, int buflen);

static int send_packet_to_parent (PPPoEConnection * conn, void *p2buffer, int buflen);

static int send_udp_packet (PPPoEConnection * conn, void *p2buffer, int buflen);

static int handle_ppp_packet_bc_mode(PPPoEConnection *conn, PPPoEPacket *packet);


static int check_for_lcp_echo_request(PPPoEConnection *conn, PPPoEPacket *packet);

static void check_for_remote_magic_number(PPPoEConnection *conn, PPPoEPacket *packet);

static void check_for_local_magic_number(PPPoEConnection *conn, PPPoEPacket *packet);


static UINT32_t * get_lcp_config_magic_number(PPPoEConnection *conn, PPPHeader *p2ppp);


static int handle_lcp_echo_req(PPPoEConnection *conn, PPPoEPacket *packet, PPPHeader *p2ppp);

static int handle_lcp_config_req(PPPoEConnection *conn, PPPoEPacket *packet, PPPHeader *p2ppp);

static int handle_lcp_config_ack(PPPoEConnection *conn, PPPoEPacket *packet, PPPHeader *p2ppp);

static int handle_lcp_config_nack(PPPoEConnection *conn, PPPoEPacket *packet, PPPHeader *p2ppp);

static int handle_lcp_config_reject(PPPoEConnection *conn, PPPoEPacket *packet, PPPHeader *p2ppp);

static int handle_lcp_terminate_req(PPPoEConnection *conn, PPPoEPacket *packet, PPPHeader *p2ppp);

static int handle_lcp_terminate_ack(PPPoEConnection *conn, PPPoEPacket *packet, PPPHeader *p2ppp);

static int handle_lcp_code_reject(PPPoEConnection *conn, PPPoEPacket *packet, PPPHeader *p2ppp);



static int handle_ipcp_config_req(PPPoEConnection *conn, PPPoEPacket *packet, PPPHeader *p2ppp);

static int handle_ipcp_config_ack(PPPoEConnection *conn, PPPoEPacket *packet, PPPHeader *p2ppp);

static int handle_ipcp_config_nack(PPPoEConnection *conn, PPPoEPacket *packet, PPPHeader *p2ppp);

static int handle_ipcp_config_reject(PPPoEConnection *conn, PPPoEPacket *packet, PPPHeader *p2ppp);

static int handle_ipcp_terminate_req(PPPoEConnection *conn, PPPoEPacket *packet, PPPHeader *p2ppp);

static int handle_ipcp_terminate_ack(PPPoEConnection *conn, PPPoEPacket *packet, PPPHeader *p2ppp);

static int handle_ipcp_code_reject(PPPoEConnection *conn, PPPoEPacket *packet, PPPHeader *p2ppp);

static UINT32_t u32seqnum = 0;


int
handle_session_packet_to_peer (PPPoEConnection * conn, PPPoEPacket * packet, UINT16_t credits)
{
    /* in non p2p mode we will consume ppp lcp and ipcp packets */
    if(conn->p2p_mode == 0)
    {
        if(handle_ppp_packet_bc_mode(conn, packet) == 0)
        {
            return 0;
        }
    }

    if(conn->local_magic == 0)
    {
        check_for_local_magic_number(conn, packet);
    }

    if(conn->lcp_mode == 1)
    {
        if(check_for_lcp_echo_request(conn, packet) == 0)
        {
            return 0;
        }
    }

    return send_session_packet_to_peer (conn, packet, credits);
}


int
send_session_packet_to_peer (PPPoEConnection * conn, PPPoEPacket * packet, UINT16_t credits)
{
    int retval;
    int buflen;
    void *p2buffer = NULL;

    p2buffer = malloc (SIZEOF_CTL_CHILD_SESSION_DATA + ntohs (packet->pppoe_length));

    if (p2buffer == NULL)
    {
        LOGGER(LOG_ERR, "(%u,%hu): unable to malloc p2buffer\n",
                           conn->peer_id, conn->sessionId);
        return -1;
    }

    if ((buflen = rfc4938_ctl_format_child_session_data (u32seqnum, conn->peer_id,
                  ntohs(packet->pppoe_length),
                  credits,
                  packet->payload,
                  p2buffer)) !=
            (int) (SIZEOF_CTL_CHILD_SESSION_DATA + ntohs (packet->pppoe_length)))
    {
        LOGGER(LOG_ERR, "(%u,%hu): Unable to format message\n",
                           conn->peer_id, conn->sessionId);
        free (p2buffer);

        return -1;
    }

    LOGGER(LOG_PKT, "(%u,%hu): sending to peer %u, seqnum %u, payload len %hu, total len %hu\n",
                        conn->peer_id, conn->sessionId,
                        conn->peer_id, u32seqnum, ntohs (packet->pppoe_length), buflen);


    if((retval = send_packet_to_peer (conn, p2buffer, buflen)) == 0)
    {
        ++u32seqnum;
    }

    free (p2buffer);

    return retval;
}


int
send_packet_to_ac (PPPoEConnection * conn, PPPoEPacket * packet, UINT16_t proto)
{
    int retval;
    int buflen;
    void *p2buffer = NULL;

    UINT16_t packetlen = ntohs (packet->pppoe_length) + ETH_PPPOE_OVERHEAD;

    p2buffer = malloc (SIZEOF_CTL_FRAME_DATA + packetlen);

    if (p2buffer == NULL)
    {
        LOGGER(LOG_ERR, "(%u,%hu): unable to malloc p2buffer\n",
                           conn->peer_id, conn->sessionId);
        return -1;
    }

    if ((buflen = rfc4938_ctl_format_frame_data (u32seqnum, conn->sessionId,
                  packetlen,
                  proto,
                  packet,
                  p2buffer)) !=
            (int) (SIZEOF_CTL_FRAME_DATA + packetlen))
    {
        LOGGER(LOG_ERR, "(%u,%hu): Unable to format message\n",
                           conn->peer_id, conn->sessionId);
        free (p2buffer);

        return -1;
    }

    LOGGER(LOG_PKT, "(%u,%hu): sending to parent, dst %u, seqnum %u, len %hu, proto 0x%04hx\n",
                        conn->peer_id, conn->sessionId, conn->peer_id,
                        u32seqnum, buflen, proto);

    if((retval = send_packet_to_parent (conn, p2buffer, buflen)) == 0)
    {
        ++u32seqnum;
    }

    free (p2buffer);

    return retval;
}





int
recv_packet_from_parent (PPPoEConnection * conn, PPPoEPacket * packet)
{
    struct sockaddr_in src_addr;
    unsigned char rcv_buffer[MAX_SOCK_MSG_LEN];

    socklen_t len_inet = sizeof (src_addr);

    int msglen = recvfrom (conn->udpIPCSocket,
                           rcv_buffer, sizeof(rcv_buffer), 0, (struct sockaddr *) &src_addr, &len_inet);

    if (msglen < 0)
    {
        LOGGER(LOG_ERR, "(%u,%hu): error:%s\n",
                           conn->peer_id, conn->sessionId, strerror (errno));

        return -1;
    }

    UINT16_t hdrchk = get_word_from_buff(rcv_buffer, 0);

    /* check to see if it is really control */
    if (hdrchk != HDR_PREFIX)
    {
        LOGGER(LOG_ERR, "(%u,%hu): len %d, hdrchk 0x%04hx failed test\n",
                           conn->peer_id, conn->sessionId, msglen, hdrchk);

        return -1;
    }
    else
    {
        int result = pppoe_ctl_packet_parser (rcv_buffer, msglen, &src_addr, packet);

        LOGGER(LOG_PKT, "(%u,%hu): len %d, hdrchk 0x%04hx, parse result %d\n",
                            conn->peer_id, conn->sessionId, msglen, hdrchk, result);

        return result;
    }
}


/***********************************************************************
 * Sends a session_start message to neighbor rfc4938 process
 ***********************************************************************/
int
send_session_start (PPPoEConnection * conn)
{
    int retval;
    int buflen;
    void *p2buffer = NULL;
    UINT16_t credit_scalar;

    p2buffer = malloc (SIZEOF_CTL_START_REQUEST);

    if (p2buffer == NULL)
    {
        LOGGER(LOG_ERR, "(%u,%hu): unable to malloc p2buffer\n",
                           conn->peer_id, conn->sessionId);
        return -1;
    }

    if (conn->mode == MODE_RFC4938_ONLY)
    {
        credit_scalar = 0;
    }
    else
    {
        credit_scalar = conn->local_credit_scalar;
    }

    if ((buflen = rfc4938_ctl_format_session_start (u32seqnum, conn->parent_id,
                  conn->host_id,
                  credit_scalar, p2buffer)) != SIZEOF_CTL_START_REQUEST)
    {
        LOGGER(LOG_ERR, "(%u,%hu): Unable to format message\n",
                           conn->peer_id, conn->sessionId);
        free (p2buffer);

        return -1;
    }

    LOGGER(LOG_PKT, "(%u,%hu): sending to peer %u, seqnum %u, pid %hu, credit scalar %hu\n",
                        conn->peer_id, conn->sessionId, conn->peer_id, u32seqnum, conn->host_id, credit_scalar);

    if((retval = send_packet_to_peer (conn, p2buffer, buflen)) == 0)
    {
        ++u32seqnum;
    }

    free (p2buffer);

    return retval;
}


/***********************************************************************
 * Sends session_start_ready msg.  This message is sent from the pppoe
 * process to its local rfc4938 process to signal the port it succesfully
 * bound to. If it was the first neighbor to be started.
 * The second neighbor will send a session_start_ready message to its
 * rfc4938 process and the first neighbor.
 ***********************************************************************/
int
send_session_start_ready (PPPoEConnection * conn)
{
    int retval;
    int buflen;
    void *p2buffer = NULL;

    p2buffer = malloc (SIZEOF_CTL_START_READY);

    if (p2buffer == NULL)
    {
        LOGGER(LOG_ERR, "(%u,%hu): unable to malloc p2buffer\n",
                           conn->peer_id, conn->sessionId);
        return -1;
    }

    if ((buflen = rfc4938_ctl_format_session_start_ready (u32seqnum, conn->parent_id,
                  conn->host_id,
                  p2buffer)) != SIZEOF_CTL_START_READY)
    {
        LOGGER(LOG_ERR, "(%u,%hu): Unable to format message\n",
                           conn->peer_id, conn->sessionId);
        free (p2buffer);
    }

    LOGGER(LOG_PKT, "(%u,%hu): sending to peer %u, seqnum %u, len %d\n",
                        conn->peer_id, conn->sessionId, conn->peer_id, u32seqnum, buflen);

    if((retval = send_packet_to_peer (conn, p2buffer, buflen)) == 0)
    {
        ++u32seqnum;
    }

    free (p2buffer);

    return retval;
}




int
send_child_ready (PPPoEConnection * conn)
{
    int retval;
    int buflen;
    void *p2buffer = NULL;

    p2buffer = malloc (SIZEOF_CTL_CHILD_READY);

    if (p2buffer == NULL)
    {
        LOGGER(LOG_ERR, "(%u,%hu): unable to malloc p2buffer\n",
                           conn->peer_id, conn->sessionId);
        return -1;
    }

    if ((buflen = rfc4938_ctl_format_child_ready (u32seqnum, conn->peer_id,    // nbr id
                  conn->my_port,    // my port
                  conn->host_id,    // my pid
                  p2buffer)) != SIZEOF_CTL_CHILD_READY)
    {
        LOGGER(LOG_ERR, "(%u,%hu): Unable to format message\n",
                           conn->peer_id, conn->sessionId);
        free (p2buffer);
    }

    LOGGER(LOG_PKT, "(%u,%hu): sending to parent, dst %u, seqnum %u, len %d\n",
                        conn->peer_id, conn->sessionId, conn->parent_id, u32seqnum, buflen);

    if((retval = send_packet_to_parent (conn, p2buffer, buflen)) == 0)
    {
        ++u32seqnum;
    }

    free (p2buffer);

    return retval;
}



int
send_session_up (PPPoEConnection * conn)
{
    int retval;
    int buflen;
    void *p2buffer = NULL;

    p2buffer = malloc (SIZEOF_CTL_CHILD_SESSION_UP);

    if (p2buffer == NULL)
    {
        LOGGER(LOG_ERR, "(%u,%hu): unable to malloc p2buffer\n",
                           conn->peer_id, conn->sessionId);

        return -1;
    }

    if ((buflen = rfc4938_ctl_format_child_session_up (u32seqnum, conn->peer_id, // nbr id
                  conn->sessionId,          // my session id
                  conn->host_id,            // my pid
                  p2buffer)) != SIZEOF_CTL_CHILD_SESSION_UP)
    {
        LOGGER(LOG_ERR, "(%u,%hu): Unable to format message\n",
                           conn->peer_id, conn->sessionId);
        free (p2buffer);
    }

    LOGGER(LOG_PKT, "(%u,%hu): sending to parent, dst %u, seqnum %u, len %d\n",
                        conn->peer_id, conn->sessionId, conn->parent_id, u32seqnum, buflen);

    if((retval = send_packet_to_parent (conn, p2buffer, buflen)) == 0)
    {
        ++u32seqnum;
    }

    free (p2buffer);

    return retval;
}




void
send_child_session_terminated (PPPoEConnection * conn)
{
    int retval;
    void *p2buffer = NULL;
    int buflen;

    p2buffer = malloc (SIZEOF_CTL_CHILD_SESSION_TERMINATED);

    if (p2buffer == NULL)
    {
        LOGGER(LOG_ERR, "(%u,%hu): unable to malloc p2buffer\n", conn->peer_id, conn->sessionId);
        return;
    }

    if ((buflen = rfc4938_ctl_format_child_session_terminated (u32seqnum, conn->peer_id,   // peer id (nbr)
                  conn->sessionId,            // session id
                  p2buffer)) != SIZEOF_CTL_CHILD_SESSION_TERMINATED)
    {
        LOGGER(LOG_ERR, "(%u,%hu): Unable to format message\n",
                           conn->peer_id, conn->sessionId);

        free (p2buffer);
    }

    LOGGER(LOG_PKT, "(%u,%hu): sending to parent, dst %u, seqnum %u, len %d\n",
                        conn->peer_id, conn->sessionId, conn->parent_id, u32seqnum, buflen);

    /* send to parent rfc4938 */
    if((retval = send_packet_to_parent (conn, p2buffer, buflen)) == 0)
    {
        ++u32seqnum;
    }

    free (p2buffer);
}



static int
pppoe_ctl_packet_parser (unsigned char *buff, int msglen, struct sockaddr_in *source_addr, PPPoEPacket * packet)
{
    PPPoEConnection *conn = get_pppoe_conn ();

    if (buff == NULL)
    {
        LOGGER(LOG_ERR, "NULL buff reference\n");

        return -1;
    }

    if (msglen <= 0)
    {
        LOGGER(LOG_ERR, "empty buff \n");

        return -1;
    }

    if (conn == NULL)
    {
        LOGGER(LOG_ERR, "NULL conn reference\n");

        return -1;
    }

    rfc4938_ctl_message_t *p2ctlmsg = (rfc4938_ctl_message_t *) buff;

    LOGGER(LOG_PKT, "(%u,%hu): len %d, seqnum %u, from %s:%hu\n",
                        conn->peer_id, conn->sessionId,
                        msglen, ntohl(p2ctlmsg->header.u32seq_number),
                        inet_ntoa (source_addr->sin_addr), htons (source_addr->sin_port));


    int result = 0;

    switch (p2ctlmsg->header.u8cmd_code)
    {
    case CTL_SESSION_START_READY:
        result = handle_session_start_ready (p2ctlmsg);
        break;

    case CTL_SESSION_STOP:
        result = handle_session_stop (p2ctlmsg);
        break;

    case CTL_PEER_SESSION_DATA:
        result = handle_session_data_from_peer (p2ctlmsg, packet);
        break;

    case CTL_FRAME_DATA:
        result = handle_frame_data (p2ctlmsg, packet);
        break;

    case CTL_SESSION_PADQ:
        result = handle_session_padq (p2ctlmsg);
        break;

    case CTL_SESSION_PADG:
        result = handle_session_padg (p2ctlmsg);
        break;

    default:
        LOGGER(LOG_ERR, "(%u,%hu):  unsupported ctrl command 0x%02hhx \n",
                           conn->peer_id, conn->sessionId, p2ctlmsg->header.u8cmd_code);
    }

    return result;
}



static int
send_packet_to_peer (PPPoEConnection * conn, void *p2buffer, int buflen)
{
    LOGGER(LOG_PKT, "(%u,%hu): sending packet len %d\n",
                        conn->peer_id, conn->sessionId, buflen);

    int z = send_udp_packet (conn, p2buffer, buflen);

    if (z < 0)
    {
        LOGGER(LOG_ERR, "(%u,%hu): error sending packet len %d:%s:\n",
                           conn->peer_id, conn->sessionId, buflen, strerror (errno));

        return -1;
    }

    return 0;
}


static int
send_packet_to_parent (PPPoEConnection * conn, void *p2buffer, int buflen)
{
    int z = send_udp_packet (conn, p2buffer, buflen);

    if (z < 0)
    {
        LOGGER(LOG_ERR, "(%u,%hu): error sending packet len %d:%s:\n",
                           conn->peer_id, conn->sessionId, buflen, strerror (errno));

        return -1;
    }

    return 0;
}



static int
send_udp_packet (PPPoEConnection * conn, void *p2buffer, int buflen)
{
    struct sockaddr_in dst_addr;
    socklen_t len_inet;
    int z;

    if (conn == NULL)
    {
        LOGGER(LOG_ERR, "NULL connection \n");

        return -1;
    }

    if (p2buffer == NULL)
    {
        LOGGER(LOG_ERR, "NULL p2buffer \n");

        return -1;
    }

    if (buflen <= 0)
    {
        LOGGER(LOG_ERR, "empty buffer \n");

        return -1;
    }

    memset (&dst_addr, 0, sizeof (dst_addr));
    dst_addr.sin_family = AF_INET;
    dst_addr.sin_port = htons (conn->parent_port);
    dst_addr.sin_addr.s_addr = inet_addr (LOCALHOST);

    len_inet = sizeof (dst_addr);

    z = sendto (conn->udpIPCSocket, p2buffer, buflen, 0, (struct sockaddr *) &dst_addr, len_inet);

    if (z < 0)
    {
        LOGGER(LOG_ERR, "(%u,%hu): error:%s\n",
                           conn->peer_id, conn->sessionId, strerror (errno));
    }
    else
    {
        LOGGER(LOG_INFO, "(%u,%hu): %s:%hu\n",
                           conn->peer_id, conn->sessionId,
                           inet_ntoa(dst_addr.sin_addr), conn->parent_port);


    }

    return z;
}





static int
handle_session_stop (rfc4938_ctl_message_t * p2ctlmsg __attribute__((unused)))
{
    PPPoEConnection *conn = get_pppoe_conn ();

    if (conn == NULL)
    {
        LOGGER(LOG_ERR, "NULL conn reference \n");

        return -1;
    }

    LOGGER(LOG_INFO, "(%u,%hu):\n", conn->peer_id, conn->sessionId);

    sendPADTandExit (conn, "Received session_stop message from parent", 0);

    return 0;
}



static int
handle_session_padq (rfc4938_ctl_message_t * p2ctlmsg)
{
    PPPoEConnection *conn = get_pppoe_conn ();

    if (p2ctlmsg == NULL)
    {
        return -1;
    }
    if (conn == NULL)
    {
        LOGGER(LOG_ERR, "NULL conn reference\n");

        return -1;
    }

    /* byte */
    UINT8_t receive_only = p2ctlmsg->ctl_padq_payload.u8receive_only;
    UINT8_t rlq          = p2ctlmsg->ctl_padq_payload.u8rlq;
    UINT8_t resources    = p2ctlmsg->ctl_padq_payload.u8resources;

    /* short */
    UINT16_t cdr_scale = ntohs (p2ctlmsg->ctl_padq_payload.u16cdr_scale);
    UINT16_t mdr_scale = ntohs (p2ctlmsg->ctl_padq_payload.u16mdr_scale);
    UINT16_t latency   = ntohs (p2ctlmsg->ctl_padq_payload.u16latency);
    UINT16_t cdr       = ntohs (p2ctlmsg->ctl_padq_payload.u16current_data_rate);
    UINT16_t mdr       = ntohs (p2ctlmsg->ctl_padq_payload.u16max_data_rate);

    LOGGER(LOG_INFO, "(%u,%hu): ro: %hhu, rlq: %hhu, res: %hhu, latency: %hu, cdr_scale: %hu, "
                       "cdr: %hu, mdr_scale: %hu, mdr: %hu\n",
                       conn->peer_id, conn->sessionId,
                       receive_only, rlq, resources, latency, cdr_scale, cdr, mdr_scale, mdr);

    sendPADQ (conn, mdr, mdr_scale, cdr, cdr_scale, latency, resources, rlq, receive_only);

    return 0;
}




static int
handle_session_padg (rfc4938_ctl_message_t * p2ctlmsg)
{
    PPPoEConnection *conn = get_pppoe_conn ();

    if (p2ctlmsg == NULL)
    {
        return -1;
    }
    if (conn == NULL)
    {
        LOGGER(LOG_ERR, "NULL conn reference\n");

        return -1;
    }

    /* get credits */
    UINT16_t req_credits = ntohs (p2ctlmsg->ctl_padg_payload.u16credits);

    UINT16_t allowed_credits = req_credits;

    if((req_credits + conn->peer_credits) > conn->grant_limit)
    {
        if(conn->grant_limit < conn->peer_credits)
        {
            LOGGER(LOG_PKT, "(%u,%hu): req_credits %hu, peer_credits %hu, already at limit, no sending PADG\n",
                                conn->peer_id, conn->sessionId, req_credits, conn->peer_credits);

            return 0;
        }
        else
        {
            allowed_credits = conn->grant_limit - conn->peer_credits;
        }
    }

    LOGGER(LOG_PKT, "(%u,%hu): req_credits %hu, peer_credits %hu, allowed_credits %hu\n",
                        conn->peer_id, conn->sessionId, req_credits, conn->peer_credits, allowed_credits);

    /* check grant enable mode */
    if (conn->timed_credits)
    {
        conn->grant_limit = allowed_credits;

        LOGGER(LOG_PKT, "(%u,%hu): 1-second grant amount updated by %hu, to %hu\n",
                            conn->peer_id, conn->sessionId, allowed_credits, conn->grant_limit);
    }
    else
    {
        LOGGER(LOG_PKT, "(%u,%hu): send out of band grant %hu\n",
                            conn->peer_id, conn->sessionId, allowed_credits);

        sendOutOfBandGrant(conn, allowed_credits);
    }

    return 0;
}


static int
handle_session_start_ready (rfc4938_ctl_message_t * p2ctlmsg)
{
    PPPoEConnection *conn = get_pppoe_conn ();

    if (p2ctlmsg == NULL)
    {
        return -1;
    }
    if (conn == NULL)
    {
        LOGGER(LOG_ERR, "NULL conn reference\n");

        return -1;
    }

    conn->peer_pid = ntohl (p2ctlmsg->ctl_start_ready_payload.u32pid);

    LOGGER(LOG_INFO, "(%u,%hu): peer id %u, pid set to %u\n",
                       conn->peer_id, conn->sessionId, conn->peer_id, conn->peer_pid);

    return 0;
}


static int
handle_session_data_from_peer (rfc4938_ctl_message_t * p2ctlmsg, PPPoEPacket * packet)
{
    PPPoEConnection *conn = get_pppoe_conn ();

    if (p2ctlmsg == NULL)
    {
        return -1;
    }

    if (conn == NULL)
    {
        LOGGER(LOG_ERR, "NULL conn reference\n");

        return -1;
    }

    if (conn->sessionId == 0)
    {
        LOGGER(LOG_ERR, "session id has not been established, drop msg\n");

        return 0;
    }

    UINT16_t len = ntohs (p2ctlmsg->ctl_session_data_payload.u16data_len);

    LOGGER(LOG_PKT, "(%u,%hu): len %hu\n", conn->peer_id, conn->sessionId, len);

    memcpy(packet->payload, p2ctlmsg->ctl_session_data_payload.data, len);

    packet->pppoe_length = htons (len);

    if(conn->peer_magic == 0)
    {
        check_for_remote_magic_number(conn, packet);
    }

    consume_credits_and_send_frame_to_ac (conn, packet);

    return 0;
}



static int
handle_frame_data (rfc4938_ctl_message_t * p2ctlmsg, PPPoEPacket * packet)
{
    PPPoEConnection *conn = get_pppoe_conn ();

    if (p2ctlmsg == NULL)
    {
        return -1;
    }
    if (conn == NULL)
    {
        LOGGER(LOG_ERR, "NULL conn reference\n");

        return -1;
    }

    UINT16_t len   = ntohs (p2ctlmsg->ctl_frame_data_payload.u16data_len);
    UINT16_t proto = ntohs (p2ctlmsg->ctl_frame_data_payload.u16proto);

    if(proto == ETH_PPPOE_DISCOVERY)
    {
        LOGGER(LOG_PKT, "(%u,%hu): recv %d byte discovery msg\n",
                            conn->peer_id, conn->sessionId, len);

        memcpy(packet, p2ctlmsg->ctl_frame_data_payload.data, len);

        return len;
    }
    else if (proto == ETH_PPPOE_SESSION)
    {
        if(conn->sessionId == 0)
        {
            LOGGER(LOG_ERR, "(%u,%hu): session not yet established with server, drop msg len %d\n",
                               conn->peer_id, conn->sessionId, len);

            return 0;
        }

        LOGGER(LOG_PKT, "(%u,%hu): recv %d byte session msg\n",
                            conn->peer_id, conn->sessionId, len);

        return handle_session_frame_from_ac (conn, (PPPoEPacket *) p2ctlmsg->ctl_frame_data_payload.data, len);
    }
    else
    {
        LOGGER(LOG_ERR, "(%u,%hu): Unsupported proto 0x%04hx\n",
                           conn->peer_id, conn->sessionId, proto);

        return -1;
    }
}



static int handle_ppp_packet_bc_mode(PPPoEConnection *conn, PPPoEPacket *packet)
{
    PPPHeader * p2ppp = (PPPHeader *) packet->payload;

    if(ntohs(p2ppp->type) == PPP_LCP)
    {
        switch(p2ppp->code)
        {
        case PPP_ECHO_REQ:
            return handle_lcp_echo_req(conn, packet, p2ppp);

        case PPP_CONFIG_REQ:
            return handle_lcp_config_req(conn, packet, p2ppp);

        case PPP_CONFIG_ACK:
            return handle_lcp_config_ack(conn, packet, p2ppp);

        case PPP_CONFIG_NAK:
            return handle_lcp_config_nack(conn, packet, p2ppp);

        case PPP_CONFIG_REJECT:
            return handle_lcp_config_reject(conn, packet, p2ppp);

        case PPP_TERMINATE_REQ:
            return handle_lcp_terminate_req(conn, packet, p2ppp);

        case PPP_TERMINATE_ACK:
            return handle_lcp_terminate_ack(conn, packet, p2ppp);

        case PPP_CODE_REJECT:
            return handle_lcp_code_reject(conn, packet, p2ppp);
        }
    }
    else if(ntohs(p2ppp->type) == PPP_IPCP)
    {
        switch(p2ppp->code)
        {
        case PPP_CONFIG_REQ:
            return handle_ipcp_config_req(conn, packet, p2ppp);

        case PPP_CONFIG_ACK:
            return handle_ipcp_config_ack(conn, packet, p2ppp);

        case PPP_CONFIG_NAK:
            return handle_ipcp_config_nack(conn, packet, p2ppp);

        case PPP_CONFIG_REJECT:
            return handle_ipcp_config_reject(conn, packet, p2ppp);

        case PPP_TERMINATE_REQ:
            return handle_ipcp_terminate_req(conn, packet, p2ppp);

        case PPP_TERMINATE_ACK:
            return handle_ipcp_terminate_ack(conn, packet, p2ppp);

        case PPP_CODE_REJECT:
            return handle_ipcp_code_reject(conn, packet, p2ppp);
        }
    }

    return 1;
}

static int check_for_lcp_echo_request(PPPoEConnection *conn, PPPoEPacket *packet)
{
    PPPHeader * p2ppp = (PPPHeader *) packet->payload;

    if(ntohs(p2ppp->type) == PPP_LCP)
    {
        switch(p2ppp->code)
        {
        case PPP_ECHO_REQ:
            return handle_lcp_echo_req(conn, packet, p2ppp);
        }
    }

    return 1;
}



static void check_for_remote_magic_number(PPPoEConnection *conn, PPPoEPacket *packet)
{
    PPPHeader * p2ppp = (PPPHeader *) packet->payload;

    if(ntohs(p2ppp->type) == PPP_LCP)
    {
        if(p2ppp->code == PPP_CONFIG_REQ)
        {
            UINT32_t * p2magic = get_lcp_config_magic_number(conn, p2ppp);

            if(p2magic != NULL)
            {
                LOGGER(LOG_PKT, "(%u,%hu): acquired peer magic number 0x%08x\n",
                                    conn->peer_id, conn->sessionId, ntohl(*p2magic));

                conn->peer_magic = ntohl(*p2magic);
            }
        }
    }
}


static void check_for_local_magic_number(PPPoEConnection *conn, PPPoEPacket *packet)
{
    PPPHeader * p2ppp = (PPPHeader *) packet->payload;

    if(ntohs(p2ppp->type) == PPP_LCP)
    {
        if(p2ppp->code == PPP_CONFIG_REQ)
        {
            UINT32_t * p2magic = get_lcp_config_magic_number(conn, p2ppp);

            if(p2magic != NULL)
            {
                LOGGER(LOG_PKT, "(%u,%hu): acquired local magic number 0x%08x\n",
                                    conn->peer_id, conn->sessionId, ntohl(*p2magic));

                conn->local_magic = ntohl(*p2magic);
            }
        }
    }
}



static int handle_lcp_echo_req(PPPoEConnection *conn, PPPoEPacket *packet, PPPHeader *p2ppp)
{
    UINT32_t * p2magic = (UINT32_t *) p2ppp->data;

    LOGGER(LOG_PKT, "(%u,%hu): len %hu, session %hu, code %hhu, id %hhu, opt(s) length %hu, magic 0x%08x, (downstream)\n",
                        conn->peer_id, conn->sessionId,
                        ntohs(packet->pppoe_length),
                        ntohs(packet->pppoe_session),
                        p2ppp->code, p2ppp->id, ntohs (p2ppp->length), ntohl(*p2magic));

    if(conn->enable_lcp_echo_reply)
    {
        if(conn->peer_magic != 0)
        {
            // swap eth dst/src addr
            SWAP_PACKET_DST_SRC(packet);

            p2ppp->code = PPP_ECHO_REPLY;

            LOGGER(LOG_PKT, "(%u,%hu): set magic number from 0x%08x to 0x%08x, bounce reply back\n",
                                conn->peer_id, conn->sessionId, ntohl(*p2magic), conn->peer_magic);

            *p2magic = htonl(conn->peer_magic);

            send_session_packet_to_ac(conn, packet);

            return 0;
        }
        else
        {
            LOGGER(LOG_PKT, "(%u,%hu): peer magic number is not known, let it thru\n",
                                conn->peer_id, conn->sessionId);

            return 1;
        }
    }
    else
    {
        LOGGER(LOG_PKT, "(%u,%hu): lcp echo pong disbled,  let it thru\n",
                            conn->peer_id, conn->sessionId);

        return 1;
    }
}


static UINT32_t * get_lcp_config_magic_number(PPPoEConnection *conn, PPPHeader *p2ppp)
{
    LOGGER(LOG_PKT, "(%u,%hu): code %hhu, id %hhu, opt(s) length %hu\n",
                        conn->peer_id, conn->sessionId,
                        p2ppp->code, p2ppp->id, ntohs (p2ppp->length));

    UINT8_t * p = (UINT8_t *) p2ppp->data;

    while(p < (p2ppp->data + ntohs(p2ppp->length) - 4))
    {
        PPPOption * p2opt = (PPPOption *) p;

        LOGGER(LOG_PKT, "(%u,%hu): option %hhu, option length %hhu\n",
                            conn->peer_id, conn->sessionId, p2opt->opt, p2opt->length);

        if(p2opt->opt == LCP_MAGIC_NUMBER)
        {
            UINT32_t * p2magic = (UINT32_t *) p2opt->data;

            LOGGER(LOG_PKT, "(%u,%hu): lcp magic number 0x%08x\n",
                                conn->peer_id, conn->sessionId, ntohl(*p2magic));

            return p2magic;
        }
        else
        {
            p += p2opt->length;
        }
    }

    return NULL;
}


static int handle_lcp_config_req(PPPoEConnection *conn, PPPoEPacket *packet, PPPHeader *p2ppp)
{
    PPPOption * p2opt = (PPPOption *) p2ppp->data;

    LOGGER(LOG_PKT, "(%u,%hu): total len %hu, code %hhu, id %hhu, opt(s) length %hu, opt %hhu, len %hhu\n",
                        conn->peer_id, conn->sessionId,
                        ntohs(packet->pppoe_length),
                        p2ppp->code, p2ppp->id, ntohs (p2ppp->length), p2opt->opt, p2opt->length);

    // swap eth dst/src addr
    SWAP_PACKET_DST_SRC(packet);

    p2ppp->code = PPP_CONFIG_ACK;

    send_session_packet_to_ac(conn, packet);

    UINT32_t *p2magic = get_lcp_config_magic_number(conn, p2ppp);

    if(p2magic != NULL)
    {
        LOGGER(LOG_PKT, "(%u,%hu): magic number 0x%08x\n",
                            conn->peer_id, conn->sessionId, ntohl(*p2magic));

        *p2magic += htonl(1);
    }

    p2ppp->code = PPP_CONFIG_REQ;

    send_session_packet_to_ac(conn, packet);

    return 0;
}


static int handle_lcp_config_ack(PPPoEConnection *conn, PPPoEPacket *packet, PPPHeader *p2ppp)
{
    LOGGER(LOG_PKT, "(%u,%hu): total len %hu, code %hhu, id %hhu, opt(s) length %hu, consume\n",
                        conn->peer_id, conn->sessionId,
                        ntohs(packet->pppoe_length),
                        p2ppp->code, p2ppp->id, ntohs (p2ppp->length));

    return 0;
}


static int handle_lcp_config_nack(PPPoEConnection *conn, PPPoEPacket *packet, PPPHeader *p2ppp)
{
    LOGGER(LOG_PKT, "(%u,%hu): total len %hu, code %hhu, id %hhu, opt(s) length %hu, consume\n",
                        conn->peer_id, conn->sessionId,
                        ntohs(packet->pppoe_length),
                        p2ppp->code, p2ppp->id, ntohs (p2ppp->length));

    return 0;
}


static int handle_lcp_config_reject(PPPoEConnection *conn, PPPoEPacket *packet, PPPHeader *p2ppp)
{
    LOGGER(LOG_PKT, "(%u,%hu): total len %hu, code %hhu, id %hhu, opt(s) length %hu\n, consume",
                        conn->peer_id, conn->sessionId,
                        ntohs(packet->pppoe_length),
                        p2ppp->code, p2ppp->id, ntohs (p2ppp->length));

    return 0;
}


static int handle_lcp_terminate_req(PPPoEConnection *conn, PPPoEPacket *packet, PPPHeader *p2ppp)
{
    LOGGER(LOG_PKT, "(%u,%hu): total len %hu, code %hhu, id %hhu, opt(s) length %hu\n",
                        conn->peer_id, conn->sessionId,
                        ntohs(packet->pppoe_length),
                        p2ppp->code, p2ppp->id, ntohs (p2ppp->length));

    SWAP_PACKET_DST_SRC(packet);

    p2ppp->code = PPP_TERMINATE_ACK;

    send_session_packet_to_ac(conn, packet);

    return 0;
}


static int handle_lcp_terminate_ack(PPPoEConnection *conn, PPPoEPacket *packet, PPPHeader *p2ppp)
{
    LOGGER(LOG_PKT, "(%u,%hu): total len %hu, code %hhu, id %hhu, opt(s) length %hu, consume\n",
                        conn->peer_id, conn->sessionId,
                        ntohs(packet->pppoe_length),
                        p2ppp->code, p2ppp->id, ntohs (p2ppp->length));

    return 0;
}


static int handle_lcp_code_reject(PPPoEConnection *conn, PPPoEPacket *packet, PPPHeader *p2ppp)
{
    LOGGER(LOG_PKT, "(%u,%hu): total len %hu, code %hhu, id %hhu, opt(s) length %hu, consume\n",
                        conn->peer_id, conn->sessionId,
                        ntohs(packet->pppoe_length),
                        p2ppp->code, p2ppp->id, ntohs (p2ppp->length));

    return 0;
}


static int handle_ipcp_config_req(PPPoEConnection *conn, PPPoEPacket *packet, PPPHeader *p2ppp)
{
    PPPOption * p2opt = (PPPOption *) p2ppp->data;

    LOGGER(LOG_PKT, "(%u,%hu): total len %hu, code %hhu, id %hhu, opt(s) length %hu, opt %hhu, len %hhu\n",
                        conn->peer_id, conn->sessionId,
                        ntohs(packet->pppoe_length),
                        p2ppp->code, p2ppp->id, ntohs (p2ppp->length), p2opt->opt, p2opt->length);

    // swap src dst
    SWAP_PACKET_DST_SRC(packet);

    p2ppp->code = PPP_CONFIG_ACK;

    send_session_packet_to_ac(conn, packet);

    if(p2opt->opt == PPP_IPCP_OPT_IP_ADDRESS)
    {
        struct in_addr * p2addr = (struct in_addr *) p2opt->data;

        LOGGER(LOG_PKT, "(%u,%hu): address %s\n",
                            conn->peer_id, conn->sessionId, inet_ntoa(*p2addr));

        p2addr->s_addr = p2addr->s_addr | htonl(255);
    }

    p2ppp->code = PPP_CONFIG_REQ;

    send_session_packet_to_ac(conn, packet);

    return 0;
}


static int handle_ipcp_config_ack(PPPoEConnection *conn, PPPoEPacket *packet, PPPHeader *p2ppp)
{
    LOGGER(LOG_PKT, "(%u,%hu): total len %hu, code %hhu, id %hhu, opt(s) length %hu, consume\n",
                        conn->peer_id, conn->sessionId,
                        ntohs(packet->pppoe_length),
                        p2ppp->code, p2ppp->id, ntohs (p2ppp->length));

    return 0;
}


static int handle_ipcp_config_nack(PPPoEConnection *conn, PPPoEPacket *packet, PPPHeader *p2ppp)
{
    LOGGER(LOG_PKT, "(%u,%hu): total len %hu, code %hhu, id %hhu, opt(s) length %hu, consume\n",
                        conn->peer_id, conn->sessionId,
                        ntohs(packet->pppoe_length),
                        p2ppp->code, p2ppp->id, ntohs (p2ppp->length));

    return 0;
}


static int handle_ipcp_config_reject(PPPoEConnection *conn, PPPoEPacket *packet, PPPHeader *p2ppp)
{
    LOGGER(LOG_PKT, "(%u,%hu): total len %hu, code %hhu, id %hhu, opt(s) length %hu, consume\n",
                        conn->peer_id, conn->sessionId,
                        ntohs(packet->pppoe_length),
                        p2ppp->code, p2ppp->id, ntohs (p2ppp->length));

    return 0;
}


static int handle_ipcp_terminate_req(PPPoEConnection *conn, PPPoEPacket *packet, PPPHeader *p2ppp)
{
    LOGGER(LOG_PKT, "(%u,%hu): total len %hu, code %hhu, id %hhu, opt(s) length %hu\n",
                        conn->peer_id, conn->sessionId,
                        ntohs(packet->pppoe_length),
                        p2ppp->code, p2ppp->id, ntohs (p2ppp->length));

    SWAP_PACKET_DST_SRC(packet);

    p2ppp->code = PPP_TERMINATE_ACK;

    send_session_packet_to_ac(conn, packet);

    return 0;
}


static int handle_ipcp_terminate_ack(PPPoEConnection *conn, PPPoEPacket *packet, PPPHeader *p2ppp)
{
    LOGGER(LOG_PKT, "(%u,%hu): total len %hu, code %hhu, id %hhu, opt(s) length %hu, consume\n",
                        conn->peer_id, conn->sessionId,
                        ntohs(packet->pppoe_length),
                        p2ppp->code, p2ppp->id, ntohs (p2ppp->length));

    return 0;
}


static int handle_ipcp_code_reject(PPPoEConnection *conn, PPPoEPacket *packet, PPPHeader *p2ppp)
{
    LOGGER(LOG_PKT, "(%u,%hu): total len %hu, code %hhu, id %hhu, opt(s) length %hu, consume\n",
                        conn->peer_id, conn->sessionId,
                        ntohs(packet->pppoe_length),
                        p2ppp->code, p2ppp->id, ntohs (p2ppp->length));

    return 0;
}
