/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

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
#include <fusion/shm/shm_internal.h>

#if FUSION_BUILD_MULTI
#include <sys/ioctl.h>
#include <linux/fusion.h>
#endif

#define FUSION_MAX_WORLDS     8

/***************************************
 *  Fusion internal type declarations  *
 ***************************************/

struct __Fusion_FusionWorldShared {
     int                  magic;

     int                  world_index;

     int                  world_abi;

     struct timeval       start_time;

     DirectLink          *arenas;
     FusionSkirmish       arenas_lock;

     FusionSkirmish       reactor_globals;

     FusionSHMShared      shm;

     FusionSHMPoolShared *main_pool;
};

struct __Fusion_FusionWorld {
     int                  magic;

     int                  refs;

     FusionWorldShared   *shared;

     int                  fusion_fd;
     FusionID             fusion_id;

     DirectThread        *dispatch_loop;

     /*
      * List of reactors with at least one local reaction attached.
      */
     DirectLink          *reactor_nodes;
     pthread_mutex_t      reactor_nodes_lock;

     FusionSHM            shm;

     FusionForkAction     fork_action;
};

/*******************************************
 *  Fusion internal function declarations  *
 *******************************************/

int      _fusion_fd( const FusionWorldShared *shared );
FusionID _fusion_id( const FusionWorldShared *shared );

FusionWorld *_fusion_world( const FusionWorldShared *shared );

/*
 * from reactor.c
 */
void _fusion_reactor_free_all       ( FusionWorld   *world );
void _fusion_reactor_process_message( FusionWorld   *world,
                                      int            reactor_id,
                                      const void    *msg_data );

/*
 * from call.c
 */
#if FUSION_BUILD_MULTI
void _fusion_call_process( FusionWorld       *world,
                           int                call_id,
                           FusionCallMessage *call );
#endif

/*
 * from shm.c
 */
#if FUSION_BUILD_MULTI
void _fusion_shmpool_process( FusionWorld          *world,
                              int                   pool_id,
                              FusionSHMPoolMessage *msg );
#endif

#endif

