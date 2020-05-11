/*-----------------------------------------------------------------------------/
 * project: rfc4938
 * file: rfc4938_io.c
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


#include <linux/if_tun.h>

#include <fcntl.h>
#include <semaphore.h>
#include <net/if.h>
#include <linux/if_packet.h>

#include "rfc4938.h"
#include "rfc4938_io.h"
#include "rfc4938_messages.h"
#include "rfc4938_debug.h"
#include "rfc4938_parser.h"
#include "rfc4938_config.h"
#include "rfc4938_transport.h"
#include "rfc4938_neighbor_manager.h"


static int rfc4938_io_read_frame(int fd);

static int rfc4938_io_send_frame_to_child(UINT16_t session_id, UINT16_t req_port, void * payload,
        UINT16_t proto, int payloadlen);


static int rfc4938_io_addto_readable(int fd, fd_set * fdset, int * maxfd);

static int rfc4938_io_getall_readable(fd_set * fdset);

static int rfc4938_io_txsock = -1;

static sem_t sem;

static int terminated = 0;

int rfc4938_io_signal_pipe[2];

int rfc4938_vif_fd  = -1;
int rfc4938_eth_sfd = -1;
int rfc4938_eth_dfd = -1;

static inline int rfc4938_io_is_fd_ready(int fd, fd_set * fdset)
{
    return ((fd >= 0) && (FD_ISSET(fd, fdset)));
}

void
rfc4938_io_forward_to_child (UINT32_t neighbor_id, const void *p2buffer, int buflen, rfc4938_neighbor_element_t *nbr)
{
    int retval;

    // check the p2p mode
    neighbor_id = rfc4938_config_get_id(neighbor_id);

    if (! nbr)
    {
        LOGGER(LOG_ERR,"(%u): error, unknown neighbor\n",
                      rfc4938_config_get_node_id ());
        return;
    }

    LOGGER(LOG_PKT,"(%u): forward msg from nbr %u, to child on port %hu, len %d\n",
                    rfc4938_config_get_node_id (), neighbor_id, nbr->child_port, buflen);

    retval = rfc4938_io_send_to_child (nbr->child_port, p2buffer, buflen);

    if(retval != buflen)
    {
        LOGGER(LOG_ERR,"(%u): failed to forward msg from nbr %u, to child on port %hu, len %d\n",
                      rfc4938_config_get_node_id (), neighbor_id, nbr->child_port, buflen);
    }
}


int
rfc4938_io_send_to_child (UINT16_t port, const void *p2buffer, int buflen)
{
    // the child pppoe process is always local
    int z = rfc4938_io_send_udp_packet (LOCALHOST, port, p2buffer, buflen);

    if (z < 0)
    {
        LOGGER(LOG_ERR,"(%u): error sending message to %s:%hu\n",
                             rfc4938_config_get_node_id (), LOCALHOST, port);
    }

    return z;
}


void
rfc4938_io_send_to_nbr (UINT32_t neighbor_id, UINT16_t credits, const void *p2buffer, int buflen)
{
    // check the p2p mode
    neighbor_id = rfc4938_config_get_id(neighbor_id);

    LOGGER(LOG_PKT,"(%u): send message to nbr %u, len %d\n",
                          rfc4938_config_get_node_id(), neighbor_id, buflen);

    rfc4938_transport_send (neighbor_id, credits, p2buffer, buflen);
}



int
rfc4938_io_send_udp_packet (char *ip, UINT16_t port, const void *p2buffer, int buflen)
{
    struct sockaddr_in dst_addr;
    socklen_t socklen;

    if (p2buffer == NULL)
    {
        LOGGER(LOG_ERR,"(%u): Attempted to send NULL p2buffer, drop\n",
                             rfc4938_config_get_node_id ());
        return (-1);
    }

    if (buflen <= 0)
    {
        LOGGER(LOG_ERR,"(%u): Attempted to send empty p2buffer, drop\n",
                             rfc4938_config_get_node_id ());
        return (-1);
    }

    memset (&dst_addr, 0, sizeof (dst_addr));
    dst_addr.sin_family      = AF_INET;
    dst_addr.sin_port        = htons (port);
    dst_addr.sin_addr.s_addr = inet_addr (ip);

    if (dst_addr.sin_addr.s_addr == INADDR_NONE)
    {
        LOGGER(LOG_ERR,"(%u): error, 0.0.0.0 is invalid dst addr.\n",
                        rfc4938_config_get_node_id ());
        return (-1);
    }

    if (dst_addr.sin_port == 0)
    {
        LOGGER(LOG_ERR,"(%u): error, port has not been established, drop.\n",
                      rfc4938_config_get_node_id ());
        return (-1);
    }

    socklen = sizeof (dst_addr);

    int z = sendto(rfc4938_io_txsock, p2buffer, buflen, 0, (struct sockaddr *) &dst_addr, socklen);

    if (z < 0)
    {
        LOGGER(LOG_ERR,"(%u): error sending packet: %s\n",
                        rfc4938_config_get_node_id (), strerror (errno));
    }
    else
    {
        LOGGER(LOG_PKT,"(%u): sent packet len %d to: %s:%d\n",
                        rfc4938_config_get_node_id (),
                        z, inet_ntoa(dst_addr.sin_addr), ntohs(dst_addr.sin_port));
    }

    return (z);
}


int
rfc4938_io_listen_for_messages (void)
{
    sem_init(&sem, 0, 0);

    if((rfc4938_io_txsock = rfc4938_io_get_udp_socket(rfc4938_config_get_client_port(), NULL)) < 0)
    {
        LOGGER(LOG_ERR,"(%u): could not open general ipc socket\n", rfc4938_config_get_node_id ());

        return -1;
    }

    while(! terminated)
    {
        int num_fd_ready = 0;

        fd_set readable;

        int max_fd = rfc4938_io_getall_readable(&readable);

        // block here
        while (! terminated)
        {
            num_fd_ready = select (max_fd + 1, &readable, NULL, NULL, NULL);

            if (num_fd_ready >= 0 || errno != EINTR)
            {
                break;
            }
        }

        if(num_fd_ready <= 0)
        {
            LOGGER(LOG_ERR,"(%u): error waiting on select(): %s\n", rfc4938_config_get_node_id(), strerror (errno));

            return (-1);
        }

        rfc4938_transport_notify_fd_ready(readable, num_fd_ready);

        sem_wait(&sem);
    }

    return 0;
}


int
rfc4938_io_get_messages (fd_set readable, int num_fd_ready)
{
    socklen_t socklen;
    struct sockaddr_in from_addr;

    UINT8_t recvbuf[MAX_SOCK_MSG_LEN];

    rfc4938_neighbor_element_t * nbr = rfc4938_neighbor_get_neighbor_head();

    while(nbr && num_fd_ready)
    {
        if(rfc4938_io_is_fd_ready(nbr->child_sock, &readable))
        {
            socklen = sizeof (from_addr);

            int z = recvfrom (nbr->child_sock, recvbuf, sizeof(recvbuf),
                              0, (struct sockaddr *) &from_addr, &socklen);

            if (z < 0)
            {
                LOGGER(LOG_ERR,"(%u): error receiving UDP packet: %s\n",
                                     rfc4938_config_get_node_id (), strerror (errno));

                sem_post(&sem);

                return (-1);
            }
            else
            {
                LOGGER(LOG_PKT,"(%u): recv UDP packet: (nbr %u, ses %hu), from %s:%hu, len %d\n",
                                      rfc4938_config_get_node_id (),
                                      nbr->neighbor_id,
                                      nbr->session_id,
                                      inet_ntoa (from_addr.sin_addr), htons (from_addr.sin_port), z);

                rfc4938_parser_parse_downstream_packet (recvbuf, z, nbr);

                --num_fd_ready;
            }
        }

        nbr = nbr->next;
    }

    if(num_fd_ready && rfc4938_io_is_fd_ready(rfc4938_io_signal_pipe[PIPE_RD_FD], &readable))
    {
        rfc4938_io_handle_signal_event();

        --num_fd_ready;
    }

    if(num_fd_ready && rfc4938_io_is_fd_ready(rfc4938_vif_fd, &readable))
    {
        LOGGER(LOG_PKT,"(%u): frame ready on vif\n", rfc4938_config_get_node_id ());

        rfc4938_io_read_frame(rfc4938_vif_fd);

        --num_fd_ready;
    }

    if(num_fd_ready && rfc4938_io_is_fd_ready(rfc4938_eth_dfd, &readable))
    {
        LOGGER(LOG_PKT,"(%u): frame ready on discovery socket\n", rfc4938_config_get_node_id ());

        rfc4938_io_read_frame(rfc4938_eth_dfd);

        --num_fd_ready;
    }

    if(num_fd_ready && rfc4938_io_is_fd_ready(rfc4938_eth_sfd, &readable))
    {
        LOGGER(LOG_PKT,"(%u): frame ready on session socket\n", rfc4938_config_get_node_id ());

        rfc4938_io_read_frame(rfc4938_eth_sfd);

        --num_fd_ready;
    }

    sem_post(&sem);

    return num_fd_ready;
}


void
rfc4938_io_handle_signal_event ()
{
    char signal_code = 0;

    if (read (rfc4938_io_signal_pipe[PIPE_RD_FD], &signal_code, sizeof(signal_code)) != sizeof(signal_code))
    {
        LOGGER(LOG_ERR,"rfc4938_io_handle_signal_event(): Could NOT read from pipe\n");

        return;
    }
    else
    {
        LOGGER(LOG_INFO, "rfc4938_io_handle_signal_event(): read signal code [%c] read from pipe\n", signal_code);
    }

    switch (signal_code)
    {
    /* child */
    case 'C':
    {
        rfc4938_neighbor_cleanup_children();
    }
    break;

    /* terminate */
    case 'T':
    {
        rfc4938_parser_cli_terminate_session (0, 0);

        terminated = 1;
    }
    break;

    /* deleted entry */
    case 'D':
    {
        // no op
    } break;
    }
}


int
rfc4938_io_open_vif (const char *ifname, UINT8_t * hwaddr)
{
    const char *path = "/dev/net/tun";
    int sock = -1;
    int fd   = -1;
    struct ifreq ifr;

    if ((fd = open (path, O_RDWR)) < 0)
    {
        LOGGER(LOG_ERR,"(%u): error open:%s %s\n", 
               rfc4938_config_get_node_id (), path, strerror (errno));

        return -1;
    }

    memset (&ifr, 0, sizeof (ifr));
    strncpy (ifr.ifr_name, ifname, IFNAMSIZ-1);
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;

    if (ioctl (fd, TUNSETIFF, &ifr) < 0)
    {
        LOGGER(LOG_ERR,"(%u): error ioctl(TUNSETIFF):%s %s\n",
               rfc4938_config_get_node_id (), path, strerror (errno));

        return -1;
    }

    if ((sock = socket (AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        LOGGER(LOG_ERR,"(%u): error socket:%s %s\n",
               rfc4938_config_get_node_id (), path, strerror (errno));

        return -1;
    }

    ifr.ifr_flags = IFF_UP;

    if (ioctl (sock, SIOCSIFFLAGS, &ifr) < 0)
    {
        LOGGER(LOG_ERR,"(%u): error ioctl(SIOCSIFFLAGS):%s %s\n",
              rfc4938_config_get_node_id (), path, strerror (errno));

        return -1;
    }

    if (hwaddr)
    {
        if (ioctl (fd, SIOCGIFHWADDR, &ifr) < 0)
        {
            LOGGER(LOG_ERR,"(%u): error ioctl(SIOCGIFHWADDR):%s %s\n",
                   rfc4938_config_get_node_id (), path, strerror (errno));

            return -1;
        }

        memcpy (hwaddr, ifr.ifr_hwaddr.sa_data, PPPOE_ETH_ALEN);
    }

    close (sock);

    return fd;
}


static int rfc4938_io_read_frame(int fd)
{
    UINT8_t recvbuf[MAX_SOCK_MSG_LEN];

    int result = read(fd, recvbuf, sizeof(recvbuf));

    PPPoEPacket *packet = (PPPoEPacket *) recvbuf;

    if(result < 0)
    {
        LOGGER(LOG_ERR,"(%u): error reading frame: %s\n",
               rfc4938_config_get_node_id (), strerror (errno));

        return -1;
    }
    else if(result < (ETH_OVERHEAD + PPPOE_OVERHEAD))
    {
        LOGGER(LOG_ERR,"(%u): len %d too short to be ppoe\n",
               rfc4938_config_get_node_id (), result);

        return 0;
    }
    else
    {
        UINT16_t proto = ntohs(packet->eth_hdr.proto);

        if((proto == ETH_PPPOE_DISCOVERY) || (proto == ETH_PPPOE_SESSION))
        {
            rfc4938_parse_ppp_packet(packet, result, "downstream");

            int sent = 0;
            int skip = 0;
            int err  = 0;

            UINT16_t session_id = ntohs(packet->pppoe_session);

            rfc4938_neighbor_element_t *nbr = rfc4938_neighbor_get_neighbor_head();

            while(nbr)
            {
                if((nbr->child_port > 0) &&
                        (((session_id != 0) && (nbr->session_id == session_id)) ||
                         ((nbr->session_id == 0) && (nbr->nbr_session_state >= READY))))
                {
                    if(rfc4938_io_send_frame_to_child (nbr->session_id, nbr->child_port, recvbuf, proto, result) > 0)
                    {
                        ++sent;
                    }
                    else
                    {
                        ++err;
                    }
                }
                else
                {
                    ++skip;
                }

                /* move to the next element */
                nbr = nbr->next;
            }

            LOGGER(LOG_PKT,"(%u): fwd frame to %d children, skipped %d, errors %d\n",
                   rfc4938_config_get_node_id (), sent, skip, err);
        }
        else
        {
            LOGGER(LOG_PKT,"(%u): recv non pppoe frame len %d, drop\n",
                   rfc4938_config_get_node_id (), result);
        }
    }

    return 0;
}


static int rfc4938_io_send_frame_to_child(UINT16_t session_id, UINT16_t port, void * payload, UINT16_t proto, int payloadlen)
{
    int retval;
    int buflen;
    void *p2buffer = NULL;

    p2buffer = malloc (SIZEOF_CTL_FRAME_DATA + payloadlen);

    if (p2buffer == NULL)
    {
        LOGGER(LOG_ERR,"(%u): unable to malloc p2buffer\n",
               rfc4938_config_get_node_id());

        return (-1);
    }

    if ((buflen = rfc4938_ctl_format_frame_data (u32seqnum,
                  session_id,
                  payloadlen,
                  proto,
                  payload,
                  p2buffer)) != (int) (SIZEOF_CTL_FRAME_DATA + payloadlen))
    {
        LOGGER(LOG_ERR,"(%u): unable to format message\n",
               rfc4938_config_get_node_id());

        free (p2buffer);

        return (-1);
    }

    LOGGER(LOG_PKT,"(%u): send frame to child seqnum %u, payload len %d, msg len %d",
           rfc4938_config_get_node_id(), u32seqnum, payloadlen, buflen);

    retval = rfc4938_io_send_to_child (port, p2buffer, buflen);

    if(retval == buflen)
    {
        ++u32seqnum;
    }
    else
    {
        LOGGER(LOG_ERR,"(%u): failed to send frame to child payload len %d, msg len %d\n",
               rfc4938_config_get_node_id(), payloadlen, buflen);
    }

    free (p2buffer);

    return retval;
}


int rfc4938_io_send_frame_to_device (const void *p2buffer, int buflen, UINT16_t proto)
{
    rfc4938_parse_ppp_packet(p2buffer, buflen, "upstream");

    if(rfc4938_config_get_vif_mode() == 1)
    {

        if(write(rfc4938_vif_fd, p2buffer, buflen) < 0)
        {
            LOGGER(LOG_ERR,"(%u): error writting to vif: %s\n",
                   rfc4938_config_get_node_id (), strerror (errno));

            return -1;
        }
        else
        {
            LOGGER(LOG_PKT,"(%u): wrote %d bytes to vif\n",
                   rfc4938_config_get_node_id (), buflen);

            return 0;
        }
    }
    else
    {
        if(proto == ETH_PPPOE_DISCOVERY)
        {
            if(write(rfc4938_eth_dfd, p2buffer, buflen) < 0)
            {
                LOGGER(LOG_ERR,"(%u): error writting to discovery socket: %s\n",
                       rfc4938_config_get_node_id (), strerror (errno));

                return -1;
            }
            else
            {
                LOGGER(LOG_PKT,"(%u): wrote %d bytes to discovery socket\n",
                       rfc4938_config_get_node_id (), buflen);

                return 0;
            }
        }
        else if(proto == ETH_PPPOE_SESSION)
        {
            if(write(rfc4938_eth_sfd, p2buffer, buflen) < 0)
            {
                LOGGER(LOG_ERR,"(%u): error writting to session socket: %s\n",
                       rfc4938_config_get_node_id (), strerror (errno));

                return -1;
            }
            else
            {
                LOGGER(LOG_PKT,"(%u): wrote %d bytes to session socket\n",
                       rfc4938_config_get_node_id (), buflen);

                return 0;
            }
        }
        else
        {
            LOGGER(LOG_ERR,"(%u): unsopported proto 0x%hx\n",
                   rfc4938_config_get_node_id (), proto);

            return -1;
        }
    }
}


int
rfc4938_io_open_interface (char const *ifname, UINT16_t type, unsigned char *hwaddr)
{
    int optval = 1;
    int fd;
    struct ifreq ifr;
    int domain, stype;

    struct sockaddr_ll sa;

    memset (&sa, 0, sizeof (sa));

    domain = PF_PACKET;
    stype = SOCK_RAW;

    if ((fd = socket (domain, stype, htons (type))) < 0)
    {
        LOGGER(LOG_ERR,"(%u): socket error  %s\n",
               rfc4938_config_get_node_id (), strerror (errno));

        return -1;
    }

    if (setsockopt (fd, SOL_SOCKET, SO_BROADCAST, &optval, sizeof (optval)) < 0)
    {
        LOGGER(LOG_ERR,"(%u): setsockopt error  %s\n",
               rfc4938_config_get_node_id (), strerror (errno));

        return -1;
    }

    /* Fill in hardware address */
    if (hwaddr)
    {
        strncpy (ifr.ifr_name, ifname, sizeof (ifr.ifr_name-1));
        if (ioctl (fd, SIOCGIFHWADDR, &ifr) < 0)
        {
            LOGGER(LOG_ERR,"(%u): ioctl error  %s\n",
                   rfc4938_config_get_node_id (), strerror (errno));

            return -1;
        }
        memcpy (hwaddr, ifr.ifr_hwaddr.sa_data, PPPOE_ETH_ALEN);
#ifdef ARPHRD_ETHER
        if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER)
        {
            LOGGER(LOG_ERR,"(%u): error %s\n",
                   rfc4938_config_get_node_id (), "interface is NOT ethernet");

            return -1;
        }
#endif
    }

    /* Sanity check on MTU */
    strncpy (ifr.ifr_name, ifname, sizeof (ifr.ifr_name-1));
    if (ioctl (fd, SIOCGIFMTU, &ifr) < 0)
    {
        LOGGER(LOG_ERR,"(%u): ioctl error  %s\n",
               rfc4938_config_get_node_id (), strerror (errno));

        return -1;
    }
    if (ifr.ifr_mtu < PPPOE_ETH_DATA_LEN)
    {
        LOGGER(LOG_ERR,"(%u): error MTU %u < %u\n",
               rfc4938_config_get_node_id(),
               ifr.ifr_mtu, PPPOE_ETH_DATA_LEN);

        return -1;
    }

    /* Get interface index */
    sa.sll_family = AF_PACKET;
    sa.sll_protocol = htons (type);

    strncpy (ifr.ifr_name, ifname, sizeof (ifr.ifr_name-1));
    if (ioctl (fd, SIOCGIFINDEX, &ifr) < 0)
    {
        LOGGER(LOG_ERR,"(%u): ioctl error  %s\n",
               rfc4938_config_get_node_id (), strerror (errno));

        return -1;
    }
    sa.sll_ifindex = ifr.ifr_ifindex;

    /* We're only interested in packets on specified interface */
    if (bind (fd, (struct sockaddr *) &sa, sizeof (sa)) < 0)
    {
        LOGGER(LOG_ERR,"(%u): bind error  %s\n",
               rfc4938_config_get_node_id (), strerror (errno));

        return -1;
    }

    return fd;
}



int rfc4938_io_get_udp_socket(int req_port, int * real_port)
{
    struct sockaddr_in addr;
    socklen_t socklen;
    int opt = 0;
    int sock = -1;

    if((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror ("socket()");

        return (-1);
    }

    socklen = sizeof(opt);

    getsockopt(sock, SOL_SOCKET, SO_RCVBUF, &opt, &socklen);

    LOGGER(LOG_INFO, "(%u): rx buff len %d bytes\n",
           rfc4938_config_get_node_id (), opt);

    memset (&addr, 0, sizeof (addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(req_port);
    addr.sin_addr.s_addr = inet_addr (LOCALHOST);


    LOGGER(LOG_INFO, "(%u): try to bind to %s:%d\n",
           rfc4938_config_get_node_id (), inet_ntoa(addr.sin_addr),req_port);

    socklen = sizeof (addr);

    if(bind (sock, (struct sockaddr *) &addr, socklen) < 0)
    {
        LOGGER(LOG_ERR,"(%u): unable to bind to address: %s\n",
               rfc4938_config_get_node_id (), strerror (errno));

        return (-1);
    }

    socklen = sizeof (addr);
    memset (&addr, 0, sizeof (addr));

    if(getsockname (sock, (struct sockaddr *) &addr, &socklen) < 0)
    {
        LOGGER(LOG_ERR,"(%u): unable to get sock info: %s\n",
               rfc4938_config_get_node_id (), strerror (errno));

        return (-1);
    }
    else
    {
        LOGGER(LOG_INFO, "(%u): sock info: fd %d, endpoint %s:%hu\n",
               rfc4938_config_get_node_id (),
               sock, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

        if(real_port)
        {
            *real_port = ntohs(addr.sin_port);
        }

        return sock;
    }
}


static int rfc4938_io_addto_readable(int fd, fd_set * fdset, int * maxfd)
{
    if(fd >= 0)
    {
        FD_SET (fd, fdset);

        if (fd > *maxfd)
        {
            *maxfd = fd;
        }

        return 1;
    }

    return 0;
}


static int rfc4938_io_getall_readable(fd_set * fdset)
{
    int maxfd = -1;

    FD_ZERO(fdset);

    /* signal event pipe */
    rfc4938_io_addto_readable(rfc4938_io_signal_pipe[PIPE_RD_FD], fdset, &maxfd);

    /* virtual interface */
    rfc4938_io_addto_readable(rfc4938_vif_fd, fdset, &maxfd);

    /* eth session interface */
    rfc4938_io_addto_readable(rfc4938_eth_sfd, fdset, &maxfd);

    /* eth discovery interface */
    rfc4938_io_addto_readable(rfc4938_eth_dfd, fdset, &maxfd);

    rfc4938_neighbor_element_t * nbr = rfc4938_neighbor_get_neighbor_head();

    while(nbr)
    {
        rfc4938_io_addto_readable(nbr->child_sock, fdset, &maxfd);

        nbr = nbr->next;
    }

    return maxfd;
}
