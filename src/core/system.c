/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#include <stdio.h>

#include <directfb.h>

#include <core/fusion/list.h>
                                   
#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/layers.h>
#include <core/palette.h>
#include <core/surfaces.h>
#include <core/system.h>

#include <gfx/convert.h>

#include <misc/conf.h>
#include <misc/mem.h>

typedef struct {
     FusionLink       link;
     CoreSystemFuncs *funcs;
     int              abi_version;
} CoreSystemModule;

static FusionLink       *system_modules = NULL;

static CoreSystemModule *system_module  = NULL;
static CoreSystemFuncs  *system_funcs   = NULL;
static CoreSystemInfo    system_info;


void
dfb_system_register_module( CoreSystemFuncs *funcs )
{
     CoreSystemModule *module;

     module = calloc( 1, sizeof(CoreSystemModule) );

     module->funcs       = funcs;
     module->abi_version = funcs->GetAbiVersion();

     fusion_list_prepend( &system_modules, &module->link );
}


static DFBResult
lookup_system()
{
     FusionLink *l;

     fusion_list_foreach( l, system_modules ) {
          CoreSystemModule *module = (CoreSystemModule*) l;

          module->funcs->GetSystemInfo( &system_info );

          if (dfb_config->system && strcasecmp( dfb_config->system,
                                                system_info.name ))
               continue;
          
          system_module = module;
          system_funcs  = module->funcs;

          break;
     }

     if (!system_module) {
          ERRORMSG("DirectFB/core/system: No system found!\n");

          return DFB_NOIMPL;
     }

     return DFB_OK;
}

DFBResult
dfb_system_initialize()
{
     DFBResult ret;

     ret = lookup_system();
     if (ret)
          return ret;

     return system_funcs->Initialize();
}

DFBResult
dfb_system_join()
{
     DFBResult ret;

     ret = lookup_system();
     if (ret)
          return ret;

     return system_funcs->Join();
}

DFBResult
dfb_system_shutdown( bool emergency )
{
     if (system_funcs)
          return system_funcs->Shutdown( emergency );

     return DFB_OK;
}

DFBResult
dfb_system_leave( bool emergency )
{
     if (system_funcs)
          return system_funcs->Leave( emergency );

     return DFB_OK;
}

CoreSystemType
dfb_system_type()
{
     return system_info.type;
}

volatile void *
dfb_system_map_mmio( unsigned int    offset,
                     int             length )
{
     return system_funcs->MapMMIO( offset, length );
}

void
dfb_system_unmap_mmio( volatile void  *addr,
                       int             length )
{
     system_funcs->UnmapMMIO( addr, length );
}

int
dfb_system_get_accelerator()
{
     return system_funcs->GetAccelerator();
}

VideoMode *
dfb_system_modes()
{
     return system_funcs->GetModes();
}

VideoMode *
dfb_system_current_mode()
{
     return system_funcs->GetCurrentMode();
}

DFBResult
dfb_system_wait_vsync()
{
     return system_funcs->WaitVSync();
}

unsigned long
dfb_system_video_memory_physical( unsigned int offset )
{
     return system_funcs->VideoMemoryPhysical( offset );
}

void *
dfb_system_video_memory_virtual( unsigned int offset )
{
     return system_funcs->VideoMemoryVirtual( offset );
}

unsigned int
dfb_system_videoram_length()
{
     return system_funcs->VideoRamLength();
}

