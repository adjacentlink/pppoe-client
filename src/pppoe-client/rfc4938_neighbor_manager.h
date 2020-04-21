/*-----------------------------------------------------------------------------/
 * project: rfc4938
 * file: rfc4938_neighbor_manager.h
 * version: 1.0
 * date: October 21, 2007
 *
 * Copyright (c) 2013 - Adjacent Link, LLC, Bridgewater NJ
 * Copyright (C), 2007-2008 by Cisco Systems, Inc.
 *
 * ===========================
 *
 * These APIs are used to manage a pool of local port numbers
 * that are associated with client instances.  Port numbers
 * are allocated for use and freed when the client instance
 * is torn down.
 *
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


#ifndef __NEIGHBOR_MANAGER_H__
#define __NEIGHBOR_MANAGER_H__

#include "rfc4938_types.h"
#include <unistd.h>

#ifndef LNLEN
#define LNLEN ( 100 )
#endif

/*
 * Neighbor states
 */
typedef enum {
    INVALID  = 0x0,
    INACTIVE = 0x1,
    PENDING  = 0x2,
    READY    = 0x3,
    ACTIVE   = 0x4,
} rfc4938_neighbor_state_t;

#define CMD_SRC_SELF       0x0
#define CMD_SRC_CLI        0x1
#define CMD_SRC_TRANSPORT  0x2
#define CMD_SRC_PEER       0x4
#define CMD_SRC_CHILD      0x8

/*
 * neighbor element
 */
typedef struct _rfc4938_neighbor_element_s {

    rfc4938_neighbor_state_t nbr_session_state;

    /*
     * neighbor that we're connected with
     */
    UINT32_t neighbor_id;
    UINT16_t child_port;     /* the child pppoe port    */
    UINT16_t session_id;     /* the session id          */
    UINT32_t neighbor_pid;   /* the nbr child pppoe pid */
    UINT32_t child_pid;      /* our child pppoe pid     */
    UINT32_t last_seqnum;    /* last rx seqnum          */
    UINT32_t missed_seqnum;  /* missed rx seqnum        */
    int      child_sock;     /* our child listen sock   */

    struct _rfc4938_neighbor_element_s *next;
} rfc4938_neighbor_element_t;


rfc4938_neighbor_element_t * rfc4938_neighbor_get_neighbor_head();

/*
 * functional prototypes
 */
void
rfc4938_neighbor_print(UINT32_t neighbor_id);

void
rfc4938_neighbor_print_all(void);

void
rfc4938_neighbor_print_all_string (char *dgram, size_t max);

int
rfc4938_neighbor_release(
           UINT32_t neighbor_id);

int
rfc4938_neighbor_query (
           UINT32_t neighbor_id,
           rfc4938_neighbor_element_t *p2neighbor);

int
rfc4938_neighbor_pointer_by_nbr_id (
           UINT32_t neighbor_id,
           rfc4938_neighbor_element_t **p2neighbor);

int
rfc4938_neighbor_pointer_by_session_id (
           UINT16_t session_id,
           rfc4938_neighbor_element_t **p2neighbor);


int
rfc4938_neighbor_pointer_by_pid (
           pid_t pid,
           rfc4938_neighbor_element_t **p2neighbor);

int
rfc4938_neighbor_pointer_by_port (
           UINT16_t port,
           rfc4938_neighbor_element_t **p2neighbor);



int
rfc4938_neighbor_toggle_all (
    void (*pt2func)(rfc4938_neighbor_element_t *, UINT16_t, UINT16_t),
    UINT16_t cmdSRC);

rfc4938_neighbor_element_t *
rfc4938_neighbor_init(UINT32_t neighbor_id);

void
rfc4938_neighbor_initiate_neighbor(UINT32_t neighbor_id, UINT32_t peer_pid, UINT16_t credit_scalar);

void rfc4938_neighbor_cleanup_children ();

void
rfc4938_neighbor_terminate_neighbor (rfc4938_neighbor_element_t * nbr, UINT16_t cmdSRC , UINT16_t not_used);

rfc4938_neighbor_state_t 
rfc4938_get_neighbor_state (UINT32_t neighbor_id);

const char * 
rfc4938_neighbor_status_to_string (rfc4938_neighbor_state_t state);

#endif

