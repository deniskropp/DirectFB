/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifndef __FUSION__FUSION_INTERNAL_H__
#define __FUSION__FUSION_INTERNAL_H__

#include <sys/types.h>
#include <sys/param.h>

#include <string.h>

#include <direct/list.h>

#include <fusion/build.h>
#include <fusion/fusion.h>
#include <fusion/lock.h>

#if FUSION_BUILD_MULTI
#include <sys/ioctl.h>
#include <linux/fusion.h>
#endif

/***************************************
 *  Fusion internal type declarations  *
 ***************************************/

typedef struct {
     int             abi_version;

     struct timeval  start_time;

     DirectLink     *arenas;
     FusionSkirmish  arenas_lock;
} FusionShared;

/*******************************************
 *  Fusion internal function declarations  *
 *******************************************/

/*
 * from fusion.c
 */
extern int _fusion_id;
extern int _fusion_fd;

extern FusionShared *_fusion_shared;

/*
 * from reactor.c
 */
void _fusion_reactor_free_all();
void _fusion_reactor_process_message( int fusion_reactor_id, const void *msg_data );

/*
 * from call.c
 */
#if FUSION_BUILD_MULTI
void _fusion_call_process( int call_id, FusionCallMessage *call );
#endif

#endif

