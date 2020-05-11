/*-----------------------------------------------------------------------------/
 * project: rfc4938
 * file: rfc4938_config.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rfc4938_config.h"
#include "rfc4938_debug.h"
#include "rfc4938_neighbor_manager.h"

#define DEFAULT_PPPOEBINARY         "/usr/sbin/rfc4938pppoe"
#define DEFAULT_SERVICE_NAME        "rfc4938"
#define DEFAULT_SINR_MIN              0.0
#define DEFAULT_SINR_MAX            20.0
#define DEFAULT_MAX_NEIGHBORS        256
#define DEFAULT_CLIENT_PORT         6001
#define DEFAULT_CREDIT_GRANT         256
#define DEFAULT_CREDIT_SCALAR         64
#define DEFAULT_HELLO_INTERVAL         5
#define DEFAULT_P2P_MODE               1
#define DEFAULT_LCP_ECHO_PONG_MODE     0
#define DEFAULT_CREDIT_DIST_MODE       CREDIT_DIST_MODE_DIRECT
#define DEFAULT_FLOW_CONTROL_ENABLED   0
#define DEFAULT_CREDIT_THRESHOLD       0.25
#define DEFAULT_SESSION_TIMEOUT        60

static struct rfc4938config_vars CONFIG;

/*
 * Read Config File
 *
 * This function parses the configuration file and assigns
 * the appropriate parameters in the CONFIG struct. Then
 * neighbors are added to linked list.
 *
 * input:
 *   filename: name of config file
 *
 * return:
 *   SUCCESS
 */


int
rfc4938_config_read_config_file (char *filename)
{
    int i;

    FILE *fp;
#define MAX_INPUT_LENGTH  ( 512 )
    char input_string[MAX_INPUT_LENGTH];

#define ARGC_MAX   ( 5 )
    UINT32_t argc;
    char *argv[ARGC_MAX];

    fp = fopen (filename, "r");
    if (!fp)
    {
        LOGGER(LOG_ERR,"error, problem opening config file: %s\n", filename);
        return (-1);
    }

    memset (&CONFIG, 0x0, sizeof (CONFIG));

    /* setup default values */
    CONFIG.sinr_min = DEFAULT_SINR_MIN;

    CONFIG.sinr_max = DEFAULT_SINR_MAX;

    strncpy(CONFIG.pppoe_binary_path, DEFAULT_PPPOEBINARY, sizeof(CONFIG.pppoe_binary_path));

    strncpy(CONFIG.service_name, DEFAULT_SERVICE_NAME, sizeof(CONFIG.service_name));

    CONFIG.max_nbrs = DEFAULT_MAX_NEIGHBORS;

    CONFIG.hello_interval = DEFAULT_HELLO_INTERVAL;

    CONFIG.client_port = DEFAULT_CLIENT_PORT;

    CONFIG.credit_grant = DEFAULT_CREDIT_GRANT;

    CONFIG.credit_scalar = DEFAULT_CREDIT_SCALAR;

    CONFIG.p2p_mode = DEFAULT_P2P_MODE;

    CONFIG.lcp_echo_pong_mode = DEFAULT_LCP_ECHO_PONG_MODE;

    CONFIG.credit_dist_mode = DEFAULT_CREDIT_DIST_MODE;

    CONFIG.flow_control_enabled = DEFAULT_FLOW_CONTROL_ENABLED;

    CONFIG.credit_threshold = DEFAULT_CREDIT_THRESHOLD;

    CONFIG.session_timeout = DEFAULT_SESSION_TIMEOUT;

    while (fgets (input_string, MAX_INPUT_LENGTH, fp))
    {
        argv[0] = strtok (input_string, " \t\n");

        argc = 1;

        for (i = 1; i < ARGC_MAX; i++)
        {
            argv[i] = strtok (NULL, " \t\n");

            if (argv[i] == NULL)
            {
                break;
            }
            else
            {
                argc++;
            }
        }

        /* empty line */
        if (argv[0] == NULL)
        {
            continue;
        }

        /* comment */
        else if (strncmp (argv[0], "#", strlen ("#")) == 0)
        {
            continue;
        }

        /* interface_name */
        else if (strncmp (argv[0], "IFACE", strlen ("IFACE")) == 0)
        {
            memset  (CONFIG.interface_name, 0, sizeof(CONFIG.interface_name));
            strncpy (CONFIG.interface_name, argv[1], sizeof(CONFIG.interface_name)-1);
        }

        /* max_neighbors */
        else if (strncmp (argv[0], "MAX_NEIGHBORS", strlen ("MAX_NEIGHBORS")) == 0)
        {
            CONFIG.max_nbrs = strtoul (argv[1], NULL, 10);
        }

        /* our port */
        else if (strncmp (argv[0], "PORT", strlen ("PORT")) == 0)
        {
            CONFIG.client_port = strtoul (argv[1], NULL, 10);
        }

        /* our id */
        else if (strncmp (argv[0], "NODE_ID", strlen ("NODE_ID")) == 0)
        {
            CONFIG.node_id = strtoul (argv[1], NULL, 10);
        }

        /* ctl_port */
        else if (strncmp (argv[0], "CTL_PORT", strlen ("CTL_PORT")) == 0)
        {
            CONFIG.ctl_port = strtoul (argv[1], NULL, 10);
        }

        /* service_name */
        else if (strncmp (argv[0], "SERVICE_NAME", strlen ("SERVICE_NAME")) == 0)
        {
            memset  (CONFIG.service_name, 0, sizeof(CONFIG.service_name));
            strncpy (CONFIG.service_name, argv[1], sizeof(CONFIG.service_name)-1);
        }

        /* debug_level */
        else if (strncmp (argv[0], "DEBUG_LEVEL", strlen ("DEBUG_LEVEL")) == 0)
        {
            CONFIG.debug_level = strtoul (argv[1], NULL, 10);
        }

        /* credit scalar */
        else if (strncmp (argv[0], "CREDIT_SCALAR", strlen ("CREDIT_SCALAR")) == 0)
        {
            CONFIG.credit_scalar = strtoul (argv[1], NULL, 10);
        }

        /* prop/hello interval */
        else if (strncmp (argv[0], "HELLO_INTERVAL", strlen ("HELLO_INTERVAL")) == 0)
        {
            CONFIG.hello_interval = strtol (argv[1], NULL, 10);
        }
        else if (strncmp (argv[0], "PROP_INTERVAL", strlen ("PROP_INTERVAL")) == 0)
        {
            CONFIG.hello_interval = strtol (argv[1], NULL, 10);
        }

        /* sinr min */
        else if (strncmp (argv[0], "SINR_MIN", strlen ("SINR_MIN")) == 0)
        {
            CONFIG.sinr_min = strtof (argv[1], NULL);
        }

        /* sinr min */
        else if (strncmp (argv[0], "SINR_MAX", strlen ("SINR_MAX")) == 0)
        {
            CONFIG.sinr_max = strtof (argv[1], NULL);
        }

        /* use virtual interface */
        else if (strncmp (argv[0], "VIF_MODE", strlen ("VIF_MODE")) == 0)
        {
            CONFIG.vif_mode = strtoul (argv[1], NULL, 10);
        }

        /* use platform endpoint */
        else if (strncmp (argv[0], "PLATFORM_ENDPOINT", strlen ("PLATFORM_ENDPOINT")) == 0)
        {
            memset  (CONFIG.platform_endpoint, 0, sizeof(CONFIG.platform_endpoint));
            strncpy (CONFIG.platform_endpoint, argv[1], sizeof(CONFIG.platform_endpoint)-1);
        }

        /* use transport endpoint */
        else if (strncmp (argv[0], "TRANSPORT_ENDPOINT", strlen ("TRANSPORT_ENDPOINT")) == 0)
        {
            memset  (CONFIG.transport_endpoint, 0, sizeof(CONFIG.transport_endpoint));
            strncpy (CONFIG.transport_endpoint, argv[1], sizeof(CONFIG.transport_endpoint)-1);
        }

        /* pppoe binary path */
        else if (strncmp (argv[0], "PPPOE_BINARY_PATH", strlen ("PPPOE_BINARY_PATH")) == 0)
        {
            memset (CONFIG.pppoe_binary_path, 0, sizeof(CONFIG.pppoe_binary_path));
            strncpy (CONFIG.pppoe_binary_path, argv[1], sizeof(CONFIG.pppoe_binary_path)-1);
        }

        /* grant */
        else if (strncmp (argv[0], "CREDIT_GRANT", strlen ("CREDIT_GRANT")) == 0)
        {
            CONFIG.credit_grant = strtol (argv[1], NULL, 10);
        }

        /* p2p mode */
        else if (strncmp (argv[0], "P2P_MODE", strlen ("P2P_MODE")) == 0)
        {
            CONFIG.p2p_mode = strtol (argv[1], NULL, 10);
        }

        /* lcp echo pong */
        else if (strncmp (argv[0], "LCP_ECHO_PONG_MODE", strlen ("LCP_ECHO_PONG_MODE")) == 0)
        {
            CONFIG.lcp_echo_pong_mode = strtol (argv[1], NULL, 10);
        }

        /* credit dist mode */
        else if (strncmp (argv[0], "CREDIT_DIST_MODE", strlen ("CREDIT_DIST_MODE")) == 0)
        {
            CONFIG.credit_dist_mode = strtol (argv[1], NULL, 10);
        }

        /* flow control mode */
        else if (strncmp (argv[0], "FLOW_CONTROL_ENABLED", strlen ("FLOW_CONTROL_ENABLED")) == 0)
        {
            CONFIG.flow_control_enabled = strtol (argv[1], NULL, 10);
        }

        /* credit threshold */
        else if (strncmp (argv[0], "CREDIT_THRESHOLD", strlen ("CREDIT_THRESHOLD")) == 0)
        {
            CONFIG.credit_threshold = strtof (argv[1], NULL);

            if(CONFIG.credit_threshold < 0.0f || CONFIG.credit_threshold > 1.0f)
            {
                LOGGER(LOG_ERR,"CREDIT_THRESHOLD of %f is invalid, must be [0.0 - 1.0]\n",
                       CONFIG.credit_threshold);
                return (-1);
            }
        }

        /* session timeout */
        else if (strncmp (argv[0], "SESSION_TIMEOUT", strlen ("SESSION_TIMEOUT")) == 0)
        {
            CONFIG.session_timeout = strtol (argv[1], NULL, 10);
        }

        else
        {
            LOGGER(LOG_ERR," Unknown config file item (%s) \n", argv[0]);
            return (-1);
        }
    }

    fclose (fp);

    if(CONFIG.sinr_max <= CONFIG.sinr_min)
    {
        LOGGER(LOG_ERR,"sinr max %f, must be larger than sinr min %f \n",
                        CONFIG.sinr_max, CONFIG.sinr_min);

        return (-1);
    }

    return 0;
}

/*
 * This function returns the interface_name
 *
 * return:
 *   interface_name
 */
char *
rfc4938_config_get_iface (void)
{
    return CONFIG.interface_name;
}

/*
 * This function returns the service name
 *
 * return:
 *   service name
 */
char *
rfc4938_config_get_service_name (void)
{
    return CONFIG.service_name;
}

/*
 * This function returns the max_nbrs
 *
 * return:
 *   max_nbrs
 */
UINT16_t
rfc4938_config_get_max_nbrs (void)
{
    return CONFIG.max_nbrs;
}

/*
 * This function returns the client_port
 *
 * return:
 *   rfc4938client_port
 */
UINT16_t
rfc4938_config_get_client_port (void)
{
    return CONFIG.client_port;
}


/*
 * This function returns the initial grant
 *
 * return:
 *   initial grant
 */
UINT16_t
rfc4938_config_get_credit_grant (void)
{
    return CONFIG.credit_grant;
}


/*
 * This function returns our id
 *
 * return:
 *   initial grant
 */
UINT32_t
rfc4938_config_get_node_id (void)
{
    return CONFIG.node_id;
}



/*
 * This function returns the virtual interface mode
 *
 * return:
 *   initial grant
 */
int
rfc4938_config_get_vif_mode (void)
{
    return CONFIG.vif_mode;
}

/*
 * This function returns the ctl_port
 *
 * return:
 *   ctl_port
 */
UINT16_t
rfc4938_config_get_ctl_port (void)
{
    return CONFIG.ctl_port;
}

/*
 * This function returns the debug level
 *
 * return:
 *   debug_level
 */
UINT16_t
rfc4938_config_get_debug_level (void)
{
    return CONFIG.debug_level;
}


/*
 * This function returns the credit scalar
 *
 * return:
 *   credit_scalar
 */
UINT16_t
rfc4938_config_get_credit_scalar (void)
{
    return CONFIG.credit_scalar;
}



/*
 * This function returns the platform endpoint
 *
 * return:
 *   platformendpoint
 */
char *
rfc4938_config_get_platform_endpoint (void)
{
    return CONFIG.platform_endpoint;
}


/*
 * This function returns the transport endpoint
 *
 * return:
 *   transportndpoint
 */
char *
rfc4938_config_get_transport_endpoint (void)
{
    return CONFIG.transport_endpoint;
}


char *
rfc4938_config_get_pppoe_binary_path (void)
{
    return CONFIG.pppoe_binary_path;
}


UINT16_t rfc4938_config_get_hello_interval (void)
{
    return CONFIG.hello_interval;
}


float rfc4938_config_get_sinr_min (void)
{
    return CONFIG.sinr_min;
}


float rfc4938_config_get_sinr_max (void)
{
    return CONFIG.sinr_max;
}


int rfc4938_config_get_p2p_mode (void)
{
    return CONFIG.p2p_mode;
}


UINT8_t * rfc4938_config_get_hwaddr (void)
{
    return CONFIG.hwaddr;
}


int rfc4938_config_get_credit_dist_mode (void)
{
    return CONFIG.credit_dist_mode;
}


int rfc4938_config_get_lcp_echo_pong_mode (void)
{
    return CONFIG.lcp_echo_pong_mode;
}


int rfc4938_config_is_flow_control_enabled (void)
{
    return CONFIG.flow_control_enabled;
}


UINT16_t rfc4938_config_get_session_timeout (void)
{
    return CONFIG.session_timeout;
}


float
rfc4938_config_get_credit_threshold (void)
{
    return CONFIG.credit_threshold;
}

UINT32_t rfc4938_config_get_id(UINT32_t id)
{
    if(CONFIG.p2p_mode)
    {
        return id;
    }
    else
    {
        return 0xffff;
    }
}

/* EOF */
