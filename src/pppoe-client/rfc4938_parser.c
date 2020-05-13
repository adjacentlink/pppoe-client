/*-----------------------------------------------------------------------------/
 * project: rfc4938
 * file: rfc4938.c
 * version: 1.05
 * date: October 4, 2007
 *
 * Copyright (c) 2013 - Adjacent Link, LLC, Bridgewater NJ
 * Copyright owner (c) 2007-2009 by cisco Systems, Inc.
 *
 * ===========================
 * This file implements the rfc4938 controlling program which communicates
 * with other rfc4938 processes and accepts user input from the rfc4938ctl
 * process.  This file also fork/execs pppoe processes corresponding to new
 * neighbors.
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

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include "rfc4938_parser.h"
#include "rfc4938_neighbor_manager.h"
#include "rfc4938_io.h"
#include "rfc4938_messages.h"
#include "rfc4938_transport.h"
#include "rfc4938_config.h"
#include "rfc4938_debug.h"

#define SHOWLEN ( LNLEN * (rfc4938_config_get_max_nbrs() + 1) )

UINT32_t u32seqnum = 0;

static void rfc4938_parser_parse_packet_i (const void *recvbuf, int bufsize, rfc4938_neighbor_element_t * nbr, int dir);
static void rfc4938_parser_ctl_recv_peer_session_data_i(UINT32_t neighbor_id, const void *buff, const int bufsize);

static void rfc4938_parser_ctl_recv_child_session_data_i(UINT32_t neighbor_id, const UINT16_t credits,
        const void *buff, const int bufsize);

static void rfc4938_parser_ctl_recv_session_start_i (UINT32_t neighbor_id, UINT32_t pid, UINT16_t scalar);
static void rfc4938_parser_ctl_recv_child_ready_i(UINT32_t neighbor_id, UINT16_t port, UINT32_t pid);
static void rfc4938_parser_ctl_recv_child_session_up_i(UINT32_t neighbor_id, UINT16_t session_id, UINT32_t pid);
static void rfc4938_parser_ctl_recv_child_session_terminated_i(UINT32_t neighbor_id, UINT16_t session_id);
static void rfc4938_parser_ctl_recv_peer_session_terminated_i(UINT32_t neighbor_id);

static int rfc4938_parser_cli_terminate_session_i (UINT32_t neighbor_id, UINT16_t cmdSRC);
static int rfc4938_parser_cli_initiate_session_i (UINT32_t neighbor_id, UINT16_t credit_scalar);
static int rfc4938_parser_cli_padq_session_i (UINT32_t neighbor_id,
        UINT8_t receive_only,
        UINT8_t rlq,
        UINT8_t resources,
        UINT16_t latency,
        UINT16_t cdr_scale,
        UINT16_t current_data_rate,
        UINT16_t mdr_scale,
        UINT16_t max_data_rate);

static int rfc4938_parser_cli_padg_session_i (UINT32_t neighbor_id, UINT16_t credits);
static int rfc4938_parser_cli_show_session_i (void);

const char * cmd_code_to_string(UINT8_t code)
{
    switch(code)
    {
    case CTL_SESSION_START:
        return "SESSION_START";

    case CTL_SESSION_START_READY:
        return "SESSION_START_READY";

    case CTL_CHILD_READY:
        return "CHILD_READY";

    case CTL_CHILD_SESSION_UP:
        return "CHILD_SESSION_UP";

    case CTL_CHILD_SESSION_TERMINATED:
        return "CHILD_SESSION_TERMINATED";

    case CTL_CHILD_SESSION_DATA:
        return "CHILD_SESSION_DATA";

    case CTL_PEER_SESSION_TERMINATED:
        return "PEER_SESSION_TERMINATED";

    case CTL_PEER_SESSION_DATA:
        return "PEER_SESSION_DATA";

    case CTL_SESSION_STOP:
        return "SESSION_STOP";

    case CTL_SESSION_PADQ:
        return "SESSION_PADQ";

    case CTL_SESSION_PADG:
        return "SESSION_PADG";

    case CTL_FRAME_DATA:
        return "FRAME_DATA";

    case CLI_SESSION_INITIATE:
        return "SESSION_INITIATE";

    case CLI_SESSION_TERMINATE:
        return "SESSION_TERMINATE";

    case CLI_SESSION_PADQ:
        return "SESSION_PADQ";

    case CLI_SESSION_PADG:
        return "SESSION_PADG";

    case CLI_SESSION_SHOW:
        return "SESSION_SHOW";

    case CLI_SESSION_SHOW_RESPONSE:
        return "SESSION_SHOW_RESPONSE";

    default:
        return "UNKNOWN";
    }
}

const char * ppp_lcp_code_to_string(UINT16_t code)
{
    switch(code)
    {
    case PPP_CONFIG_REQ:
        return "CONFIG_REQ";

    case PPP_CONFIG_ACK:
        return "CONFIG_ACK";

    case PPP_CONFIG_NAK:
        return "CONFIG_NAK";

    case PPP_CONFIG_REJECT:
        return "CONFIG_REJECT";

    case PPP_TERMINATE_REQ:
        return "TERMINATE_REQ";

    case PPP_TERMINATE_ACK:
        return "TERMINATE_ACK";

    case PPP_CODE_REJECT:
        return "CODE_REJECT";

    case PPP_ECHO_REQ:
        return "ECHO_REQ";

    case PPP_ECHO_REPLY:
        return "ECHO_REPLY";

    default:
        return "UNKNOWN";
    }
}


void
rfc4938_parser_parse_rx_ota_packet (const void *buff, int bufsize, UINT32_t neighbor_id)
{
    rfc4938_neighbor_element_t *nbr;

    if (rfc4938_neighbor_pointer_by_nbr_id (neighbor_id, &nbr) != 0)
    {
        LOGGER(LOG_INFO,"(%u): error, unable to find neighbor_id %u\n",
                        rfc4938_config_get_node_id (), neighbor_id);

        return;
    }

    LOGGER(LOG_INFO, "(%u): len %u, src %u\n",
                         rfc4938_config_get_node_id (), bufsize, neighbor_id);

    rfc4938_parser_parse_packet_i (buff, bufsize, nbr, 0);
}


void
rfc4938_parser_parse_child_packet (const void *buff, int bufsize, rfc4938_neighbor_element_t *nbr)
{
    LOGGER(LOG_INFO, "(%u): len %u, nbr %u\n",
                         rfc4938_config_get_node_id (), bufsize, nbr->neighbor_id);

    rfc4938_parser_parse_packet_i (buff, bufsize, nbr, 1);
}


int
rfc4938_parser_cli_initiate_session (UINT32_t neighbor_id, UINT16_t credit_scalar)
{
    int result = rfc4938_parser_cli_initiate_session_i (neighbor_id, credit_scalar);

    return result;
}


static void
rfc4938_parser_parse_packet_i(const void *buff, int bufsize, rfc4938_neighbor_element_t *nbr, int dir)
{
    rfc4938_ctl_message_t *p2ctlmsg;

    if (buff == NULL)
    {
        LOGGER(LOG_ERR,"(%u): error, NULL buff\n",
                             rfc4938_config_get_node_id ());

        return;
    }

    if (bufsize < (int) sizeof (rfc4938_ctl_message_header_t))
    {
        LOGGER(LOG_ERR,"(%u): error, msg too short (%d < %zd)\n",
                        rfc4938_config_get_node_id (), bufsize,
                        sizeof (rfc4938_ctl_message_header_t));
        return;
    }

    p2ctlmsg = (rfc4938_ctl_message_t *) buff;

    UINT32_t seq = ntohl(p2ctlmsg->header.u32seq_number);

    LOGGER(LOG_PKT,"(%u): recv msg %s, len %d, seqnum %u, child_pid %u\n",
                     rfc4938_config_get_node_id (),
                     cmd_code_to_string(p2ctlmsg->header.u8cmd_code),
                     bufsize, seq, nbr->child_pid);

    if(dir == 1)
    {
        if((seq != nbr->last_seqnum + 1) && nbr->child_port)
        {
            nbr->missed_seqnum += 1;

            LOGGER(LOG_PKT,"(%u): missed seqnum %u, last %u, total_missed %u, child_pid %u\n",
                             rfc4938_config_get_node_id (),
                             seq, nbr->last_seqnum, nbr->missed_seqnum, nbr->child_pid);
        }

        nbr->last_seqnum = seq;
    }


    switch (p2ctlmsg->header.u8cmd_code)
    {
    /* CTL */
    /* from a child */
    case CTL_SESSION_START:
        rfc4938_parser_ctl_recv_session_start_i(ntohl(p2ctlmsg->ctl_start_payload.u32neighbor_id),
                                                ntohl(p2ctlmsg->ctl_start_payload.u32pid),
                                                ntohs(p2ctlmsg->ctl_start_payload.u16credit_scalar));
        break;

    /* from peer to our child */
    case CTL_SESSION_START_READY:
        rfc4938_io_forward_to_child(rfc4938_config_get_id(ntohl(p2ctlmsg->ctl_start_ready_payload.u32neighbor_id)), 
                                    buff,
                                    bufsize,
                                    nbr);
        break;

    /* from our child */
    case CTL_CHILD_READY:
        rfc4938_parser_ctl_recv_child_ready_i(ntohl(p2ctlmsg->ctl_child_ready_payload.u32neighbor_id),
                                              ntohs(p2ctlmsg->ctl_child_ready_payload.u16port_number),
                                              ntohl(p2ctlmsg->ctl_child_ready_payload.u32pid));
        break;

    /* from our child */
    case CTL_CHILD_SESSION_UP:
        rfc4938_parser_ctl_recv_child_session_up_i(ntohl(p2ctlmsg->ctl_child_session_up_payload.u32neighbor_id),
                                                   ntohs(p2ctlmsg->ctl_child_session_up_payload.u16session_id),
                                                   ntohl(p2ctlmsg->ctl_child_session_up_payload.u32pid));
        break;

    /* from our child */
    case CTL_CHILD_SESSION_TERMINATED:
        rfc4938_parser_ctl_recv_child_session_terminated_i(ntohl(p2ctlmsg->ctl_child_session_terminated_payload.u32neighbor_id),
                                                           ntohs(p2ctlmsg->ctl_child_session_terminated_payload.u16session_id));

        rfc4938_io_send_frame_to_device(p2ctlmsg->ctl_frame_data_payload.data,
                                        ntohs(p2ctlmsg->ctl_frame_data_payload.u16data_len),
                                        ntohs(p2ctlmsg->ctl_frame_data_payload.u16proto));

        break;

    /* from our peer */
    case CTL_PEER_SESSION_TERMINATED:
        rfc4938_parser_ctl_recv_peer_session_terminated_i(
            rfc4938_config_get_id(ntohl(p2ctlmsg->ctl_peer_session_terminated_payload.u32neighbor_id)));
        break;

    /* from peer to our child */
    case CTL_PEER_SESSION_DATA:
        rfc4938_parser_ctl_recv_peer_session_data_i(
            rfc4938_config_get_id(ntohl(p2ctlmsg->ctl_session_data_payload.u32neighbor_id)), buff, bufsize);

        break;

    /* from child to peer */
    case CTL_CHILD_SESSION_DATA:
        rfc4938_parser_ctl_recv_child_session_data_i(ntohl(p2ctlmsg->ctl_session_data_payload.u32neighbor_id),
                                                     ntohs(p2ctlmsg->ctl_session_data_payload.u16credits),
                                                     buff,
                                                     bufsize);
        break;

    /* from our child */
    case CTL_FRAME_DATA:
        rfc4938_io_send_frame_to_device (p2ctlmsg->ctl_frame_data_payload.data,
                                         ntohs(p2ctlmsg->ctl_frame_data_payload.u16data_len),
                                         ntohs(p2ctlmsg->ctl_frame_data_payload.u16proto));
        break;

    /* CLI */
    case CLI_SESSION_INITIATE:
        rfc4938_parser_cli_initiate_session_i(ntohl(p2ctlmsg->cli_initiate_payload.u32neighbor_id),
                                              ntohs(p2ctlmsg->cli_initiate_payload.u16credit_scalar));
        break;

    case CLI_SESSION_TERMINATE:
        rfc4938_parser_cli_terminate_session_i(ntohl(p2ctlmsg->cli_terminate_payload.u32neighbor_id), CMD_SRC_CLI);
        break;

    case CLI_SESSION_PADQ:
        rfc4938_parser_cli_padq_session_i(ntohl(p2ctlmsg->cli_padq_payload.u32neighbor_id),
                                          p2ctlmsg->cli_padq_payload.u8receive_only,
                                          p2ctlmsg->cli_padq_payload.u8rlq,
                                          p2ctlmsg->cli_padq_payload.u8resources,
                                          ntohs (p2ctlmsg->cli_padq_payload.u16latency),
                                          ntohs (p2ctlmsg->cli_padq_payload.u16cdr_scale),
                                          ntohs (p2ctlmsg->cli_padq_payload.u16current_data_rate),
                                          ntohs (p2ctlmsg->cli_padq_payload.u16mdr_scale),
                                          ntohs (p2ctlmsg->cli_padq_payload.u16max_data_rate));
        break;

    case CLI_SESSION_PADG:
        rfc4938_parser_cli_padg_session_i(ntohl(p2ctlmsg->cli_padg_payload.u32neighbor_id),
                                          ntohs(p2ctlmsg->cli_padg_payload.u16credits));
        break;

    case CLI_SESSION_SHOW:
        rfc4938_parser_cli_show_session_i ();
        break;

    case CLI_SESSION_SHOW_RESPONSE:
        break;

    default:
        LOGGER(LOG_ERR,"(%u): error, unknown command 0x%02hhx \n",
                        rfc4938_config_get_node_id (), p2ctlmsg->header.u8cmd_code);
        break;
    }

    return;
}




int
rfc4938_parser_cli_terminate_session (UINT32_t neighbor_id, UINT16_t cmdSRC)
{
    int result = rfc4938_parser_cli_terminate_session_i (neighbor_id, cmdSRC);

    return result;
}


int
rfc4938_parser_cli_padq_session (UINT32_t neighbor_id,
                                 UINT8_t receive_only,
                                 UINT8_t rlq,
                                 UINT8_t resources,
                                 UINT16_t latency,
                                 UINT16_t cdr_scale,
                                 UINT16_t current_data_rate,
                                 UINT16_t mdr_scale,
                                 UINT16_t max_data_rate)
{
    int result = rfc4938_parser_cli_padq_session_i (neighbor_id, receive_only, rlq, resources,
                 latency, cdr_scale,  current_data_rate, mdr_scale, max_data_rate);

    return result;
}



static int
rfc4938_parser_cli_padq_session_i (UINT32_t neighbor_id,
                                   UINT8_t receive_only,
                                   UINT8_t rlq,
                                   UINT8_t resources,
                                   UINT16_t latency,
                                   UINT16_t cdr_scale,
                                   UINT16_t current_data_rate,
                                   UINT16_t mdr_scale,
                                   UINT16_t max_data_rate)
{
    int retval;
    int buflen;
    rfc4938_neighbor_element_t *nbr;
    void *p2buffer = NULL;

    /* find the neighbor */
    if (rfc4938_neighbor_pointer_by_nbr_id (neighbor_id, &nbr) != 0)
    {
        LOGGER(LOG_ERR,"(%u): error, unable to find neighbor_id %u\n",
                       rfc4938_config_get_node_id (), neighbor_id);

        return (-1);
    }

    /* check to see if there is a session up for it */
    if (nbr->nbr_session_state != ACTIVE)
    {
        LOGGER(LOG_INFO, "(%u): neighbor_id %u is not active yet, drop\n",
                          rfc4938_config_get_node_id (), neighbor_id);

        return (-1);
    }

    p2buffer = malloc (SIZEOF_CTL_PADQ_REQUEST);

    if (p2buffer == NULL)
    {
        LOGGER(LOG_ERR,"(%u): error, unable to allocate buffer for ctl_padq_request\n",
                        rfc4938_config_get_node_id ());

        return (-1);
    }

    if ((buflen = rfc4938_ctl_format_session_padq (u32seqnum++,
                  receive_only,
                  rlq,
                  resources,
                  latency,
                  cdr_scale,
                  current_data_rate,
                  mdr_scale,
                  max_data_rate,
                  p2buffer)) != SIZEOF_CTL_PADQ_REQUEST)
    {
        LOGGER(LOG_ERR,"(%u): error, unable to format message\n",
                        rfc4938_config_get_node_id ());

        free (p2buffer);

        return (-1);
    }

    retval = rfc4938_io_send_to_child (nbr->child_port, p2buffer, buflen);

    if(retval == buflen)
    {
        LOGGER(LOG_INFO, "(%u): sent PADQ for neighbor %u, seqnum %u, "
                             "ro %hhu, rlq %hhu, resources %hhu, latency %hu, "
                             "cdr_scale %hu, cdr %hu, mdr_scale %hu, mdr %hu\n",
                             rfc4938_config_get_node_id (),
                             nbr->neighbor_id, u32seqnum - 1, receive_only, rlq, resources, latency,
                             cdr_scale, current_data_rate, mdr_scale, max_data_rate);
    }
    else
    {
        LOGGER(LOG_ERR,"(%u): failed to send PADQ for neighbor %u, "
                             "ro %hhu, rlq %hhu, resources %hhu, latency %hu, "
                             "cdr_scale %hu, cdr %hu, mdr_scale %hu, mdr %hu\n",
                             rfc4938_config_get_node_id (),
                             nbr->neighbor_id, receive_only, rlq, resources, latency,
                             cdr_scale, current_data_rate, mdr_scale, max_data_rate);
    }

    free (p2buffer);

    return (retval);
}



int
rfc4938_parser_cli_padg_session (UINT32_t neighbor_id, UINT16_t credits)
{
    int result = rfc4938_parser_cli_padg_session_i (neighbor_id, credits);

    return result;
}


void rfc4938_parse_ppp_packet(const PPPoEPacket *packet, int framelen, const char * direction)
{
    UINT16_t proto = ntohs(packet->eth_hdr.proto);

    if(proto == ETH_PPPOE_DISCOVERY)
    {
        LOGGER(LOG_PKT,"flen %d, type %hhu, ver %hhu, code %hhu (%s), session %hu, (%s)\n", 
                              framelen,
                              packet->pppoe_type,
                              packet->pppoe_ver,
                              packet->pppoe_code,
                              rfc4938_debug_code_to_string(packet->pppoe_code),
                              ntohs(packet->pppoe_session),
                              direction);
    }
    else if(proto == ETH_PPPOE_SESSION)
    {
        unsigned char *p = (unsigned char *) packet->payload;

        PPPoETag * tag = (PPPoETag *) p;

        switch(ntohs(tag->type))
        {
          case TAG_RFC4938_CREDITS:
          case TAG_RFC4938_METRICS:
          case TAG_RFC4938_SEQ_NUM:
          case TAG_RFC4938_SCALAR:

            LOGGER(LOG_PKT,"found tag 0x%04hx, skip %d bytes\n", ntohs(tag->type),
                                  ntohs(tag->length) + TAG_HDR_SIZE);

            p += ntohs(tag->length) + TAG_HDR_SIZE;
          break;
        }

        const PPPHeader * p2ppp = (const PPPHeader *) p;

        UINT16_t type = ntohs(p2ppp->type);

        if(type == PPP_LCP)
        {
            if(p2ppp->code == PPP_ECHO_REQ || p2ppp->code == PPP_ECHO_REPLY)
            {
                UINT32_t * p = (UINT32_t*) p2ppp->data;

                LOGGER(LOG_PKT,"flen %hu, type 0x%04hx (PPP_LCP), code %hhu (%s), session %hu, id %hhu, magic 0x%08x, (%s)\n",
                                      framelen,
                                      type,
                                      p2ppp->code,
                                      ppp_lcp_code_to_string(p2ppp->code),
                                      ntohs(packet->pppoe_session),
                                      p2ppp->id,
                                      ntohl(*p),
                                      direction);
            }
            else
            {
                LOGGER(LOG_PKT,"flen %hu, type 0x%04hx (PPP_LCP), code %hhu (%s), session %hu, id %hhu (%s)\n", 
                                      framelen,
                                      type,
                                      p2ppp->code,
                                      ppp_lcp_code_to_string(p2ppp->code),
                                      ntohs(packet->pppoe_session),
                                      p2ppp->id,
                                      direction);
            }
        }
        else if(type == PPP_IPCP)
        {
            LOGGER(LOG_PKT,"flen %hu, type 0x%04hx (PPP_IPCP), code %hhu (%s), session %hu, id %hhu (%s)\n",
                                  framelen,
                                  type,
                                  p2ppp->code,
                                  ppp_lcp_code_to_string(p2ppp->code),
                                  ntohs(packet->pppoe_session),
                                  p2ppp->id,
                                  direction);
        }
    }
}



static int
rfc4938_parser_cli_padg_session_i (UINT32_t neighbor_id, UINT16_t credits)
{
    int retval;
    int buflen;
    rfc4938_neighbor_element_t *nbr;
    void *p2buffer = NULL;

    LOGGER(LOG_INFO, "(%u): received CLI request to send %hu credits for nbr %u\n",
                      rfc4938_config_get_node_id (), credits, neighbor_id);

    /* find the neighbor */
    if (rfc4938_neighbor_pointer_by_nbr_id (neighbor_id, &nbr) != 0)
    {
        LOGGER(LOG_ERR,"(%u): error, unable to find neighbor_id %u\n",
                        rfc4938_config_get_node_id (), neighbor_id);
        return (-1);
    }

    /* check to see if there is a session up for it */
    if (nbr->nbr_session_state != ACTIVE)
    {
        LOGGER(LOG_INFO, "(%u): neighbor_id %u is not active yet, drop\n",
                         rfc4938_config_get_node_id (), neighbor_id);

        return (-1);
    }

    p2buffer = malloc (SIZEOF_CTL_PADG_REQUEST);

    if (p2buffer == NULL)
    {
        LOGGER(LOG_ERR,"(%u): error, unable to allocate buffer for ctl_padg_request\n",
                        rfc4938_config_get_node_id ());

        return (-1);
    }

    if ((buflen = rfc4938_ctl_format_session_padg (u32seqnum++, credits, p2buffer)) != SIZEOF_CTL_PADG_REQUEST)
    {
        LOGGER(LOG_ERR,"(%u): error, unable to format message\n",
                        rfc4938_config_get_node_id ());

        free (p2buffer);

        return (-1);
    }

    retval = rfc4938_io_send_to_child (nbr->child_port, p2buffer, buflen);

    if(retval == buflen)
    {
        LOGGER(LOG_PKT,"(%u): sent PADG seqnum %u, with %hu credits for neighbor %u\n",
                        rfc4938_config_get_node_id (), u32seqnum - 1, credits, nbr->neighbor_id);
    }
    else
    {
        LOGGER(LOG_ERR,"(%u): failed to send PADG with %hu credits for neighbor %u\n",
                        rfc4938_config_get_node_id (), credits, nbr->neighbor_id);
    }

    free (p2buffer);

    return (retval);
}


static int
rfc4938_parser_cli_show_session_i (void)
{
    char dgram[SHOWLEN];

    memset(dgram, 0x0, SHOWLEN);

    rfc4938_neighbor_print_all_string (dgram, SHOWLEN);

    int z = rfc4938_io_send_udp_packet (LOCALHOST,
                                        rfc4938_config_get_ctl_port(),
                                        dgram,
                                        (strlen (dgram) + 1));

    if (z < 0)
    {
        LOGGER(LOG_ERR,"(%u): error sending message:", rfc4938_config_get_node_id ());

        return (-1);
    }
    else
    {
        LOGGER(LOG_PKT,"(%u): sent message to %s:%hu, len %d",
                              rfc4938_config_get_node_id (), LOCALHOST,
                              rfc4938_config_get_ctl_port(), z);

        return (0);
    }
}



static void
rfc4938_parser_ctl_recv_session_start_i (UINT32_t neighbor_id, UINT32_t pid, UINT16_t scalar)
{
    rfc4938_neighbor_element_t *nbr;
    int buflen;
    void *p2buffer = NULL;

    LOGGER(LOG_INFO, "(%u): nbr %u, pid %u, scalar %u\n",
                         rfc4938_config_get_node_id (), neighbor_id, pid, scalar);

    if (rfc4938_neighbor_pointer_by_nbr_id (neighbor_id, &nbr) != 0)
    {
        LOGGER(LOG_ERR,"(%u): error, unknown nbr %u, drop\n",
                             rfc4938_config_get_node_id (), neighbor_id);

        return;
    }

    if (nbr->nbr_session_state == ACTIVE || nbr->child_pid != 0)
    {
        /*
         * we've hit a timing window where both pppoe clients were started
         * but neither will get the session_start_ready message to learn
         * about each other.  We will send this message.
         */
        if (nbr->nbr_session_state == INACTIVE)
        {
            LOGGER(LOG_INFO, "(%u): pppoe processs for nbr %u is inactive"
                                 " saving nbr pid %u \n", rfc4938_config_get_node_id (),
                                 neighbor_id, pid);

            nbr->neighbor_pid = pid;
        }
        else
        {
            p2buffer = malloc (SIZEOF_CTL_START_READY);

            if (p2buffer == NULL)
            {
                LOGGER(LOG_ERR,"(%u): error, unable to allocate buffer for ctl_start_ready\n",
                                rfc4938_config_get_node_id ());

                return;
            }

            if ((buflen = rfc4938_ctl_format_session_start_ready (u32seqnum++,
                          neighbor_id,    // nbr
                          pid,
                          p2buffer)) !=
                    SIZEOF_CTL_START_READY)
            {
                LOGGER(LOG_ERR,"(%u): Unable to format message\n", rfc4938_config_get_node_id ());

                free (p2buffer);

                return;
            }
            else
            {
                LOGGER(LOG_INFO, "(%u): pppoe processs for nbr %u is active"
                                     " send session_start_ready to child with pid %u, seqnum %u\n",
                                     rfc4938_config_get_node_id (), neighbor_id, pid, u32seqnum - 1);


            }

            rfc4938_io_send_to_child (nbr->child_port, p2buffer, buflen);

            free (p2buffer);
        }
    }
    else
    {
        LOGGER(LOG_INFO, "(%u): pppoe process for nbr %u, is not active, initiating now.\n",
                          rfc4938_config_get_node_id (), neighbor_id);

        rfc4938_neighbor_initiate_neighbor (neighbor_id, pid, scalar);
    }

    return;
}


static void
rfc4938_parser_ctl_recv_child_ready_i (UINT32_t neighbor_id, UINT16_t port, UINT32_t pid)
{
    rfc4938_neighbor_element_t *nbr;

    LOGGER(LOG_INFO, "(%u): nbr %u, port %hu, pid %u\n",
                      rfc4938_config_get_node_id(),
                         neighbor_id, port, pid);

    if (rfc4938_neighbor_pointer_by_nbr_id (neighbor_id, &nbr) != 0)
    {
        LOGGER(LOG_ERR,"(%u): error, unknown neighbor\n", rfc4938_config_get_node_id());
    }
    else if (nbr->child_pid != pid)
    {
        LOGGER(LOG_ERR,"(%u): error, child_pid mismatch expected %u, got %u\n",
                             rfc4938_config_get_node_id(), nbr->child_pid, pid);
    }
    else
    {
        nbr->nbr_session_state = READY;
        nbr->child_port = port;
    }

    return;
}



static void
rfc4938_parser_ctl_recv_child_session_terminated_i (UINT32_t neighbor_id, UINT16_t session_id)
{
    rfc4938_neighbor_element_t *nbr;

    LOGGER(LOG_INFO, "(%u): session %hu, neighbor %u\n",
                         rfc4938_config_get_node_id (), session_id, neighbor_id);

    if (rfc4938_neighbor_pointer_by_nbr_id (neighbor_id, &nbr) != 0)
    {
        LOGGER(LOG_ERR,"(%u): error, unknown nbr %u\n",
                             rfc4938_config_get_node_id (), neighbor_id);
    }
    else
    {
        rfc4938_neighbor_terminate_neighbor (nbr, CMD_SRC_CHILD, 0);
    }

    return;
}


static void
rfc4938_parser_ctl_recv_peer_session_terminated_i (UINT32_t neighbor_id)
{
    rfc4938_neighbor_element_t *nbr;

    LOGGER(LOG_INFO, "(%u): nbr %u\n", rfc4938_config_get_node_id (), neighbor_id);

    if (rfc4938_neighbor_pointer_by_nbr_id (neighbor_id, &nbr) != 0)
    {
        LOGGER(LOG_ERR,"(%u): error, unknown nbr %u\n", rfc4938_config_get_node_id (), neighbor_id);
    }
    else
    {
        rfc4938_neighbor_terminate_neighbor (nbr, CMD_SRC_PEER, 0);
    }

    return;
}


static void rfc4938_parser_ctl_recv_peer_session_data_i(UINT32_t neighbor_id, const void *buff, const int bufsize)
{
    rfc4938_neighbor_element_t *nbr;

    if (rfc4938_neighbor_pointer_by_nbr_id (neighbor_id, &nbr) != 0)
    {
        LOGGER(LOG_ERR,"(%u): error, unknown nbr %hu\n",
                             rfc4938_config_get_node_id (), neighbor_id);
    }
    else
    {
        LOGGER(LOG_INFO, "(%u): nbr %u, len %d, session state is %s\n",
                          rfc4938_config_get_node_id (),
                          neighbor_id, bufsize,
                          rfc4938_neighbor_status_to_string(nbr->nbr_session_state));

        rfc4938_io_forward_to_child (neighbor_id, buff, bufsize, nbr);
    }

    return;
}




static void rfc4938_parser_ctl_recv_child_session_data_i(UINT32_t neighbor_id, UINT16_t credits,
        const void *buff, const int bufsize)
{
    rfc4938_neighbor_element_t *nbr;

    if (rfc4938_neighbor_pointer_by_nbr_id (neighbor_id, &nbr) != 0)
    {
        LOGGER(LOG_ERR,"(%u): error, unknown nbr %hu\n", rfc4938_config_get_node_id (), neighbor_id);
    }
    else
    {
        rfc4938_ctl_format_peer_session_data (u32seqnum++, (void*) buff, rfc4938_config_get_node_id());

        LOGGER(LOG_INFO, "(%u): neighbor %u, len %d, seqnum %u\n",
                             rfc4938_config_get_node_id (), neighbor_id, bufsize, u32seqnum - 1);

        rfc4938_io_send_to_nbr (neighbor_id, credits, buff, bufsize);
    }

    return;
}


static void
rfc4938_parser_ctl_recv_child_session_up_i (UINT32_t neighbor_id, UINT16_t session_id, UINT32_t pid)
{
    rfc4938_neighbor_element_t *nbr;

    LOGGER(LOG_INFO, "(%u): nbr %u, session id %hu, pid %u\n",
                      rfc4938_config_get_node_id (),
                      neighbor_id, session_id, pid);


    if (rfc4938_neighbor_pointer_by_nbr_id (neighbor_id, &nbr) != 0)
    {
        LOGGER(LOG_ERR,"(%u): error, unknown neighbor %u\n", rfc4938_config_get_node_id (), neighbor_id);

        return;
    }

    if (nbr->child_pid != pid)
    {
        LOGGER(LOG_ERR,"(%u): error, child_pid mismatch expected %u, got %u\n",
                             rfc4938_config_get_node_id(), nbr->child_pid, pid);

        return;
    }

    if(session_id != 0)
    {
        nbr->nbr_session_state = ACTIVE;

        nbr->session_id = session_id;

        LOGGER(LOG_INFO, "(%u): nbr %u, session id %hu, pid %u set to ACTIVE\n",
                          rfc4938_config_get_node_id (),
                          neighbor_id, session_id, pid);
    }
    else
    {
        LOGGER(LOG_ERR,"(%u): error, neighbor_id %u already on session %hu\n",
                        rfc4938_config_get_node_id (), neighbor_id, session_id);
    }

    return;
}

static int
rfc4938_parser_cli_initiate_session_i (UINT32_t neighbor_id, UINT16_t credit_scalar)
{
    LOGGER(LOG_INFO, "(%u): initiating session for nbr %u, credit_scalar %hu\n",
                      rfc4938_config_get_node_id (), neighbor_id, credit_scalar);

    rfc4938_neighbor_initiate_neighbor (neighbor_id, 0, credit_scalar);

    return 0;
}


static int
rfc4938_parser_cli_terminate_session_i (UINT32_t neighbor_id, UINT16_t cmdSRC)
{
    int retval;

    rfc4938_neighbor_element_t *nbr;

    LOGGER(LOG_INFO, "(%u): neighbor_id %u, cmd %hu\n",
                      rfc4938_config_get_node_id (), neighbor_id, cmdSRC);

    if (neighbor_id == 0)
    {
        rfc4938_neighbor_toggle_all (&rfc4938_neighbor_terminate_neighbor, cmdSRC);

        retval = 0;
    }
    else
    {
        if (rfc4938_neighbor_pointer_by_nbr_id (neighbor_id, &nbr) != 0)
        {
            LOGGER(LOG_ERR,"(%u): error, unable to find neighbor_id %u\n",
                            rfc4938_config_get_node_id (), neighbor_id);

            retval = -1;
        }
        else
        {
            rfc4938_neighbor_terminate_neighbor (nbr, cmdSRC, 0);

            retval = 0;
        }
    }

    return retval;
}



