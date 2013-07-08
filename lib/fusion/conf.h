/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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



#ifndef __FUSION__CONF_H__
#define __FUSION__CONF_H__


#include <fusion/types.h>

struct __Fusion_FusionConfig {
     char *tmpfs;             /* location of shm file */

     bool  debugshm;
     bool  madv_remove;
     bool  madv_remove_force;
     bool  force_slave;

     gid_t shmfile_gid;       /* group that owns shm file */     

     bool  secure_fusion;

     bool  defer_destructors;

     int   trace_ref;

     bool  fork_handler;

     unsigned int call_bin_max_num;
     unsigned int call_bin_max_data;
     pid_t        skirmish_warn_on_thread;
};

extern FusionConfig FUSION_API *fusion_config;

extern const char   FUSION_API *fusion_config_usage;


DirectResult        FUSION_API  fusion_config_set( const char *name, const char *value );


void __Fusion_conf_init( void );
void __Fusion_conf_deinit( void );

#endif

