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

#include <config.h>

#include <core/fusion/arena.h>
#include <core/fusion/shmalloc.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/core.h>
#include <core/core_parts.h>

#include <misc/mem.h>


DFBResult
dfb_core_part_initialize( CoreDFB  *core,
                          CorePart *core_part )
{
     DFBResult  ret;
     void      *local  = NULL;
     void      *shared = NULL;

     if (core_part->initialized) {
          BUG( core_part->name );
          return DFB_BUG;
     }

     DEBUGMSG( "DirectFB/CoreParts: "
               "Going to initialize '%s' core...\n", core_part->name );

     if (core_part->size_local)
          local = DFBCALLOC( 1, core_part->size_local );

     if (core_part->size_shared)
          shared = SHCALLOC( 1, core_part->size_shared );

     ret = core_part->Initialize( core, local, shared );
     if (ret) {
          ERRORMSG( "DirectFB/Core: Could not initialize '%s' core!\n"
                    "    --> %s\n", core_part->name,
                    DirectFBErrorString( ret ) );

          if (shared)
               SHFREE( shared );

          if (local)
               DFBFREE( local );

          return ret;
     }

     if (shared)
          fusion_arena_add_shared_field( dfb_core_arena( core ),
                                         core_part->name, shared );

     core_part->data_local  = local;
     core_part->data_shared = shared;
     core_part->initialized = true;

     return DFB_OK;
}

DFBResult
dfb_core_part_join( CoreDFB  *core,
                    CorePart *core_part )
{
     DFBResult  ret;
     void      *local  = NULL;
     void      *shared = NULL;

     if (core_part->initialized) {
          BUG( core_part->name );
          return DFB_BUG;
     }

     DEBUGMSG( "DirectFB/CoreParts: "
               "Going to join '%s' core...\n", core_part->name );

     if (core_part->size_shared &&
         fusion_arena_get_shared_field( dfb_core_arena( core ),
                                        core_part->name, &shared ))
          return DFB_FUSION;

     if (core_part->size_local)
          local = DFBCALLOC( 1, core_part->size_local );

     ret = core_part->Join( core, local, shared );
     if (ret) {
          ERRORMSG( "DirectFB/Core: Could not join '%s' core!\n"
                    "    --> %s\n", core_part->name,
                    DirectFBErrorString( ret ) );

          if (local)
               DFBFREE( local );

          return ret;
     }

     core_part->data_local  = local;
     core_part->data_shared = shared;
     core_part->initialized = true;

     return DFB_OK;
}

DFBResult
dfb_core_part_shutdown( CoreDFB  *core,
                        CorePart *core_part,
                        bool      emergency )
{
     DFBResult ret;

     if (!core_part->initialized)
          return DFB_OK;

     DEBUGMSG( "DirectFB/CoreParts: "
               "Going to shutdown '%s' core...\n", core_part->name );

     ret = core_part->Shutdown( core, emergency );
     if (ret)
          ERRORMSG( "DirectFB/Core: Could not shutdown '%s' core!\n"
                    "    --> %s\n", core_part->name,
                    DirectFBErrorString( ret ) );

     if (core_part->data_shared)
          SHFREE( core_part->data_shared );

     if (core_part->data_local)
          DFBFREE( core_part->data_local );

     core_part->data_local  = NULL;
     core_part->data_shared = NULL;
     core_part->initialized = false;

     return DFB_OK;
}

DFBResult
dfb_core_part_leave( CoreDFB  *core,
                     CorePart *core_part,
                     bool      emergency )
{
     DFBResult ret;

     if (!core_part->initialized)
          return DFB_OK;

     DEBUGMSG( "DirectFB/CoreParts: "
               "Going to leave '%s' core...\n", core_part->name );

     ret = core_part->Leave( core, emergency );
     if (ret)
          ERRORMSG( "DirectFB/Core: Could not leave '%s' core!\n"
                    "    --> %s\n", core_part->name,
                    DirectFBErrorString( ret ) );

     if (core_part->data_local)
          DFBFREE( core_part->data_local );

     core_part->data_local  = NULL;
     core_part->data_shared = NULL;
     core_part->initialized = false;

     return DFB_OK;
}

