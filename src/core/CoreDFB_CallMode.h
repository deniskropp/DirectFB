/*
   (c) Copyright 2001-2012  The world wide DirectFB Open Source Community (directfb.org)
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

#include <core/core.h>
#include <fusion/conf.h>
#include <fusion/fusion.h>
#include <misc/conf.h>

typedef enum {
     COREDFB_CALL_DENY,
     COREDFB_CALL_DIRECT,
     COREDFB_CALL_INDIRECT
} CoreDFBCallMode;

static __inline__ CoreDFBCallMode
CoreDFB_CallMode( CoreDFB *core )
{
#if FUSION_BUILD_MULTI
     if (dfb_config->call_nodirect) {
          if (dfb_core_is_master( core )) {
               DirectThread *self = direct_thread_self();

               if (self && fusion_dispatcher_tid( core->world ) == direct_thread_get_tid( self ))
                    return COREDFB_CALL_DIRECT;
          }

          return COREDFB_CALL_INDIRECT;
     }

     if (core->shutdown_tid && core->shutdown_tid != direct_gettid() && direct_gettid() != fusion_dispatcher_tid(core->world) && !Core_GetCalling()) {
          while (core_dfb)
               direct_thread_sleep(10000);

          return COREDFB_CALL_DENY;
     }

     if (dfb_core_is_master( core ) || !fusion_config->secure_fusion)
          return COREDFB_CALL_DIRECT;

     return COREDFB_CALL_INDIRECT;
#else
     return dfb_config->call_nodirect ? COREDFB_CALL_INDIRECT : COREDFB_CALL_DIRECT;
#endif
}

