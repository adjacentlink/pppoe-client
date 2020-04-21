/*-----------------------------------------------------------------------------/
 * project: rfc4938
 * file: rfc4938_config.h
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

#ifndef __RFC4938_CONFIG_H__
#define __RFC4938_CONFIG_H__


#include "pppoe_types.h"

#define CREDIT_DIST_MODE_EVEN    0   // distribute credits evenly (flow control enabled)
#define CREDIT_DIST_MODE_FLAT    1   // distribute credits flat (flow control enabled)
#define CREDIT_DIST_MODE_DIRECT  2   // distribute credits directly (no flow control required)



int rfc4938_config_read_config_file (char *filename);

char* rfc4938_config_get_iface (void);

UINT32_t rfc4938_config_get_node_id (void);

char* rfc4938_config_get_transport_endpoint (void);

char* rfc4938_config_get_platform_endpoint (void);

char* rfc4938_config_get_service_name (void);

char* rfc4938_config_get_pppoe_binary_path (void);

UINT16_t rfc4938_config_get_max_nbrs (void);

UINT16_t rfc4938_config_get_client_port (void);

UINT16_t rfc4938_config_get_ctl_port (void);

UINT16_t rfc4938_config_get_debug_level (void);

UINT16_t rfc4938_config_get_credit_scalar (void);

UINT16_t rfc4938_config_get_credit_grant (void);

UINT16_t rfc4938_config_get_hello_interval (void);

UINT16_t rfc4938_config_get_session_timeout (void);

int rfc4938_config_get_vif_mode (void);

int rfc4938_config_get_p2p_mode (void);

int rfc4938_config_get_credit_dist_mode (void);

int rfc4938_config_is_flow_control_enabled(void);

int rfc4938_config_get_lcp_echo_pong_mode (void);

float rfc4938_config_get_sinr_min (void);

float rfc4938_config_get_sinr_max (void);

float rfc4938_config_get_credit_threshold(void);

UINT32_t rfc4938_config_get_id(UINT32_t id);

UINT8_t * rfc4938_config_get_hwaddr (void);

struct rfc4938config_vars {
    char     interface_name[64];
    char     service_name[256];
    char     pppoe_binary_path[512];
    char     platform_endpoint[512];
    char     transport_endpoint[512];

    UINT32_t node_id;

    UINT16_t max_nbrs;
    UINT16_t client_port;
    UINT16_t ctl_port;
    UINT16_t debug_level;
    UINT16_t credit_scalar;
    UINT16_t credit_grant;
    UINT16_t session_timeout;

    int      hello_interval;
    int      vif_mode;
    int      p2p_mode;
    int      credit_dist_mode;
    int      flow_control_enabled;
    int      lcp_echo_pong_mode;

    float    sinr_min;
    float    sinr_max;
    float    credit_threshold;

    UINT8_t   hwaddr[6];
};

#define CAP_CREDIT_GRANT(x) do { \
                                 if((rfc4938_config_get_credit_grant() != 0) &&  \
                                    ((*x) > rfc4938_config_get_credit_grant()))   \
                                       (*x) = rfc4938_config_get_credit_grant();  \
                               } while (0); \

#endif
