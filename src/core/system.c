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
#include <string.h>

#include <directfb.h>

#include <core/fusion/list.h>
                                   
#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/core_parts.h>
#include <core/layers.h>
#include <core/modules.h>
#include <core/palette.h>
#include <core/surfaces.h>
#include <core/system.h>

#include <gfx/convert.h>

#include <misc/conf.h>
#include <misc/mem.h>

DEFINE_MODULE_DIRECTORY( dfb_core_systems, "systems",
                         DFB_CORE_SYSTEM_ABI_VERSION );

typedef struct {
     CoreSystemInfo system_info;
} CoreSystemField;

DFB_CORE_PART( system, 0, sizeof(CoreSystemField) )

static CoreSystemField       *system_field  = NULL;

static ModuleEntry           *system_module = NULL;
static const CoreSystemFuncs *system_funcs  = NULL;
static CoreSystemInfo         system_info;
static void                  *system_data   = NULL;


DFBResult
dfb_system_lookup()
{
     FusionLink *l;

     dfb_modules_explore_directory( &dfb_core_systems );
     
     fusion_list_foreach( l, dfb_core_systems.entries ) {
          ModuleEntry           *module = (ModuleEntry*) l;
          const CoreSystemFuncs *funcs;

          funcs = dfb_module_ref( module );
          if (!funcs)
               continue;

          if (!system_module || (!dfb_config->system ||
              !strcasecmp( dfb_config->system, module->name )))
          {
               if (system_module)
                    dfb_module_unref( system_module );

               system_module = module;
               system_funcs  = funcs;
               
               funcs->GetSystemInfo( &system_info );
          }
          else
               dfb_module_unref( module );
     }

     if (!system_module) {
          ERRORMSG("DirectFB/core/system: No system found!\n");

          return DFB_NOIMPL;
     }

     return DFB_OK;
}

static DFBResult
dfb_system_initialize( void *data_local, void *data_shared )
{
     DFB_ASSERT( system_funcs != NULL );
     DFB_ASSERT( system_field == NULL );

     system_field = data_shared;

     system_field->system_info = system_info;

     return system_funcs->Initialize( &system_data );
}

static DFBResult
dfb_system_join( void *data_local, void *data_shared )
{
     DFB_ASSERT( system_funcs != NULL );
     DFB_ASSERT( system_field == NULL );

     system_field = data_shared;
     
     if (system_field->system_info.type != system_info.type ||
         strcmp( system_field->system_info.name, system_info.name ))
     {
          ERRORMSG( "DirectFB/core/system: "
                    "running system '%s' doesn't match system '%s'!\n",
                    system_field->system_info.name, system_info.name );

          system_field = NULL;

          return DFB_UNSUPPORTED;
     }
     
     if (system_field->system_info.version.major != system_info.version.major ||
         system_field->system_info.version.minor != system_info.version.minor)
     {
          ERRORMSG( "DirectFB/core/system: running system version '%d.%d' "
                    "doesn't match version '%d.%d'!\n",
                    system_field->system_info.version.major,
                    system_field->system_info.version.minor,
                    system_info.version.major,
                    system_info.version.minor );

          system_field = NULL;

          return DFB_UNSUPPORTED;
     }
     
     return system_funcs->Join( &system_data );
}

static DFBResult
dfb_system_shutdown( bool emergency )
{
     DFBResult ret;
     
     DFB_ASSERT( system_field != NULL );
     DFB_ASSERT( system_module != NULL );
     
     ret = system_funcs->Shutdown( emergency );

     dfb_module_unref( system_module );

     system_module = NULL;
     system_funcs  = NULL;
     system_field  = NULL;
     system_data   = NULL;

     return ret;
}

static DFBResult
dfb_system_leave( bool emergency )
{
     DFBResult ret;
     
     DFB_ASSERT( system_field != NULL );
     DFB_ASSERT( system_module != NULL );
     
     ret = system_funcs->Leave( emergency );

     dfb_module_unref( system_module );

     system_module = NULL;
     system_funcs  = NULL;
     system_field  = NULL;
     system_data   = NULL;

     return ret;
}

static DFBResult
dfb_system_suspend()
{
     DFB_ASSERT( system_funcs != NULL );
     DFB_ASSERT( system_field != NULL );

     return system_funcs->Suspend();
}

static DFBResult
dfb_system_resume()
{
     DFB_ASSERT( system_funcs != NULL );
     DFB_ASSERT( system_field != NULL );

     return system_funcs->Resume();
}

CoreSystemType
dfb_system_type()
{
     return system_info.type;
}

void *
dfb_system_data()
{
     return system_data;
}

volatile void *
dfb_system_map_mmio( unsigned int    offset,
                     int             length )
{
     DFB_ASSERT( system_funcs != NULL );

     return system_funcs->MapMMIO( offset, length );
}

void
dfb_system_unmap_mmio( volatile void  *addr,
                       int             length )
{
     DFB_ASSERT( system_funcs != NULL );

     system_funcs->UnmapMMIO( addr, length );
}

int
dfb_system_get_accelerator()
{
     DFB_ASSERT( system_funcs != NULL );

     return system_funcs->GetAccelerator();
}

VideoMode *
dfb_system_modes()
{
     DFB_ASSERT( system_funcs != NULL );

     return system_funcs->GetModes();
}

VideoMode *
dfb_system_current_mode()
{
     DFB_ASSERT( system_funcs != NULL );

     return system_funcs->GetCurrentMode();
}

DFBResult
dfb_system_thread_init()
{
     DFB_ASSERT( system_funcs != NULL );

     return system_funcs->ThreadInit();
}

bool
dfb_system_input_filter( InputDevice   *device,
                         DFBInputEvent *event )
{
     DFB_ASSERT( system_funcs != NULL );

     return system_funcs->InputFilter( device, event );
}

unsigned long
dfb_system_video_memory_physical( unsigned int offset )
{
     DFB_ASSERT( system_funcs != NULL );

     return system_funcs->VideoMemoryPhysical( offset );
}

void *
dfb_system_video_memory_virtual( unsigned int offset )
{
     DFB_ASSERT( system_funcs != NULL );

     return system_funcs->VideoMemoryVirtual( offset );
}

unsigned int
dfb_system_videoram_length()
{
     DFB_ASSERT( system_funcs != NULL );

     return system_funcs->VideoRamLength();
}

