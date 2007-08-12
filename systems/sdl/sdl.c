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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <directfb.h>

#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/thread.h>

#include <fusion/arena.h>
#include <fusion/shmalloc.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/layers.h>
#include <core/palette.h>
#include <core/surface.h>
#include <core/system.h>

#include <gfx/convert.h>

#include <misc/conf.h>

#include <SDL.h>

#include "sdl.h"
#include "primary.h"

#include <core/core_system.h>

DFB_CORE_SYSTEM( sdl )


DFBSDL  *dfb_sdl      = NULL;
CoreDFB *dfb_sdl_core = NULL;

extern const SurfacePoolFuncs sdlSurfacePoolFuncs;

static DFBResult dfb_fbdev_read_modes();


static void
system_get_info( CoreSystemInfo *info )
{
     info->type = CORE_SDL;
     info->caps = CSCAPS_ACCELERATION;

     snprintf( info->name, DFB_CORE_SYSTEM_INFO_NAME_LENGTH, "SDL" );
}

static DFBResult
system_initialize( CoreDFB *core, void **data )
{
     char       *driver;
     CoreScreen *screen;

     D_ASSERT( dfb_sdl == NULL );

     dfb_sdl = (DFBSDL*) SHCALLOC( dfb_core_shmpool(core), 1, sizeof(DFBSDL) );
     if (!dfb_sdl) {
          D_ERROR( "DirectFB/SDL: Couldn't allocate shared memory!\n" );
          return D_OOSHM();
     }

     dfb_sdl_core = core;

     dfb_fbdev_read_modes();  /* use same mode list as a fake */

     driver = getenv( "SDL_VIDEODRIVER" );
     if (driver && !strcasecmp( driver, "directfb" )) {
          D_INFO( "DirectFB/SDL: SDL_VIDEODRIVER is 'directfb', unsetting it.\n" );
          unsetenv( "SDL_VIDEODRIVER" );
     }

     /* Initialize SDL */
     if ( SDL_Init(SDL_INIT_VIDEO) < 0 ) {
          D_ERROR( "DirectFB/SDL: Couldn't initialize SDL: %s\n", SDL_GetError() );

          SHFREE( dfb_core_shmpool(core), dfb_sdl );
          dfb_sdl = NULL;

          return DFB_INIT;
     }

     fusion_skirmish_init( &dfb_sdl->lock, "SDL System", dfb_core_world(core) );

     fusion_call_init( &dfb_sdl->call, dfb_sdl_call_handler, NULL, dfb_core_world(core) );

     screen = dfb_screens_register( NULL, NULL, &sdlPrimaryScreenFuncs );

     dfb_layers_register( screen, NULL, &sdlPrimaryLayerFuncs );

     fusion_arena_add_shared_field( dfb_core_arena( core ), "sdl", dfb_sdl );

     dfb_surface_pool_initialize( core, &sdlSurfacePoolFuncs, &dfb_sdl->sdl_pool );

     *data = dfb_sdl;

     return DFB_OK;
}

static DFBResult
system_join( CoreDFB *core, void **data )
{
     void       *ret;
     CoreScreen *screen;

     D_ASSERT( dfb_sdl == NULL );

     fusion_arena_get_shared_field( dfb_core_arena( core ), "sdl", &ret );

     dfb_sdl = ret;
     dfb_sdl_core = core;

     screen = dfb_screens_register( NULL, NULL, &sdlPrimaryScreenFuncs );

     dfb_layers_register( screen, NULL, &sdlPrimaryLayerFuncs );

     dfb_surface_pool_join( core, dfb_sdl->sdl_pool, &sdlSurfacePoolFuncs );

     *data = dfb_sdl;

     return DFB_OK;
}

static DFBResult
system_shutdown( bool emergency )
{
     FusionSHMPoolShared *pool;

     D_ASSERT( dfb_sdl != NULL );

     /* Stop update thread. */
     if (dfb_sdl->update.thread) {
          if (!emergency) {
               dfb_sdl->update.quit = true;

               pthread_cond_signal( &dfb_sdl->update.cond );

               direct_thread_join( dfb_sdl->update.thread );
          }

          direct_thread_destroy( dfb_sdl->update.thread );
     }

     dfb_surface_pool_destroy( dfb_sdl->sdl_pool );

     fusion_call_destroy( &dfb_sdl->call );

     fusion_skirmish_prevail( &dfb_sdl->lock );

     SDL_Quit();

     fusion_skirmish_destroy( &dfb_sdl->lock );

     pool = dfb_core_shmpool(dfb_sdl_core);

     while (dfb_sdl->modes) {
          VideoMode *next = dfb_sdl->modes->next;

          SHFREE( pool, dfb_sdl->modes );

          dfb_sdl->modes = next;
     }

     SHFREE( pool, dfb_sdl );
     dfb_sdl = NULL;
     dfb_sdl_core = NULL;

     return DFB_OK;
}

static DFBResult
system_leave( bool emergency )
{
     D_ASSERT( dfb_sdl != NULL );

     dfb_sdl = NULL;
     dfb_sdl_core = NULL;

     return DFB_OK;
}

static DFBResult
system_suspend()
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
system_resume()
{
     return DFB_UNIMPLEMENTED;
}

static volatile void *
system_map_mmio( unsigned int    offset,
                 int             length )
{
    return NULL;
}

static void
system_unmap_mmio( volatile void  *addr,
                   int             length )
{
}

static int
system_get_accelerator()
{
     return -1;
}

static VideoMode *
system_get_modes()
{
     return dfb_sdl->modes;
}

static VideoMode *
system_get_current_mode()
{
     return NULL;
}

static DFBResult
system_thread_init()
{
     return DFB_OK;
}

static bool
system_input_filter( CoreInputDevice *device,
                     DFBInputEvent   *event )
{
     return false;
}

static unsigned long
system_video_memory_physical( unsigned int offset )
{
     return 0;
}

static void *
system_video_memory_virtual( unsigned int offset )
{
     return NULL;
}

static unsigned int
system_videoram_length()
{
     return 0;
}

static unsigned long
system_aux_memory_physical( unsigned int offset )
{
     return 0;
}

static void *
system_aux_memory_virtual( unsigned int offset )
{
     return NULL;
}

static unsigned int
system_auxram_length()
{
     return 0;
}

static void
system_get_busid( int *ret_bus, int *ret_dev, int *ret_func )
{
     return;
}

static void
system_get_deviceid( unsigned int *ret_vendor_id,
                     unsigned int *ret_device_id )
{
     return;
}


/*
 * parses video modes in /etc/fb.modes and stores them in dfb_fbdev->shared->modes
 * (to be replaced by DirectFB's own config system
 */
static DFBResult dfb_fbdev_read_modes()
{
     FILE *fp;
     char line[80],label[32],value[16];
     int geometry=0, timings=0;
     int dummy;
     VideoMode temp_mode;
     VideoMode *m = dfb_sdl->modes;

     if (!(fp = fopen("/etc/fb.modes","r")))
          return errno2result( errno );

     while (fgets(line,79,fp)) {
          if (sscanf(line, "mode \"%31[^\"]\"",label) == 1) {
               memset( &temp_mode, 0, sizeof(VideoMode) );
               geometry = 0;
               timings = 0;
               while (fgets(line,79,fp) && !(strstr(line,"endmode"))) {
                    if (5 == sscanf(line," geometry %d %d %d %d %d", &temp_mode.xres, &temp_mode.yres, &dummy, &dummy, &temp_mode.bpp)) {
                         geometry = 1;
                    }
                    else if (7 == sscanf(line," timings %d %d %d %d %d %d %d", &temp_mode.pixclock, &temp_mode.left_margin,  &temp_mode.right_margin,
                                         &temp_mode.upper_margin, &temp_mode.lower_margin, &temp_mode.hsync_len,    &temp_mode.vsync_len)) {
                         timings = 1;
                    }
                    else if (1 == sscanf(line, " hsync %15s",value) && 0 == strcasecmp(value,"high")) {
                         temp_mode.hsync_high = 1;
                    }
                    else if (1 == sscanf(line, " vsync %15s",value) && 0 == strcasecmp(value,"high")) {
                         temp_mode.vsync_high = 1;
                    }
                    else if (1 == sscanf(line, " csync %15s",value) && 0 == strcasecmp(value,"high")) {
                         temp_mode.csync_high = 1;
                    }
                    else if (1 == sscanf(line, " laced %15s",value) && 0 == strcasecmp(value,"true")) {
                         temp_mode.laced = 1;
                    }
                    else if (1 == sscanf(line, " double %15s",value) && 0 == strcasecmp(value,"true")) {
                         temp_mode.doubled = 1;
                    }
                    else if (1 == sscanf(line, " gsync %15s",value) && 0 == strcasecmp(value,"true")) {
                         temp_mode.sync_on_green = 1;
                    }
                    else if (1 == sscanf(line, " extsync %15s",value) && 0 == strcasecmp(value,"true")) {
                         temp_mode.external_sync = 1;
                    }
                    else if (1 == sscanf(line, " bcast %15s",value) && 0 == strcasecmp(value,"true")) {
                         temp_mode.broadcast = 1;
                    }
               }
               if (geometry && timings) {
                    if (!m) {
                         dfb_sdl->modes = SHCALLOC( dfb_core_shmpool(dfb_sdl_core), 1, sizeof(VideoMode) );
                         m = dfb_sdl->modes;
                    }
                    else {
                         m->next = SHCALLOC( dfb_core_shmpool(dfb_sdl_core), 1, sizeof(VideoMode) );
                         m = m->next;
                    }
                    direct_memcpy (m, &temp_mode, sizeof(VideoMode));
                    D_DEBUG( "DirectFB/FBDev: %20s %4dx%4d  %s%s\n", label, temp_mode.xres, temp_mode.yres,
                              temp_mode.laced ? "interlaced " : "", temp_mode.doubled ? "doublescan" : "" );
               }
          }
     }

     fclose (fp);

     return DFB_OK;
}

