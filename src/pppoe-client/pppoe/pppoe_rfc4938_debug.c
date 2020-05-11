/*-----------------------------------------------------------------------------/
 * project: rfc4938
 * file: pppoe_rfc4938_debug.c
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
#include "pppoe_rfc4938_debug.h"
#define PPPOE_DEFAULT_DEBUG ( 0 )

static UINT32_t pppoe_debugs = PPPOE_DEFAULT_DEBUG;

/*
 * Name
 *    void
 *    pppoe_set_debug_mask(UINT32_t mask)
 *
 * Description
 *    Sets specific debug flags
 *
 * Parameters
 *    mask       bit mask indicating which flags to manipulate.
 *
 * Returns
 *    none
 *
 * Notes
 *    To set all flags      pppoe_set_debug_mask( -1 )
 *
 *    Set all flags to 0    pppoe_set_debug_mask( 0 )
 *
 */
void
pppoe_set_debug_mask (UINT32_t mask)
{
    if (mask == 0)
    {
        pppoe_debugs = 0;
    }
    else
    {
        pppoe_debugs |= mask;
    }
    return;
}


/*
 * Name
 *     void
 *     pppoe_clear_debug_mask(UINT32_t mask)
 *
 * Description
 *     Clears specific debug flags
 *
 * Parameters
 *     mask               bit mask indicating which flags to manipulate.
 *
 * Returns
 *     none
 *
 * Notes
 *     To clear all flags
 *     Set all flags to 0
 *
 */
void
pppoe_clear_debug_mask (UINT32_t mask)
{
    if (mask == 0)
    {
        pppoe_debugs = 0;
    }
    else
    {
        pppoe_debugs &= ~mask;
    }
    return;
}


/*
 * Name
 *     boolean
 *     is_pppoe_debug_flag_set(UINT32_t flag)
 *
 * Description
 *     returns status of debug flag(s)
 *
 * Parameters
 *     mask               debug flag(s).
 *
 * Returns
 *     TRUE  flag(s) set
 *     FALSE flag(s) clear
 *
 */
int
is_pppoe_debug_flag_set (UINT32_t flag)
{
    return (((pppoe_debugs & flag) ? TRUE : FALSE));
}


/*
 * Name
 *     void
 *     pppoe_debug_all(boolean flag)
 *
 * Description
 *     This function should be used to set or clear all PPPOE
 *     debugs.
 *
 * Parameter
 *     flag              TRUE to set all debugs
 *                       FALSE to clear all debugs
 * Returns
 *     none
 *
 */
void
pppoe_debug_all (int flag)
{
    if (flag)
    {
        pppoe_debugs = PPPOE_ALL_DEBUG;
    }
    else
    {
        pppoe_debugs = PPPOE_DEFAULT_DEBUG;
    }
}
