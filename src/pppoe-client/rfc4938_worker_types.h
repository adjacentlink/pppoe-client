/*-----------------------------------------------------------------------------/
 * project: rfc4938
 * file: rfc4938_worker_types.h
 * version: 1.0
 * date: Oct 21, 2013
 *
 * Copyright (c) 2013 - Adjacent Link, LLC, Bridgewater NJ
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


#ifndef RFC4938_WORKER_TYPES_H
#define RFC4938_WORKER_TYPES_H

const unsigned long WORK_ITEM_TYPE_NONE         = 0;
const unsigned long WORK_ITEM_TYPE_PADG_MSG     = 1;   // padg msg ready
const unsigned long WORK_ITEM_TYPE_PADI_MSG     = 2;   // padi msg ready
const unsigned long WORK_ITEM_TYPE_FLOWCTRL_MSG = 3;   // flow control
const unsigned long WORK_ITEM_TYPE_UPSTREAM_PKT = 4;   // upstream pkt ready
const unsigned long WORK_ITEM_TYPE_DNSTREAM_PKT = 5;   // downstream pkt ready
const unsigned long WORK_ITEM_TYPE_DEL_NBR      = 6;   // delete nbr
const unsigned long WORK_ITEM_TYPE_FD_READY     = 7;   // file desriptor ready
const unsigned long WORK_ITEM_TYPE_METRIC_MSG   = 8;   // nbr, queue and self metrics


inline const char * workIdToString(unsigned long id)
{
    switch(id)
    {
    case WORK_ITEM_TYPE_NONE:
        return "no_data";

    case WORK_ITEM_TYPE_PADG_MSG:
        return "padg_msg";

    case WORK_ITEM_TYPE_PADI_MSG:
        return "padi_msg";

    case WORK_ITEM_TYPE_FLOWCTRL_MSG:
        return "token_update";

    case WORK_ITEM_TYPE_METRIC_MSG:
        return "metric";

    case WORK_ITEM_TYPE_UPSTREAM_PKT:
        return "upstream_pkt";

    case WORK_ITEM_TYPE_DNSTREAM_PKT:
        return "downstream_pkt";

    case WORK_ITEM_TYPE_DEL_NBR:
        return "delete_nbr";

    case WORK_ITEM_TYPE_FD_READY:
        return "fd_ready";
    }

    return "unknown";
}
#endif
