/*-----------------------------------------------------------------------------/
 * project: rfc4938
 * file: pppoe_rfc4938_debug.h
 * version: 1.0
 * date: October 4, 2007
 *
 * Copyright (c) 2013 - Adjacent Link, LLC, Bridgewater NJ
 * Copyright (C) 2007-2008, Cisco Systems, Inc.
 *
 * ===========================
 *
 * Debug definitions
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

#ifndef  __H_PPPOE_DEBUG_H__
#define  __H_PPPOE_DEBUG_H__

#include "../pppoe_types.h"
#include "stdio.h"

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

// #define PPPOE_PRINTF_ENABLED
#define PPPOE_DEBUG_ENABLED      //enables debugs
//#undef PPPOE_DEBUG_ENABLED     //disables all debugs

#define TRUE                   ( 1 )
#define FALSE                  ( 0 )

// debug bit positions in global debug mask
#define  PPPOE_ALL_DEBUG                     ( 0xffffffff)
#define  PPPOE_G_PACKET_DEBUG                ( 0x04 )
#define  PPPOE_G_EVENT_DEBUG                 ( 0x08 )
#define  PPPOE_G_ERROR_DEBUG                 ( 0x10 )

#if 0
#ifdef  PPPOE_PRINTF_ENABLED
#define PPPOE_DEBUG_PRINT(myargs...)  LOGGER(myargs);
#else
#define PPPOE_DEBUG_PRINT(myargs...)
#endif
/*
 *  macro packet debug
 */
#ifdef PPPOE_DEBUG_ENABLED
#define PPPOE_DEBUG_PACKET(args...)  \
        if (is_pppoe_debug_flag_set(     \
                PPPOE_G_PACKET_DEBUG)){ PPPOE_DEBUG_PRINT(args)}
#else
#define PPPOE_DEBUG_PACKET(args...)  ((void)0)
#endif

/*
 *  macro event debug
 */
#ifdef PPPOE_DEBUG_ENABLED
#define PPPOE_DEBUG_EVENT(args...)  \
        if (is_pppoe_debug_flag_set(     \
                PPPOE_G_EVENT_DEBUG)){ PPPOE_DEBUG_PRINT(args)}
#else
#define PPPOE_DEBUG_EVENT(args...)  ((void)0)
#endif


/*
 *  macro error debug
 */
#ifdef PPPOE_DEBUG_ENABLED
#define PPPOE_DEBUG_ERROR(args...)  \
        if (is_pppoe_debug_flag_set(     \
                PPPOE_G_ERROR_DEBUG)){ PPPOE_DEBUG_PRINT(args)}
#else
#define PPPOE_DEBUG_ERROR(args...)  ((void)0)
#endif
#endif

extern void pppoe_set_debug_mask (UINT32_t mask);
extern void pppoe_clear_debug_mask (UINT32_t mask);
extern int is_pppoe_debug_flag_set (UINT32_t flag);
extern void pppoe_debug_all (int flag);




#endif
