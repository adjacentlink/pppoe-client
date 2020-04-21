/*-----------------------------------------------------------------------------/
 * project: rfc4938
 * file: rfc4938.h
 * version: 1.0
 * date: October 4, 2007
 *
 * Copyright (c) 2013 - Adjacent Link, LLC, Bridgewater NJ
 * Copyright (C) 2007-2008 by Cisco Systems, Inc.
 *
 * ===========================
 * This is the header file for rfc4938.c and rfc4938ctl.c
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

#ifndef __RFC4938_H__
#define __RFC4938_H__

#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <wait.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>     
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/ether.h>


#include "pppoe_types.h"
#include "rfc4938_types.h"
#include "rfc4938_neighbor_manager.h"
#include "rfc4938_messages.h"
#include "rfc4938_debug.h"

#define MAX_SOCK_MSG_LEN ( 2048 )
#define MAXFILENAME ( 512 )
#define CONFIGPATH "/etc/rfc4938.conf"
#define LNLEN ( 100 )

#define IPV4_STR_LENGTH ( 20 )
#define MAX_IFACES ( 10 )

#define ETH_ADDR_LEN 6

/* argv definitions */
#define CMD ( 1 )
#define PR_STR ( 2 )
#define PR ( 3 )
#define CREDS ( 4 )
#define MDR_STR ( 4 )
#define SCLR ( 4 )
#define MDR ( 5 )
#define MDR_S ( 6 )
#define CDR_STR ( 7 )
#define CDR ( 8 )
#define CDR_S ( 9 )
#define LTNCY_STR ( 10 )
#define LTNCY ( 11 )
#define RSRCS_STR ( 12 )
#define RSRCS ( 13 )
#define RLQ_STR ( 14 )
#define RLQ ( 15 )
#define RCV ( 16 )
#define END ( 17 )

/* base for stroul conversion */
#define BASE ( 10 )

/* MAX values for user input */
#define SCLR_MAX ( 65535 )
#define MDR_MAX  ( 65535 )
#define CDR_MAX  ( 65535 )
#define LTNCY_MAX ( 65535 )
#define CREDS_MAX ( 65535 )
#define MDR_SCLR_MAX ( 3 )
#define CDR_SCLR_MAX ( 3 )
#define RSRCS_MAX ( 100 )
#define RLQ_MAX ( 100 ) 


void rfc4938client_handle_signal_event ();

#endif
