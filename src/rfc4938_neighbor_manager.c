/*-----------------------------------------------------------------------------/
 * project: rfc4938
 * file: rfc4938_neighbor_id_manager.c
 * version: 1.0
 * date: October 21, 2007
 *
 * Copyright (c) 2013 - Adjacent Link, LLC, Bridgewater NJ
 * Copyright (C), 2007-2008 by Cisco Systems, Inc.
 *
 * ===========================
 *
 * These APIs are used to manage a pool of local neighbor_id numbers
 * that are associated with client instances.  Port numbers
 * are allocated for use and freed when the client instance
 * is torn down.
 *
 * The pool of neighbor_id numbers is a range from a base neighbor_id
 * through a max number of neighbor_ids.  The base neighbor_id is reserved
 * for the control process.  The (base+1) through the last
 * neighbor_id  number are associated with the clients.
 *
 * This implementation keeps it __very simple__, the allocation
 * scheme increments a working neighbor_id number pointer.  Once the
 * neighbor_id pointer reaches the last neighbor_id number, it wraps back around
 * to (base+1).  The user must validate the socket_open() return
 * to ensure success.
 *
 * It is possible to improve upon this simplicity by inserting
 * logic to dynamically track neighbor_id number allocation and free.
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
#include <stdlib.h>
#include <unistd.h>

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/ether.h>
#include "pppoe_types.h"

#include "rfc4938_io.h"
#include "rfc4938_parser.h"
#include "rfc4938_debug.h"
#include "rfc4938_config.h"
#include "rfc4938_messages.h"
#include "rfc4938_neighbor_manager.h"
#include "rfc4938_transport.h"

#define PPPOE_START_PORT ( 10000 )
#define MAXARGS          ( 20 )
#define OPTLEN           ( 4 ) /* length of an option, a space, and a \0 '-R ' */
#define PORTARG          ( 11 )


static unsigned short rfc4938_rolling_pppoe_port = PPPOE_START_PORT;

static int rcf4938_numChildren = 0;

/*
 * The list of neighbors
 */
static rfc4938_neighbor_element_t *neighbor_head = NULL;

rfc4938_neighbor_element_t * rfc4938_neighbor_get_neighbor_head()
{
  return neighbor_head;
}

/*
 * print the requested neighbor id
 *
 * Description:
 *     Prints the requested active neighbor info to stdout.
 *
 * Inputs:
 *     neighbor_id - id number of the neighbor to print
 *
 * Outputs:
 *     Neighbor ID, address, and port
 * Returns:
 *     void
 */
void
rfc4938_neighbor_print (UINT32_t neighbor_id)
{
  rfc4938_neighbor_element_t *nbr = neighbor_head;

  while (nbr)
    {
      if (nbr->neighbor_id == neighbor_id)
        {
           printf ("\nNeighbor ID %u remote pid %u, local pid %u, port %hu, session id %hu, last seqnum %u, num missed seqnum %u\n",
                   nbr->neighbor_id, 
                   nbr->neighbor_pid, 
                   nbr->child_pid, 
                   nbr->child_port,
                   nbr->session_id,
                   nbr->last_seqnum,
                   nbr->missed_seqnum);
            
           return;
        }

      /* move to the next element */
      nbr = nbr->next;
    }
  printf ("\n");

  return;
}


/*
 * print all active neighbors
 *
 * Description:
 *     Prints all active neighbor info to stdout.
 *
 * Inputs:
 *     void
 *
 * Outputs:
 *     Prints neighbor ID and ACTIVE/INACTIVE state
 *     
 * Returns:
 *     void
 *
 */
void
rfc4938_neighbor_print_all (void)
{
  rfc4938_neighbor_element_t *nbr = neighbor_head;

  printf ("Neighbor\t Active\n");
  while (nbr)
    {
       printf ("%u\t\t %s", nbr->neighbor_id, rfc4938_neighbor_status_to_string(nbr->nbr_session_state));

      /* move to the next element */
      nbr = nbr->next;
    }
  printf ("\n");

  return;
}


/*
 * print all active neighbors to a string
 *
 * Description:
 *     Prints all active neighbor info to a string.
 *
 * Inputs:
 *     dgram: pointer to string of length SHOWLEN
 *
 * Outputs:
 *     dgram: formated string with neighbor information
 *
 * Returns:
 *
 */
void
rfc4938_neighbor_print_all_string (char *dgram, size_t max_len)
{
  rfc4938_neighbor_element_t *nbr = neighbor_head;

  char tmp_str[LNLEN] = {0};

  int total_len = snprintf (dgram, max_len, "Neighbor\t Active\n");

  while (nbr && total_len < max_len)
    {
       int len = snprintf (tmp_str, LNLEN, "%u\t\t ", nbr->neighbor_id);
       strncat (dgram, tmp_str, max_len - total_len);

       total_len += len;

       len = snprintf (tmp_str, LNLEN, "%s\n", rfc4938_neighbor_status_to_string(nbr->nbr_session_state));
       strncat (dgram, tmp_str, max_len - total_len);

       total_len += len;

      /* move to the next element */
      nbr = nbr->next;
    }
  strncat (dgram, "\n", max_len - total_len);

  return;
}



/*
 * Neighbor query
 *
 * Description:
 *     Returns a copy of the neighbor data if active.
 *
 * Inputs:
 *     neighbor_id      Requested neighbor ID
 *     p2neighbor       Pointer to data to receive the data
 *
 * Outputs:
 *     p2neighbor       Updated with the copied data.
 *
 * Returns:
 *     SUCCESS
 *     ERANGE
 *     ENODEV
 */
int
rfc4938_neighbor_query (UINT32_t neighbor_id, rfc4938_neighbor_element_t * p2neighbor)
{
  if (p2neighbor == NULL)
    {
      return (-1);
    }

  rfc4938_neighbor_element_t *nbr = neighbor_head;

  while (nbr)
    {
      if (nbr->neighbor_id == neighbor_id)
        {
          memcpy (p2neighbor, nbr, sizeof (rfc4938_neighbor_element_t));

          return  0;
        }
      else
        {
          /* move to the next element */
          nbr = nbr->next;
        }
    }

  return -1;
}


/*
 * Neighbor pointer
 *
 * Description:
 *     Returns a pointer to the neighbor data.
 *
 * Inputs:
 *     neighbor_id      Requested neighbor ID
 *     p2neighbor       Pointer to receive the pointer
 *
 * Outputs:
 *     p2neighbor       Updated with the pointer.
 *
 * Returns:
 *     SUCCESS
 *     ERANGE
 *     ENODEV
 */
int
rfc4938_neighbor_pointer_by_nbr_id (UINT32_t neighbor_id, rfc4938_neighbor_element_t ** p2neighbor)
{
  if (p2neighbor == NULL)
    {
      return (-1);
    }

  rfc4938_neighbor_element_t *nbr = neighbor_head;

  while (nbr)
    {
      if (nbr->neighbor_id == neighbor_id)
        {

          *p2neighbor = nbr;

          return 0;
        }
      else
        {
          /* move to the next element */
          nbr = nbr->next;
        }
    }

  return -1;
}


/*
 * Neighbor pointer by pid
 *
 * Description:
 *     Returns a pointer to the neighbor data.
 *
 * Inputs:
 *     pid              Requested pid
 *     p2neighbor       Pointer to receive the pointer
 *
 * Outputs:
 *     p2neighbor       Updated with the pointer.
 *
 * Returns:
 *     SUCCESS
 *     ERANGE
 *     ENODEV
 */
int
rfc4938_neighbor_pointer_by_pid (pid_t pid, rfc4938_neighbor_element_t ** p2neighbor)
{
  if (p2neighbor == NULL)
    {
      return (-1);
    }

  rfc4938_neighbor_element_t *nbr = neighbor_head;

  while (nbr)
    {
      if ((pid_t)nbr->child_pid == pid)
        {
          *p2neighbor = nbr;

          return 0;
        }
      else
        {
          /* move to the next element */
          nbr = nbr->next;
        }
    }

  return -1;
}


int
rfc4938_neighbor_pointer_by_port (UINT16_t port, rfc4938_neighbor_element_t ** p2neighbor)
{
  if (p2neighbor == NULL)
    {
      return (-1);
    }

  rfc4938_neighbor_element_t *nbr = neighbor_head;

  while (nbr)
    {
      if ((pid_t)nbr->child_port == port)
        {
          *p2neighbor = nbr;

          return 0;
        }
      else
        {
          /* move to the next element */
          nbr = nbr->next;
        }
    }

  return -1;
}


int
rfc4938_neighbor_pointer_by_session_id (UINT16_t session_id, rfc4938_neighbor_element_t ** p2neighbor)
{
  if (p2neighbor == NULL)
    {
      return (-1);
    }

  rfc4938_neighbor_element_t *nbr = neighbor_head;

  while (nbr)
    {
      if (nbr->session_id == session_id)
        {

          *p2neighbor = nbr;

          return 0;
        }
      else
        {
          /* move to the next element */
          nbr = nbr->next;
        }
    }

  return -1;
}



/*
 * Neighbor toggle all
 *
 * Description:
 *     Changes state of all neighbors according to supplied function pointer
 *
 * Inputs:
 *     *pt2func         Function pointer to initiate or terminate
 *
 *
 * Returns:
 *     SUCCESS
 *     ERANGE
 *     ENODEV
 */
int
rfc4938_neighbor_toggle_all (void (*pt2func) (rfc4938_neighbor_element_t *, UINT16_t, UINT16_t),
                             UINT16_t cmdSRC)
{
  if (pt2func == NULL)
    {
      return (-1);
    }

  rfc4938_neighbor_element_t *nbr = neighbor_head;

  while (nbr)
    {
      pt2func (nbr, cmdSRC, 0);

      nbr = nbr->next;
    }

  return 0;
}


rfc4938_neighbor_element_t *
rfc4938_neighbor_init (UINT32_t neighbor_id)
{
  if(rcf4938_numChildren >= rfc4938_config_get_max_nbrs())
   {
      return NULL;
   }

  rfc4938_neighbor_element_t *curr = neighbor_head;

  while (curr)
    {
      if (curr->neighbor_id == neighbor_id)
        {
          return curr;
        }
      else
        {
          /* move to the next element */
          curr = curr->next;
        }
    }

  rfc4938_neighbor_element_t *nbr =  malloc (sizeof (rfc4938_neighbor_element_t));

  if (nbr == NULL)
    {
      RFC4938_DEBUG_ERROR ("%s:(%u): malloc error \n", 
                           __func__, rfc4938_config_get_node_id ());

      return NULL;
    }

   nbr->nbr_session_state = INACTIVE;
   nbr->neighbor_id   = neighbor_id;
   nbr->child_port    = 0;
   nbr->session_id    = 0;
   nbr->neighbor_pid  = 0;
   nbr->child_pid     = 0;
   nbr->last_seqnum   = 0;
   nbr->missed_seqnum = 0;
   nbr->child_sock    = -1;

   /* insert at the top */
   nbr->next = neighbor_head;

   neighbor_head = nbr;

   return nbr;
}


int
rfc4938_neighbor_release (UINT32_t neighbor_id)
{
  rfc4938_neighbor_element_t *curr = neighbor_head;
  rfc4938_neighbor_element_t *prev = NULL;

  while (curr)
    {
      if (curr->neighbor_id == neighbor_id)
        {
          if(prev == NULL)
           {
             neighbor_head = curr->next;
           }
          else
           {
             prev->next = curr->next;          
           }

          free(curr);

          return 0;
        }
      else
        {
          prev = curr;

          /* move to the next element */
          curr = curr->next;
        }
    }

  return -1;
}



void
rfc4938_neighbor_initiate_neighbor(UINT32_t neighbor_id,
                                   UINT32_t peer_pid, UINT16_t credit_scalar)
{
  int a;
  int i;
  pid_t pid;
  int   port;

  char *arguments[MAXARGS];

  rfc4938_neighbor_element_t * nbr = rfc4938_neighbor_init (neighbor_id);

  if (nbr == NULL)
    {
      RFC4938_DEBUG_ERROR ("%s:(%u): could not allocate neighbor %u\n",
                           __func__, rfc4938_config_get_node_id (), nbr->neighbor_id);
      return;
    }

  if (nbr->nbr_session_state == ACTIVE)
    {
      RFC4938_DEBUG_EVENT ("%s:(%u): neighbor %u already ACTIVE, not initiating new one\n",
                           __func__, rfc4938_config_get_node_id (), nbr->neighbor_id);
      return;
    }

  if (nbr->child_pid != 0)
    {
      RFC4938_DEBUG_EVENT ("%s:(%u): already have a pppoe process (%d), for neighbor %u,"
                           " not initiating new one\n", __func__, rfc4938_config_get_node_id (),
                           nbr->child_pid, nbr->neighbor_id);
      return;
    }

  if((nbr->child_sock = rfc4938_io_get_udp_socket(0, &port)) < 0)
    {
      RFC4938_DEBUG_ERROR ("%s:(%u): could get child sock for pid (%d), for neighbor %u\n", 
                           __func__, rfc4938_config_get_node_id (),
                           nbr->child_pid, nbr->neighbor_id);
      return;
    }
  else
    {
      RFC4938_DEBUG_EVENT ("%s:(%u): opened child sock (%d), port %hu, for neighbor %u\n", 
                           __func__, rfc4938_config_get_node_id (),
                           nbr->child_sock, nbr->child_port, nbr->neighbor_id);
    }


  /* build argument list */

  /* prevent rollover of port */
  if (rfc4938_rolling_pppoe_port == 0xFFFF)
    {
      rfc4938_rolling_pppoe_port = PPPOE_START_PORT;
    }
  else
    {
      rfc4938_rolling_pppoe_port++;
    }

  a = 0;

  arguments[a] = rfc4938_config_get_pppoe_binary_path();
  a++;

  /* set host unique tag to be used */
  arguments[a] = "-U";
  a++;

  /* set interface name */
  arguments[a] = malloc (sizeof (char) * (strlen (rfc4938_config_get_iface ()) + OPTLEN));
  if (arguments[a] == NULL)
    {
      RFC4938_DEBUG_ERROR ("%s:(%u): malloc error for iface name\n", __func__,
                           rfc4938_config_get_node_id ());
      return;
    }
  sprintf (arguments[a], "-I%s", rfc4938_config_get_iface ());
  a++;

  /* set service name */
  arguments[a] = malloc (sizeof (char) * (strlen (rfc4938_config_get_service_name ()) + OPTLEN));
  if (arguments[a] == NULL)
    {
      RFC4938_DEBUG_ERROR ("%s:(%u): malloc error for service name\n", __func__,
                           rfc4938_config_get_node_id ());
      return;
    }
  sprintf (arguments[a], "-S%s", rfc4938_config_get_service_name ());
  a++;

  /* set id of neighbor */
  arguments[a] = malloc (sizeof (char *) * PORTARG);
  if (arguments[a] == NULL)
    {
      RFC4938_DEBUG_ERROR ("%s:(%u): malloc error for nbr id arg\n", __func__,
                           rfc4938_config_get_node_id ());
      return;
    }
  sprintf (arguments[a], "-y%u", nbr->neighbor_id);
  a++;

  /* set id of parent */
  arguments[a] = malloc (sizeof (char *) * PORTARG);
  if (arguments[a] == NULL)
    {
      RFC4938_DEBUG_ERROR ("%s:(%u): malloc error for parent id arg\n", __func__,
                           rfc4938_config_get_node_id ());
      return;
    }
  sprintf (arguments[a], "-Y%u", rfc4938_config_get_node_id ());
  a++;

  /* set pid of neighbor */
  arguments[a] = malloc (sizeof (char *) * PORTARG);
  if (arguments[a] == NULL)
    {
      RFC4938_DEBUG_ERROR ("%s:(%u): malloc error for nbr pid arg\n", __func__,
                           rfc4938_config_get_node_id ());
      return;
    }
  sprintf (arguments[a], "-R%u", peer_pid);
  a++;

  /* set port for pppoe to listen to */
  arguments[a] = malloc (sizeof (char) * PORTARG);
  if (arguments[a] == NULL)
    {
      RFC4938_DEBUG_ERROR ("%s:(%u): malloc error for pppoe port arg\n", __func__,
                           rfc4938_config_get_node_id ());
      return;
    }
  sprintf (arguments[a], "-r%hu", rfc4938_rolling_pppoe_port);
  a++;

  /* set the scaling factor */
  arguments[a] = malloc (sizeof (char) * PORTARG);
  if (arguments[a] == NULL)
    {
      RFC4938_DEBUG_ERROR ("%s:(%u): malloc error for rfc4938 scalar arg\n", __func__,
                           rfc4938_config_get_node_id ());
      return;
    }
  sprintf (arguments[a], "-x%hu", credit_scalar);
  a++;

  /* set port for parent process */
  arguments[a] = malloc (sizeof (char) * PORTARG);
  if (arguments[a] == NULL)
    {
      RFC4938_DEBUG_ERROR ("%s:(%u): malloc error for rfc4938 port arg\n", __func__,
                           rfc4938_config_get_node_id ());
      return;
    }
  sprintf (arguments[a], "-c%u", port);
  a++;

  /* set debug level */
  arguments[a] = malloc (sizeof (char) * PORTARG);
  if (arguments[a] == NULL)
    {
      RFC4938_DEBUG_ERROR ("%s:(%u): malloc error for debug level arg\n", __func__,
                           rfc4938_config_get_node_id ());
      return;
    }
  sprintf (arguments[a], "-z%u", rfc4938_config_get_debug_level());
  a++;

  /* set credit grant amount */
  arguments[a] = malloc (sizeof (char) * PORTARG);
  if (arguments[a] == NULL)
    {
      RFC4938_DEBUG_ERROR ("%s:(%u): malloc error for credit grant amount arg\n", __func__,
                           rfc4938_config_get_node_id ());
      return;
    }
  sprintf (arguments[a], "-g%hu", rfc4938_config_get_credit_grant ());
  a++;

  /* set timeout */
  arguments[a] = malloc (sizeof (char) * PORTARG);
  if (arguments[a] == NULL)
    {
      RFC4938_DEBUG_ERROR ("%s:(%u): malloc error for session timeout arg\n", __func__,
                           rfc4938_config_get_node_id ());

      return;
    }
  sprintf (arguments[a], "-T%hu", rfc4938_config_get_session_timeout ());
  a++;

  arguments[a] = malloc (sizeof (char) * 
                  (strlen (ether_ntoa((struct ether_addr*)rfc4938_config_get_hwaddr())) + OPTLEN));
  if (arguments[a] == NULL)
    {
      RFC4938_DEBUG_ERROR ("%s:(%u): malloc error for eth addr mode arg\n", __func__,
                           rfc4938_config_get_node_id ());
      return;
    }
  sprintf (arguments[a], "-E%s", ether_ntoa((struct ether_addr*)rfc4938_config_get_hwaddr()));
  a++;

  /* check if broadcast mode is enabled */
  if (! rfc4938_config_get_p2p_mode())
    {
      arguments[a] = malloc (sizeof (char) * PORTARG);
      if (arguments[a] == NULL)
        {
          RFC4938_DEBUG_ERROR ("%s:(%u): malloc error for broadcast mode arg\n", __func__,
                               rfc4938_config_get_node_id ());
          return;
        }
      sprintf (arguments[a], "-B");
      a++;
    }

  /* check if lcp echo pong mode is enabled */
  if (rfc4938_config_get_lcp_echo_pong_mode () != 0)
    {
      arguments[a] = malloc (sizeof (char) * PORTARG);
      if (arguments[a] == NULL)
        {
          RFC4938_DEBUG_ERROR ("%s:(%u): malloc error for lcp echo pong mode arg\n", __func__,
                               rfc4938_config_get_node_id ());
          return;
        }
      sprintf (arguments[a], "-L");
      a++;
    }


  arguments[a] = NULL;

  RFC4938_DEBUG_EVENT ("%s:(%u): Creating child pppoe session with params:\n", __func__,
                       rfc4938_config_get_node_id ());
  for (i = 0; i < a; i++)
    {
      RFC4938_DEBUG_EVENT (":%s\n", arguments[i]);
    }

  pid = fork ();

  switch (pid)
    {
    case 0:                    /* The child process */
      execvp (rfc4938_config_get_pppoe_binary_path(), arguments);

      break;

    case -1:                   /* error */
      RFC4938_DEBUG_ERROR ("%s:(%u): error, fork failed with error: %s for nbr %u\n", __func__,
                           rfc4938_config_get_node_id (), strerror (errno), nbr->neighbor_id);
      break;

    default:                   /* The parent process */
      /* save the pid */
      nbr->child_pid = pid;

      nbr->nbr_session_state = PENDING;

      ++rcf4938_numChildren;

      if(write (rfc4938_io_signal_pipe[PIPE_WR_FD], "N", 1) < 0)
       {
         RFC4938_DEBUG_ERROR ("%s:(%u): error, faled to write to signal pipe,  error: %s\n", __func__,
                              rfc4938_config_get_node_id (), strerror (errno));
       }

      RFC4938_DEBUG_EVENT ("%s:(%u): Child process created ID: %d for nbr %u, total children %d\n",
                           __func__, rfc4938_config_get_node_id (), nbr->child_pid, nbr->neighbor_id,
                           rcf4938_numChildren);
      break;
    }

  /* free argument string */
  for (i = 3; i < a; i++)
    {
      /* start at 3 since 0,1,2 are statics */
      if (arguments[i] != NULL)
        {
          free (arguments[i]);
        }
    }
}


void
rfc4938_neighbor_cleanup_children ()
{
  pid_t pid;
  int status;
  int count = 0;

  RFC4938_DEBUG_EVENT ("%s:(%u): checking our %d child(ren)\n",
                       __func__, rfc4938_config_get_node_id (), rcf4938_numChildren);

  while (1)
    {
      pid = waitpid (-1, &status, WNOHANG);

      if (pid > 0)
        {
          if (WIFEXITED (status) || WIFSIGNALED(status))
            {
              RFC4938_DEBUG_EVENT ("%s:(%u): pppoe child %d terminated %s\n",
                                   __func__, rfc4938_config_get_node_id (),
                                   pid, (WEXITSTATUS (status) == 0) ? "cleanly" : "unexpectedly");

              --rcf4938_numChildren;

              ++count;

              rfc4938_neighbor_element_t *nbr = NULL;

              if (rfc4938_neighbor_pointer_by_pid (pid, &nbr) == 0)
               {
                 rfc4938_neighbor_terminate_neighbor (nbr, CMD_SRC_SELF, 0);
               }
            }
        }
      else
        {
          break;
        }
    }

  RFC4938_DEBUG_EVENT ("%s:(%u): cleaned up %d child(ren)\n",
                       __func__, rfc4938_config_get_node_id (), count);
}


void
rfc4938_neighbor_terminate_neighbor (rfc4938_neighbor_element_t * nbr,
                                     UINT16_t cmdSRC, UINT16_t not_used __attribute__ ((unused)))
{
  int buflen;
  void *p2buffer = NULL;


  if (nbr == NULL)
    {
      return;
    }

  if(cmdSRC != CMD_SRC_SELF && 
     cmdSRC != CMD_SRC_CHILD)
   {
     if(nbr->nbr_session_state > PENDING)
       {
         p2buffer = malloc (SIZEOF_CTL_SESSION_STOP);

         if (p2buffer == NULL)
          {
            RFC4938_DEBUG_ERROR ("%s:(%u): malloc error \n", 
                           __func__, rfc4938_config_get_node_id ());

            return;
          }

         if ((buflen = rfc4938_ctl_format_session_stop (u32seqnum++, p2buffer)) != SIZEOF_CTL_SESSION_STOP)
          {
            RFC4938_DEBUG_ERROR ("%s:(%u): format error \n", 
                           __func__, rfc4938_config_get_node_id ());

            free (p2buffer);

            return;
          }

         RFC4938_DEBUG_EVENT ("%s:(%u): neighbor %u state is %s, sending termination to child, seqnum %u\n",
                               __func__, rfc4938_config_get_node_id (), 
                               nbr->neighbor_id, rfc4938_neighbor_status_to_string(nbr->nbr_session_state), u32seqnum - 1);


         rfc4938_io_send_to_child (nbr->child_port, p2buffer, buflen);

         free(p2buffer);
       }
      else
       {
         RFC4938_DEBUG_EVENT ("%s:(%u): neighbor %u state is %s, skip sending termination to child\n",
                             __func__, rfc4938_config_get_node_id (), 
                             nbr->neighbor_id, rfc4938_neighbor_status_to_string(nbr->nbr_session_state));
       }
    }

  if(cmdSRC == CMD_SRC_SELF ||
     cmdSRC == CMD_SRC_CLI  ||
     cmdSRC == CMD_SRC_PEER ||
     cmdSRC == CMD_SRC_CHILD)
    {
      RFC4938_DEBUG_EVENT ("%s:(%u): neighbor %u state is %s, sending termination to transport\n",
                               __func__, rfc4938_config_get_node_id (), 
                               nbr->neighbor_id, rfc4938_neighbor_status_to_string(nbr->nbr_session_state));

      rfc4938_transport_neighbor_terminated  (nbr->neighbor_id);
    }

  if(cmdSRC == CMD_SRC_SELF      ||
     cmdSRC == CMD_SRC_CLI       ||
     cmdSRC == CMD_SRC_TRANSPORT ||
     cmdSRC == CMD_SRC_CHILD)
    {
      p2buffer = malloc (SIZEOF_CTL_PEER_SESSION_TERMINATED);

      if (p2buffer == NULL)
       {
         RFC4938_DEBUG_ERROR ("%s:(%u): malloc error \n", 
                           __func__, rfc4938_config_get_node_id ());

         return;
       }

     if ((buflen = rfc4938_ctl_format_peer_session_terminated (u32seqnum++, 
                                                               rfc4938_config_get_node_id (),  // our id
                                                               p2buffer)) != SIZEOF_CTL_PEER_SESSION_TERMINATED)
      {
        RFC4938_DEBUG_ERROR ("%s:(%u): format error \n", 
                           __func__, rfc4938_config_get_node_id ());

        free (p2buffer);
 
        return;
      }

      RFC4938_DEBUG_EVENT ("%s:(%u): neighbor %u state is %s, sending termination to peer, seqnum %u\n",
                       __func__, rfc4938_config_get_node_id (), 
                       nbr->neighbor_id, rfc4938_neighbor_status_to_string(nbr->nbr_session_state), u32seqnum - 1);


     rfc4938_io_send_to_nbr (nbr->neighbor_id, 0, p2buffer, buflen);

     free (p2buffer);
   }

  nbr->nbr_session_state = INACTIVE;
  nbr->child_pid     = 0;
  nbr->child_port    = 0;
  nbr->session_id    = 0;
  nbr->neighbor_pid  = 0;
  nbr->last_seqnum   = 0;
  nbr->missed_seqnum = 0;

  if(nbr->child_sock >= 0)
   {
     close(nbr->child_sock);

     nbr->child_sock = -1;
   }

  if(write (rfc4938_io_signal_pipe[PIPE_WR_FD], "D", 1) < 0)
   {
     RFC4938_DEBUG_ERROR ("%s:(%u): error, faled to write to signal pipe,  error: %s\n", __func__,
                          rfc4938_config_get_node_id (), strerror (errno));
   }
  else
   {
     RFC4938_DEBUG_EVENT ("%s:(%u): wrote 'D' to signal pipe\n", __func__,
                          rfc4938_config_get_node_id ());
   }
}

const char * 
rfc4938_neighbor_status_to_string (rfc4938_neighbor_state_t state)
{
  switch (state)
    {
       case INACTIVE: 
        return "INACTIVE";

       case READY:    
        return "READY";

       case PENDING:  
        return "PENDING";

       case ACTIVE:   
        return "ACTIVE";

       default:       
        return "INVALID";
    }
}

rfc4938_neighbor_state_t 
rfc4938_get_neighbor_state (UINT32_t neighbor_id)
{
  rfc4938_neighbor_element_t *nbr = neighbor_head;

  while (nbr)
    {
      if (nbr->neighbor_id == neighbor_id)
        {
           RFC4938_DEBUG_EVENT ("%s:(%u): nbr %u state is %s\n",
                               __func__, rfc4938_config_get_node_id (), 
                               neighbor_id, rfc4938_neighbor_status_to_string(nbr->nbr_session_state));
            
           return  nbr->nbr_session_state;
        }

      /* move to the next element */
      nbr = nbr->next;
    }

  return INVALID;
}


