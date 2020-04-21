/*-----------------------------------------------------------------------------/
 * project: rfc4938
 * file: rfc4938_messages.c
 * version: 1.0
 * date: October 21, 2007
 *
 * Copyright (c) 2013 - Adjacent Link, LLC, Bridgewater NJ
 * Copyright (C), 2007-2008 by Cisco Systems, Inc.
 *
 * ===========================
 *
 * This file provides APIs to format the CLI and control messages.
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


#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <arpa/inet.h>

#include "rfc4938_types.h"
#include "rfc4938_messages.h"


/*
 * rfc4938_ctl_format_session_start
 *
 * Description
 *    This function formats the CTL session start message in the
 *    provided buffer.  This message is sent from one box to
 *    another to initiate the session.
 *
 * Inputs:
 *    p2buffer       Pointer to the buffer to be formatted
 *
 * Outputs:
 *    p2buffer       Formatted buffer. Only valid w/ SUCCESS.
 *
 * Returns:
 *    SUCCESS msg len
 *    ERROR -1
 */
int
rfc4938_ctl_format_session_start (UINT32_t seq, UINT32_t neighbor_id,
                                  UINT32_t pid, UINT16_t credit_scalar, void *p2buffer)
{
  rfc4938_ctl_message_t *p2msg;

  if (p2buffer == NULL)
    {
      return (-1);
    }

  p2msg = p2buffer;

  /*
   * Insert the header
   */
  p2msg->header.u16hdrchk       = htons (HDR_PREFIX);
  p2msg->header.u8cmd_code      = CTL_SESSION_START;
  p2msg->header.u32seq_number   = htonl(seq);

  /*
   * and now the payload
   */
  p2msg->ctl_start_payload.u32neighbor_id   = htonl (neighbor_id);
  p2msg->ctl_start_payload.u32pid           = htonl (pid);
  p2msg->ctl_start_payload.u16credit_scalar = htons (credit_scalar);

  return SIZEOF_CTL_START_REQUEST;
}


/*
 * rfc4938_ctl_format_session_start_ready
 *
 * Description
 *    This function formats the session start ready message
 *    in the provided buffer.  This message is sent in response
 *    to the session start message, confirming the establishment
 *    of the neighbor session.  This is a box to box message.
 *
 * Inputs:
 *    pid_number
 *    p2buffer       Pointer to the buffer to be formatted
 *
 * Outputs:
 *    p2buffer       Formatted buffer. Only valid w/ SUCCESS.
 *
 * Returns:
 *    SUCCESS msg len
 *    ERROR -1
 */
int
rfc4938_ctl_format_session_start_ready (UINT32_t seq, UINT32_t neighbor_id, UINT32_t pid, void *p2buffer)
{
  rfc4938_ctl_message_t *p2msg;

  if (p2buffer == NULL)
    {
      return (-1);
    }

  p2msg = p2buffer;

  /*
   * Insert the header
   */
  p2msg->header.u16hdrchk       = htons (HDR_PREFIX);
  p2msg->header.u8cmd_code      = CTL_SESSION_START_READY;
  p2msg->header.u32seq_number   = htonl(seq);

  /*
   * and now the payload
   */
  p2msg->ctl_start_ready_payload.u32neighbor_id = htonl (neighbor_id);
  p2msg->ctl_start_ready_payload.u32pid = htonl (pid);

  return SIZEOF_CTL_START_READY;
}

/*
 * rfc4938_ctl_format_child_ready
 *
 * Description
 *    This function formats the session start ready message
 *    in the provided buffer.  This message is sent in response
 *    to the session start message, confirming the establishment
 *    of the neighbor session.  This is a child to parent message.
 *
 * Inputs:
 *    port_number  
 *    pid            The child pid
 *    p2buffer       Pointer to the buffer to be formatted
 *
 * Outputs:
 *    p2buffer       Formatted buffer. Only valid w/ SUCCESS.
 *
 * Returns:
 *    SUCCESS msg len
 *    ERROR -1
 */
int
rfc4938_ctl_format_child_ready (UINT32_t seq, UINT32_t neighbor_id,
                                UINT16_t port_number, 
                                UINT32_t pid, void *p2buffer)
{
  rfc4938_ctl_message_t *p2msg;

  if (p2buffer == NULL)
    {
      return (-1);
    }

  p2msg = p2buffer;

  /*
   * Insert the header
   */
  p2msg->header.u16hdrchk       = htons (HDR_PREFIX);
  p2msg->header.u8cmd_code      = CTL_CHILD_READY;
  p2msg->header.u32seq_number   = htonl(seq);

  /*
   * and now the payload
   */
  p2msg->ctl_child_ready_payload.u32neighbor_id = htonl (neighbor_id);
  p2msg->ctl_child_ready_payload.u16port_number = htons (port_number);
  p2msg->ctl_child_ready_payload.u32pid         = htonl (pid);

  return SIZEOF_CTL_CHILD_READY;
}


/*
 * rfc4938_ctl_format_child_session_up
 *
 * Description
 *    This function formats the session start ready message
 *    in the provided buffer.  This message is sent in response
 *    to the session start message, confirming the establishment
 *    of the neighbor session.  This is a child to parent message.
 *
 * Inputs:
 *    session id     The session id
 *    pid            The child pid
 *    p2buffer       Pointer to the buffer to be formatted
 *
 * Outputs:
 *    p2buffer       Formatted buffer. Only valid w/ SUCCESS.
 *
 * Returns:
 *    SUCCESS msg len
 *    ERROR -1
 */
int
rfc4938_ctl_format_child_session_up (UINT32_t seq, UINT32_t neighbor_id,
                                     UINT16_t session_id,
                                     UINT32_t pid, void *p2buffer)
{
  rfc4938_ctl_message_t *p2msg;

  if (p2buffer == NULL)
    {
      return (-1);
    }

  p2msg = p2buffer;

  /*
   * Insert the header
   */
  p2msg->header.u16hdrchk       = htons (HDR_PREFIX);
  p2msg->header.u8cmd_code      = CTL_CHILD_SESSION_UP;
  p2msg->header.u32seq_number   = htonl(seq);

  /*
   * and now the payload
   */
  p2msg->ctl_child_session_up_payload.u32neighbor_id = htonl (neighbor_id);
  p2msg->ctl_child_session_up_payload.u16session_id  = htons (session_id);
  p2msg->ctl_child_session_up_payload.u32pid         = htonl (pid);

  return SIZEOF_CTL_CHILD_SESSION_UP;
}




/*
 * rfc4938_ctl_format_session_stop
 *
 * Description
 *    This function formats the session stop message in the
 *    provided buffer.  This message is sent to tear down
 *    an active neighbor session.  This is a box to box
 *    message.
 *
 * Inputs:
 *    p2buffer       Pointer to the buffer to be formatted
 *
 * Outputs:
 *    p2buffer       Formatted buffer. Only valid w/ SUCCESS.
 *
 * Returns:
 *    SUCCESS
 *    ERANGE
 */
int
rfc4938_ctl_format_session_stop (UINT32_t seq, void *p2buffer)
{
  rfc4938_ctl_message_t *p2msg;

  if (p2buffer == NULL)
    {
      return (-1);
    }

  p2msg = p2buffer;

  /*
   * Insert the header
   */
  p2msg->header.u16hdrchk       = htons (HDR_PREFIX);
  p2msg->header.u8cmd_code      = CTL_SESSION_STOP;
  p2msg->header.u32seq_number   = htonl(seq);

  /* no payload */

  return SIZEOF_CTL_SESSION_STOP;
}


/*
 * rfc4938_ctl_format_child_session_terminated
 *
 * Description
 *    This function formats the session stop message in the
 *    provided buffer.  This message is sent to tear down
 *    an active neighbor session.  This is a box to box
 *    message.
 *
 * Inputs:
 *    p2buffer       Pointer to the buffer to be formatted
 *
 * Outputs:
 *    p2buffer       Formatted buffer. Only valid w/ SUCCESS.
 *
 * Returns:
 *    SUCCESS
 *    ERANGE
 */
int
rfc4938_ctl_format_child_session_terminated (UINT32_t seq, UINT32_t neighbor_id, UINT16_t session_id, void *p2buffer)
{
  rfc4938_ctl_message_t *p2msg;

  if (p2buffer == NULL)
    {
      return (-1);
    }

  p2msg = p2buffer;

  /*
   * Insert the header
   */
  p2msg->header.u16hdrchk       = htons (HDR_PREFIX);
  p2msg->header.u8cmd_code      = CTL_CHILD_SESSION_TERMINATED;
  p2msg->header.u32seq_number   = htonl(seq);

  p2msg->ctl_child_session_terminated_payload.u32neighbor_id = htonl (neighbor_id);
  p2msg->ctl_child_session_terminated_payload.u16session_id  = htons (session_id);

  return SIZEOF_CTL_CHILD_SESSION_TERMINATED;
}


/*
 * rfc4938_ctl_format_peer_session_terminated
 *
 * Description
 *    This function formats the session stop message in the
 *    provided buffer.  This message is sent to tear down
 *    an active neighbor session.  This is a box to box
 *    message.
 *
 * Inputs:
 *    p2buffer       Pointer to the buffer to be formatted
 *
 * Outputs:
 *    p2buffer       Formatted buffer. Only valid w/ SUCCESS.
 *
 * Returns:
 *    SUCCESS
 *    ERANGE
 */
int
rfc4938_ctl_format_peer_session_terminated (UINT32_t seq, UINT32_t neighbor_id, void *p2buffer)
{
  rfc4938_ctl_message_t *p2msg;

  if (p2buffer == NULL)
    {
      return (-1);
    }

  p2msg = p2buffer;

  /*
   * Insert the header
   */
  p2msg->header.u16hdrchk       = htons (HDR_PREFIX);
  p2msg->header.u8cmd_code      = CTL_PEER_SESSION_TERMINATED;
  p2msg->header.u32seq_number   = htonl(seq);

  p2msg->ctl_peer_session_terminated_payload.u32neighbor_id  = htonl (neighbor_id);

  return SIZEOF_CTL_PEER_SESSION_TERMINATED;
}



/*
 * rfc4938_ctl_format_session_padq
 *
 * Description
 *    This function formats the session PADQ message in the
 *    provided buffer.  This message is sent to manipulate
 *    the quality metrics of the specified active neighbor
 *    session.  This is a box to box message.
 *
 * Inputs:
 *    receive_only       present, but not used
 *    rlq                relative link quality, 0-100%
 *    resources          0-100%
 *    latency            milliseconds
 *    cdr_scale
 *    current_data_rate
 *    mdr_scale
 *    max_data_rate
 *    p2buffer           Pointer to the buffer to be formatted
 *
 * Outputs:
 *    p2buffer           Formatted buffer. Only valid w/ SUCCESS.
 *
 * Returns:
 *    SUCCESS msg len
 *    ERROR -1
 */
int
rfc4938_ctl_format_session_padq (UINT32_t seq, UINT8_t receive_only,
                                 UINT8_t rlq,
                                 UINT8_t resources,
                                 UINT16_t latency,
                                 UINT16_t cdr_scale,
                                 UINT16_t current_data_rate,
                                 UINT16_t mdr_scale, 
                                 UINT16_t max_data_rate, 
                                 void *p2buffer)
{
  rfc4938_ctl_message_t *p2msg;

  if (p2buffer == NULL)
    {
      return (-1);
    }

  p2msg = p2buffer;

  /*
   * Insert the header
   */
  p2msg->header.u16hdrchk       = htons (HDR_PREFIX);
  p2msg->header.u8cmd_code      = CTL_SESSION_PADQ;
  p2msg->header.u32seq_number   = htonl(seq);

  /*
   * and now the payload
   */
  p2msg->ctl_padq_payload.u8receive_only = receive_only;
  p2msg->ctl_padq_payload.u8rlq          = rlq;
  p2msg->ctl_padq_payload.u8resources    = resources;

  p2msg->ctl_padq_payload.u16latency           = htons (latency);
  p2msg->ctl_padq_payload.u16cdr_scale         = htons (cdr_scale);
  p2msg->ctl_padq_payload.u16current_data_rate = htons (current_data_rate);
  p2msg->ctl_padq_payload.u16mdr_scale         = htons (mdr_scale);
  p2msg->ctl_padq_payload.u16max_data_rate     = htons (max_data_rate);


  return SIZEOF_CTL_PADQ_REQUEST;
}


/*
 * rfc4938_ctl_format_session_padg
 *
 * Description
 *    This function formats the session PADG message in the
 *    provided buffer.  This message is sent to manipulate
 *    the credit grants of the specified active neighbor
 *    session.  This is a box to box message.
 *
 * Inputs:
 *    credits            credits to grant (inject)
 *    p2buffer           Pointer to the buffer to be formatted
 *
 * Outputs:
 *    p2buffer           Formatted buffer. Only valid w/ SUCCESS.
 *
 * Returns:
 *    SUCCESS msg len
 *    ERROR -1
 */
int
rfc4938_ctl_format_session_padg (UINT32_t seq, UINT16_t credits, void *p2buffer)
{
  rfc4938_ctl_message_t *p2msg;

  if (p2buffer == NULL)
    {
      return (-1);
    }

  p2msg = p2buffer;

  /*
   * Insert the header
   */
  p2msg->header.u16hdrchk       = htons (HDR_PREFIX);
  p2msg->header.u8cmd_code      = CTL_SESSION_PADG;
  p2msg->header.u32seq_number   = htonl(seq);

  /*
   * and now the payload
   */
  p2msg->ctl_padg_payload.u16credits = htons (credits);

  return SIZEOF_CTL_PADG_REQUEST;
}


int
rfc4938_ctl_format_child_session_data (UINT32_t seq, UINT32_t neighbor_id,
                                       UINT16_t datalen, UINT16_t credits,
                                       void *p2data, void *p2buffer)
{
  rfc4938_ctl_message_t *p2msg;

  if (p2buffer == NULL)
    {
      return (-1);
    }

  if (p2data == NULL)
    {
      return (-1);
    }

  p2msg = p2buffer;

  /*
   * Insert the header
   */
  p2msg->header.u16hdrchk       = htons (HDR_PREFIX);
  p2msg->header.u8cmd_code      = CTL_CHILD_SESSION_DATA;
  p2msg->header.u32seq_number   = htonl(seq);

  /*
   * and now the payload
   */
  p2msg->ctl_session_data_payload.u16data_len    = htons (datalen);
  p2msg->ctl_session_data_payload.u16credits     = htons (credits);
  p2msg->ctl_session_data_payload.u32neighbor_id = htonl (neighbor_id);
  memcpy (p2msg->ctl_session_data_payload.data, p2data, datalen);

  return SIZEOF_CTL_CHILD_SESSION_DATA + datalen;
}


int
rfc4938_ctl_format_peer_session_data (UINT32_t seq, void *p2buffer, UINT32_t neighbor_id)
{
  rfc4938_ctl_message_t *p2msg;

  if (p2buffer == NULL)
    {
      return (-1);
    }

  p2msg = p2buffer;

  p2msg->header.u16hdrchk       = htons (HDR_PREFIX);
  p2msg->header.u8cmd_code      = CTL_PEER_SESSION_DATA;
  p2msg->header.u32seq_number   = htonl(seq);

  p2msg->ctl_session_data_payload.u32neighbor_id = htonl (neighbor_id);

  return 1;
}



int
rfc4938_ctl_format_frame_data (UINT32_t seq, UINT16_t session_id, UINT16_t datalen, UINT16_t proto,
                            void *p2data, void *p2buffer)
{
  rfc4938_ctl_message_t *p2msg;

  if (p2buffer == NULL)
    {
      return (-1);
    }

  if (p2data == NULL)
    {
      return (-1);
    }

  p2msg = p2buffer;

  /*
   * Insert the header
   */
  p2msg->header.u16hdrchk       = htons (HDR_PREFIX);
  p2msg->header.u8cmd_code      = CTL_FRAME_DATA;
  p2msg->header.u32seq_number   = htonl(seq);

  /*
   * and now the payload
   */
  p2msg->ctl_frame_data_payload.u16data_len   = htons (datalen);
  p2msg->ctl_frame_data_payload.u16proto      = htons (proto);
  p2msg->ctl_frame_data_payload.u16session_id = htons (session_id);
  memcpy (p2msg->ctl_frame_data_payload.data, p2data, datalen);


  return SIZEOF_CTL_FRAME_DATA + datalen;
}


/*
 * rfc4938_cli_format_session_initiate
 *
 * Description
 *    This function formats the CLI session initiate
 *    message in the provided buffer.  This message
 *    is sent from the CLI process to the control
 *    process to initiate a neighbor session.
 *
 * Inputs:
 *    credit_scalar    Credit scalar to use for the neighbor
 *    p2buffer         Pointer to the buffer to be formatted
 *
 * Outputs:
 *    p2buffer         Formatted buffer. Only valid w/ SUCCESS.
 *
 * Returns:
 *    SUCCESS
 *    ERANGE
 */
int
rfc4938_cli_format_session_initiate (UINT32_t seq, UINT32_t neighbor_id, 
                                    UINT16_t credit_scalar, void *p2buffer)
{
  rfc4938_ctl_message_t *p2msg;

  if (p2buffer == NULL)
    {
      return (-1);
    }

  p2msg = p2buffer;

  /*
   * Insert the header
   */
  p2msg->header.u16hdrchk       = htons (HDR_PREFIX);
  p2msg->header.u8cmd_code      = CLI_SESSION_INITIATE;
  p2msg->header.u32seq_number   = htonl(seq);

  /*
   * and now the payload
   */
  p2msg->cli_initiate_payload.u16credit_scalar = htons (credit_scalar);
  p2msg->cli_initiate_payload.u32neighbor_id   = htonl (neighbor_id);

  return (SIZEOF_CLI_INITIATE_REQUEST);
}


/*
 * rfc4938_cli_format_session_terminate
 *
 * Description
 *    This function formats the CLI session terminate
 *    message in the provided buffer.  This message
 *    is sent from the CLI process to the control
 *    process to terminate a neighbor session.
 *
 * Inputs:
 *    p2buffer       Pointer to the buffer to be formatted
 *
 * Outputs:
 *    p2buffer       Formatted buffer. Only valid w/ SUCCESS.
 *
 * Returns:
 *    SUCCESS
 *    ERANGE
 */
int
rfc4938_cli_format_session_terminate (UINT32_t seq, UINT32_t neighbor_id, void *p2buffer)
{
  rfc4938_ctl_message_t *p2msg;

  if (p2buffer == NULL)
    {
      return (-1);
    }

  p2msg = p2buffer;

  /*
   * Insert the header
   */
  p2msg->header.u16hdrchk       = htons (HDR_PREFIX);
  p2msg->header.u8cmd_code      = CLI_SESSION_TERMINATE;
  p2msg->header.u32seq_number   = htonl(seq);

  /*
   * and now the payload
   */
  p2msg->cli_terminate_payload.u32neighbor_id = htonl (neighbor_id);

  return (SIZEOF_CLI_TERMINATE_REQUEST);
}


/*
 * rfc4938_cli_format_padq
 *
 * Description
 *    This function formats the PADQ message in the
 *    provided buffer.  This message is sent from
 *    the CLI process to the control process to inject
 *    a set of quality metric into an active neighbor session.
 *
 * Inputs:
 *    receive_only       present, but not used
 *    rlq                relative link quality, 0-100%
 *    resources          0-100%
 *    latency            milliseconds
 *    cdr_scale
 *    current_data_rate
 *    mdr_scale
 *    max_data_rate
 *    p2buffer           Pointer to the buffer to be formatted
 *
 * Outputs:
 *    p2buffer           Formatted buffer. Only valid w/ SUCCESS.
 *
 * Returns:
 *    SUCCESS
 *    ERANGE
 */
int
rfc4938_cli_format_padq (UINT32_t seq, UINT32_t neighbor_id,
                         UINT8_t receive_only,
                         UINT8_t rlq,
                         UINT8_t resources,
                         UINT16_t latency,
                         UINT16_t cdr_scale,
                         UINT16_t current_data_rate,
                         UINT16_t mdr_scale, 
                         UINT16_t max_data_rate, 
                         void *p2buffer)
{
  rfc4938_ctl_message_t *p2msg;

  if (p2buffer == NULL)
    {
      return (-1);
    }

  p2msg = p2buffer;

  /*
   * Insert the header
   */
  p2msg->header.u16hdrchk       = htons (HDR_PREFIX);
  p2msg->header.u8cmd_code      = CLI_SESSION_PADQ;
  p2msg->header.u32seq_number   = htonl(seq);

  /*
   * and now the payload
   */
  p2msg->cli_padq_payload.u32neighbor_id = htonl (neighbor_id);

  p2msg->cli_padq_payload.u8receive_only = receive_only;
  p2msg->cli_padq_payload.u8rlq          = rlq;
  p2msg->cli_padq_payload.u8resources    = resources;

  p2msg->cli_padq_payload.u16latency           = htons (latency);
  p2msg->cli_padq_payload.u16cdr_scale         = htons (cdr_scale);
  p2msg->cli_padq_payload.u16current_data_rate = htons (current_data_rate);
  p2msg->cli_padq_payload.u16mdr_scale         = htons (mdr_scale);
  p2msg->cli_padq_payload.u16max_data_rate     = htons (max_data_rate);

  return (SIZEOF_CLI_PADQ_REQUEST);
}


/*
 * rfc4938_cli_format_session_padg
 *
 * Description
 *    This function formats the session PADG message in the
 *    provided buffer.  This message is sent from
 *    the CLI process to the control process to manipulate
 *    the credits of an active neighbor session.
 *
 * Inputs:
 *    credit_scalar      scalar value
 *    credits            credits to grant (inject)
 *    p2buffer           Pointer to the buffer to be formatted
 *
 * Outputs:
 *    p2buffer           Formatted buffer. Only valid w/ SUCCESS.
 *
 * Returns:
 *    SUCCESS
 *    ERANGE
 */
int
rfc4938_cli_format_session_padg (UINT32_t seq, UINT32_t neighbor_id, 
                                 UINT16_t credits, void *p2buffer)
{
  rfc4938_ctl_message_t *p2msg;

  if (p2buffer == NULL)
    {
      return (-1);
    }

  p2msg = p2buffer;

  /*
   * Insert the header
   */
  p2msg->header.u16hdrchk       = htons (HDR_PREFIX);
  p2msg->header.u8cmd_code      = CLI_SESSION_PADG;
  p2msg->header.u32seq_number   = htonl(seq);

  /*
   * and now the payload
   */
  p2msg->cli_padg_payload.u32neighbor_id = htonl (neighbor_id);
  p2msg->cli_padg_payload.u16credits     = htons (credits);

  return (SIZEOF_CLI_PADG_REQUEST);
}


/*
 * rfc4938_cli_format_session_show
 *
 * Description
 *    This function formats the session show message in the
 *    provided buffer.  This message is sent from
 *    the CLI process to the control process to display
 *    status information of an active neighbor session.
 *
 * Inputs:
 *    p2buffer       Pointer to the buffer to be formatted
 *
 * Outputs:
 *    p2buffer       Formatted buffer. Only valid w/ SUCCESS.
 *
 * Returns:
 *    SUCCESS
 *    ERANGE
 */
int
rfc4938_cli_format_session_show (UINT32_t seq, void *p2buffer)
{
  rfc4938_ctl_message_t *p2msg;

  if (p2buffer == NULL)
    {
      return (-1);
    }

  p2msg = p2buffer;

  /*
   * Insert the header
   */
  p2msg->header.u16hdrchk       = htons (HDR_PREFIX);
  p2msg->header.u8cmd_code      = CLI_SESSION_SHOW;
  p2msg->header.u32seq_number   = htonl(seq);

  /*  no payload */

  return (SIZEOF_CLI_SHOW_REQUEST);
}


/*
 * rfc4938_cli_format_session_show_response
 *
 * Description
 *    This function formats the session show response
 *    message in the provided buffer.
 *    *** DO WE REALLY NEED THIS ***
 *
 * Inputs:
 *    p2buffer       Pointer to the buffer to be formatted
 *
 * Outputs:
 *    p2buffer       Formatted buffer. Only valid w/ SUCCESS.
 *
 * Returns:
 *    SUCCESS
 *    ERANGE
 */
int
rfc4938_cli_format_session_show_response (UINT32_t seq, UINT32_t neighbor_id, void *p2buffer)
{
  rfc4938_ctl_message_t *p2msg;

  if (p2buffer == NULL)
    {
      return (-1);
    }

  p2msg = p2buffer;

  /*
   * Insert the header
   */
  p2msg->header.u16hdrchk       = htons (HDR_PREFIX);
  p2msg->header.u8cmd_code      = CLI_SESSION_SHOW_RESPONSE;
  p2msg->header.u32seq_number   = htonl(seq);

  /*
   * and now the payload
   */
  p2msg->cli_show_response_payload.u32neighbor_id = htonl (neighbor_id);

  return (SIZEOF_CLI_SHOW_RESPONSE);
}



