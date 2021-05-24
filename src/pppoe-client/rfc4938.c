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

#include "rfc4938.h"
#include "rfc4938_io.h"
#include "rfc4938_messages.h"
#include "rfc4938_parser.h"
#include "rfc4938_transport.h"
#include "rfc4938_config.h"
#include "rfc4938_io.h"

#include <sched.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <string.h>

/* rfc4938 usage information */
static char rfc4938_usage[] =
    "\n"
    "  usage: rfc4938client [options]\n"
    "\n"
    "  Options:\n"
    "      -h                : Show this help menu\n"
    "      -f (filepath)     : Override the default config file path\n"
    "      -Z (priority)     : sched priority default 1\n"
    "      -v                : Display version info\n" "\n"
    "      -D logpath        : log file path\n" "\n";

/* version information */
static char version[] =
    "\n"
    "\tversion: rfc4938client 1.07+emane\n"
    "\t      Copyright (C) 2007-2009 by Cisco Systems, Inc.\n" "\n";



static void rfc4938client_signal_handler (int signo);

const char * log_path = "/tmp";

FILE * LoggerFp = NULL;

/*
 * MAIN
 */
int
main (int argc, char *argv[])
{
    int c;
    int rfc4938_debug;
    char filename[MAXFILENAME] = {0};
    FILE *f;
    int priority = 1;

    signal (SIGCHLD, rfc4938client_signal_handler);
    signal (SIGINT,  rfc4938client_signal_handler);
    signal (SIGHUP,  rfc4938client_signal_handler);
    signal (SIGTERM, rfc4938client_signal_handler);
    signal (SIGTSTP, rfc4938client_signal_handler);

    /* read options */
    while ((c = getopt (argc, argv, "vhf:Z:D:")) != EOF)
    {
        switch (c)
        {
        case 'h':              /* usage */
            printf ("%s", rfc4938_usage);
            return 0;

        case 'v':              /* version */
            printf ("%s", version);
            return 0;

        case 'f':              /* config file */
            strncpy (filename, optarg, MAXFILENAME-1);
            break;

        case 'Z':              /* priority */
            priority = atoi(optarg);
            break;

        case 'D':
            log_path = strdup(optarg);
        break;

        default:
            return (-1);
        }
    }

#ifdef HAVE_SYSLOG_H
    openlog("rfc4938client", LOG_PID, LOG_DAEMON);
#else
    char buff[256] = {0};
    snprintf(buff, sizeof(buff), "%s/%s", log_path, "pppoe_client.log");

    if((LoggerFp = fopen(buff, "w")) == NULL)
      {
        fprintf(stderr, "could not open client log file '%s', %s\n", buff, strerror(errno));
      }
    else
      {
        LOGGER(LOG_INFO, "XXXXXXXXX BEGIN XXXXXXXX");
      }
#endif

    if (filename[0] == 0)
    {
        strncpy (filename, CONFIGPATH, MAXFILENAME);
    }

    /* lets see config errors */
    rfc4938_debug_set_mask (RFC4938_G_ERROR_DEBUG);

    /* read the config file */
    if (rfc4938_config_read_config_file (filename))
    {
        LOGGER(LOG_ERR,"error: reading config file %s\n", filename);

        return (-1);
    }

    if( rfc4938_config_get_node_id () == 0)
    {
        LOGGER(LOG_ERR,"error: could not find NODE_ID in connfig file %s\n", filename);

        return (-1);
    }

    if (pipe(rfc4938_io_signal_pipe) < 0)
    {
        LOGGER(LOG_ERR,"%s: Could NOT create pipe %s\n", argv[0], strerror(errno));

        return -1;
    }

    if (priority > 0)
    {
        struct sched_param sp;
        memset(&sp, 0x0, sizeof(sp));

        sp.sched_priority = 1;

        if(sched_setscheduler(0, SCHED_RR, &sp) < 0)
        {
            LOGGER(LOG_ERR,"%s: Could NOT set rt priority %d %s\n",
                   argv[0], priority, strerror(errno));
        }
    }

    /* Check to make sure the pppoe binary exists */
    f = fopen (rfc4938_config_get_pppoe_binary_path(), "r");
    if (f == NULL)
    {
        LOGGER(LOG_ERR,"(%u): error, pppoe binary %s doesn't exist\n",
               rfc4938_config_get_node_id (), 
               rfc4938_config_get_pppoe_binary_path());
        return (-1);
    }
    fclose (f);

    /* check if virtual interface is being used */
    if(rfc4938_config_get_vif_mode() == 1)
    {
        if((rfc4938_vif_fd = rfc4938_io_open_vif (rfc4938_config_get_iface(),
                             rfc4938_config_get_hwaddr())) < 0)
        {
            LOGGER(LOG_ERR,"(%u): error, could not open virtual interface %s\n",
                   rfc4938_config_get_node_id(),
                   rfc4938_config_get_iface());

            return (-1);
        }
        else
        {
            LOGGER(LOG_INFO,"(%u): opened virtual interface %s, hwaddr %s\n",
                   rfc4938_config_get_node_id(),
                   rfc4938_config_get_iface(),
                   ether_ntoa((struct ether_addr*)rfc4938_config_get_hwaddr()));
        }
    }
    else
    {
        if((rfc4938_eth_sfd = rfc4938_io_open_interface (rfc4938_config_get_iface(),
                              ETH_PPPOE_SESSION,
                              rfc4938_config_get_hwaddr())) < 0)
        {
            LOGGER(LOG_ERR,"(%u): error, could not open eth interface %s for session traffic\n",
                                 rfc4938_config_get_node_id(), rfc4938_config_get_iface());

            return (-1);
        }
        else if((rfc4938_eth_dfd = rfc4938_io_open_interface (rfc4938_config_get_iface(),
                                   ETH_PPPOE_DISCOVERY,
                                   rfc4938_config_get_hwaddr())) < 0)
        {
            LOGGER(LOG_ERR,"(%u): error, could not open eth interface %s for discovery traffic\n",
                                 rfc4938_config_get_node_id(), rfc4938_config_get_iface());

            return (-1);
        }

        else
        {
            LOGGER(LOG_INFO,"(%u): opened eth interface %s, hwaddr %s\n",
                   rfc4938_config_get_node_id(),
                   rfc4938_config_get_iface(),
                   ether_ntoa((struct ether_addr*)rfc4938_config_get_hwaddr()));
        }
    }


    if (strlen(rfc4938_config_get_platform_endpoint())  != 0 &&
            strlen(rfc4938_config_get_transport_endpoint()) != 0)
    {
        LOGGER(LOG_INFO,"(%u): setup transport platform %s, transport %s\n",
               rfc4938_config_get_node_id (),
               rfc4938_config_get_platform_endpoint (),
               rfc4938_config_get_transport_endpoint ());


        rfc4938_transport_setup (rfc4938_config_get_platform_endpoint (),
                                 rfc4938_config_get_transport_endpoint (),
                                 rfc4938_config_get_node_id ());

        rfc4938_transport_enable_hellos(rfc4938_config_get_hello_interval());
    }
    else
    {
        LOGGER(LOG_ERR,"(%u): no transport endpoints given\n",
                             rfc4938_config_get_node_id ());
        return (-1);
    }


    LOGGER(LOG_INFO,": CONFIG struct: \n");
    LOGGER(LOG_INFO,": IFACE: %s\n", rfc4938_config_get_iface ());
    LOGGER(LOG_INFO,": MAX_NBR: %u\n", rfc4938_config_get_max_nbrs ());
    LOGGER(LOG_INFO,": RFC4938 port: %hu\n", rfc4938_config_get_client_port ());
    LOGGER(LOG_INFO,": RFC4938CTL port: %hu\n", rfc4938_config_get_ctl_port ());
    LOGGER(LOG_INFO,": SERVICE_NAME: %s\n", rfc4938_config_get_service_name ());
    LOGGER(LOG_INFO,": DEBUG_LEVEL: %u\n", rfc4938_config_get_debug_level ());
    LOGGER(LOG_INFO,": CREDIT_GRANT: %d\n", rfc4938_config_get_credit_grant ());
    LOGGER(LOG_INFO,": USE_VIRTUAL_INTERFACE: %d\n", rfc4938_config_get_vif_mode ());
    LOGGER(LOG_INFO,": NODE_ID: %u\n", rfc4938_config_get_node_id ());

    /* so far so good, now unset debug levels */
    rfc4938_debug_set_mask (RFC4938_G_OFF);

    /* Initialize debugs */
    rfc4938_debug = rfc4938_config_get_debug_level ();

    /* setup runtime logs */
    if (rfc4938_debug >= 1)
    {
        rfc4938_debug_set_mask (RFC4938_G_ERROR_DEBUG);
    }
    if (rfc4938_debug >= 2)
    {
        rfc4938_debug_set_mask (RFC4938_G_EVENT_DEBUG);
    }
    if (rfc4938_debug >= 3)
    {
        rfc4938_debug_set_mask (RFC4938_G_PACKET_DEBUG);
    }

    verbose_level = rfc4938_debug;

    /* open a socket an listen for messages, blocking */
    rfc4938_io_listen_for_messages ();

    /* all done */
    rfc4938_transport_cleanup ();

    return 0;
}




void
rfc4938client_signal_handler (int signo)
{
    if (signo == SIGINT || signo == SIGHUP || signo == SIGTERM || signo == SIGTSTP)
    {
        if (write (rfc4938_io_signal_pipe[PIPE_WR_FD], "T", 1) < 0)
        {
            printf ("terminate_event_signal_handler(): Could NOT write to pipe\n");
        }
    }
    else if (signo == SIGCHLD)
    {
        if (write (rfc4938_io_signal_pipe[PIPE_WR_FD], "C", 1) < 0)
        {
            printf ("terminate_event_signal_handler(): Could NOT write to pipe\n");
        }
    }
}





/* EOF */
