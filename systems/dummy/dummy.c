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



#include <config.h>

#include <core/Task.h>
#include <core/DisplayTask.h>

#include <core/layers.h>
#include <core/screens.h>
#include <core/surface_allocation.h>
#include <core/surface_pool.h>

#include <misc/conf.h>


#define DUMMY_WIDTH  8
#define DUMMY_HEIGHT 8
#define DUMMY_FORMAT DSPF_ARGB


#include <core/core_system.h>

DFB_CORE_SYSTEM( dummy )


D_DEBUG_DOMAIN( Dummy_Display, "Dummy/Display", "DirectFB Dummy System Display" );
D_DEBUG_DOMAIN( Dummy_Layer,   "Dummy/Layer",   "DirectFB Dummy System Layer" );
D_DEBUG_DOMAIN( Dummy_Screen,  "Dummy/Screen",  "DirectFB Dummy System Screen" );
D_DEBUG_DOMAIN( Dummy_Display_PTS, "Dummy/Display/PTS", "DirectFB Dummy System Display PTS" );

/**********************************************************************************************************************/
/**********************************************************************************************************************/

static DirectThread    *dummy_display_thread;
static bool             dummy_display_thread_stop;
static DirectMutex      dummy_display_lock;
static DirectWaitQueue  dummy_display_wq;
static DirectLink      *dummy_display_list;
static DFB_DisplayTask *dummy_display_task;

typedef struct {
     DirectLink             link;

     int                    magic;

     CoreSurface           *surface;
     CoreSurfaceBuffer     *buffer;
     int                    index;
     DFB_DisplayTask       *task;

     long long              pts;

     CoreSurfaceBufferLock  lock;
} DummyDisplayBuffer;

/**********************************************************************************************************************/

static void *
dummy_display_loop( DirectThread *thread,
                    void         *ctx )
{
     while (!dummy_display_thread_stop) {
          DummyDisplayBuffer *request;

          direct_mutex_lock( &dummy_display_lock );

          while (!dummy_display_list) {
               direct_waitqueue_wait( &dummy_display_wq, &dummy_display_lock );

               if (dummy_display_thread_stop) {
                    direct_mutex_unlock( &dummy_display_lock );
                    goto out;
               }
          }

          request = (DummyDisplayBuffer*) dummy_display_list;
          D_MAGIC_ASSERT( request, DummyDisplayBuffer );

          direct_mutex_unlock( &dummy_display_lock );


          long long now = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );

          D_DEBUG_AT( Dummy_Display, "%s() <- request %p\n", __FUNCTION__, request );
          D_DEBUG_AT( Dummy_Display, "  -> surface %p\n", request->surface );
          D_DEBUG_AT( Dummy_Display, "  -> index   %d\n", request->index );
          D_DEBUG_AT( Dummy_Display, "  -> task    %p\n", request->task );
          D_DEBUG_AT( Dummy_Display, "  -> pts     %lldus (%lldus from now)\n", request->pts, request->pts - now );


          while (request->pts - now > 100) {
               long long delay = request->pts - now - 100;

               D_DEBUG_AT( Dummy_Display, "  -> sleeping for %lldus...\n", delay );

               direct_thread_sleep( delay );

               now = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );
          }

          D_DEBUG_AT( Dummy_Display_PTS, "  => display at %lldus (%lld from pts)\n", now, now - request->pts );

          dfb_surface_notify_display2( request->surface, request->index, request->task );

          if (direct_config_get_int_value( "dummy-layer-dump" ) && request->pts > 0) {
               static long long first;

               if (!first)
                    first = request->pts;

               char buf[100];

               snprintf( buf, sizeof(buf), "dfb_dummy_layer_%lu_%09lld", request->surface->resource_id, request->pts - first );

               D_DEBUG_AT( Dummy_Display_PTS, "  => dumping frame with pts %lld\n", request->pts );

               dfb_surface_buffer_dump_type_locked( request->buffer, buf, NULL, false, &request->lock );
          }


          direct_mutex_lock( &dummy_display_lock );

          if (dummy_display_task) {
               D_DEBUG_AT( Dummy_Display, "  <= done previous task %p (pts %lld, %lld to current)\n", dummy_display_task,
                           DisplayTask_GetPTS( dummy_display_task ), request->pts - DisplayTask_GetPTS( dummy_display_task ) );

               Task_Done( dummy_display_task );
          }

          dummy_display_task = request->task;

          direct_list_remove( &dummy_display_list, &request->link );

          direct_mutex_unlock( &dummy_display_lock );

          direct_waitqueue_broadcast( &dummy_display_wq );


          dfb_surface_buffer_unref( request->buffer );

          dfb_surface_unref( request->surface );

          D_MAGIC_CLEAR( request );

          D_FREE( request );
     }

out:
     D_DEBUG_AT( Dummy_Display, "%s() <- stop!\n", __FUNCTION__ );

     if (dummy_display_task)
          Task_Done( dummy_display_task );

     return NULL;
}

/**********************************************************************************************************************/

static DFBResult
dummyInitScreen( CoreScreen           *screen,
                 CoreGraphicsDevice   *device,
                 void                 *driver_data,
                 void                 *screen_data,
                 DFBScreenDescription *description )
{
     D_DEBUG_AT( Dummy_Screen, "%s( %p, %p )\n", __FUNCTION__, screen, device );

     description->caps = DSCCAPS_NONE;

     direct_snputs( description->name, "Dummy", DFB_SCREEN_DESC_NAME_LENGTH );

     dummy_display_thread = direct_thread_create( DTT_OUTPUT, dummy_display_loop, NULL, "Dummy Display" );

     return DFB_OK;
}

static DFBResult
dummyShutdownScreen( CoreScreen           *screen,
                     void                 *driver_data,
                     void                 *screen_data )
{
     D_DEBUG_AT( Dummy_Screen, "%s( %p )\n", __FUNCTION__, screen );

     direct_mutex_lock( &dummy_display_lock );

     dummy_display_thread_stop = true;

     direct_waitqueue_signal( &dummy_display_wq );

     direct_mutex_unlock( &dummy_display_lock );


     direct_thread_join( dummy_display_thread );
     direct_thread_destroy( dummy_display_thread );

     dummy_display_thread      = NULL;
     dummy_display_thread_stop = false;

     return DFB_OK;
}

static DFBResult
dummyGetScreenSize( CoreScreen *screen,
                    void       *driver_data,
                    void       *screen_data,
                    int        *ret_width,
                    int        *ret_height )
{
     *ret_width  = dfb_config->mode.width  ?: DUMMY_WIDTH;
     *ret_height = dfb_config->mode.height ?: DUMMY_HEIGHT;

     return DFB_OK;
}

static ScreenFuncs dummyScreenFuncs = {
     .InitScreen     = dummyInitScreen,
     .ShutdownScreen = dummyShutdownScreen,
     .GetScreenSize  = dummyGetScreenSize
};

/**********************************************************************************************************************/

static DFBResult
dummyDisplayRequest( CoreSurface           *surface,
                     CoreSurfaceBufferLock *lock )
{
     DFBResult           ret;
     DummyDisplayBuffer *request;

     D_DEBUG_AT( Dummy_Display, "%s( %p, %p )\n", __FUNCTION__, surface, lock );

     CORE_SURFACE_BUFFER_LOCK_ASSERT( lock );
     CORE_SURFACE_ALLOCATION_ASSERT( lock->allocation );

     CORE_SURFACE_ASSERT( surface );

     ret = dfb_surface_ref( surface );
     if (ret)
          return ret;

     CORE_SURFACE_BUFFER_ASSERT( lock->buffer );

     ret = dfb_surface_buffer_ref( lock->buffer );
     if (ret) {
          dfb_surface_unref( surface );
          return ret;
     }

     CORE_SURFACE_BUFFER_ASSERT( lock->buffer );

     /* Allocate new request */
     request = D_CALLOC( 1, sizeof(DummyDisplayBuffer) );
     if (!request) {
          dfb_surface_buffer_unref( lock->buffer );
          dfb_surface_unref( surface );
          return D_OOM();
     }

     CORE_SURFACE_ALLOCATION_ASSERT( lock->allocation );

     /* Initialize request */
     request->surface = surface;
     request->buffer  = lock->buffer;
     request->index   = lock->allocation->index;
     request->task    = lock->task;
     request->pts     = lock->task ? DisplayTask_GetPTS( lock->task ) : -1;
     request->lock    = *lock;

     D_MAGIC_SET( request, DummyDisplayBuffer );

     D_DEBUG_AT( Dummy_Display, "  -> %p\n", request );
     D_DEBUG_AT( Dummy_Display, "  -> index %d\n", request->index );
     D_DEBUG_AT( Dummy_Display, "  -> pts   %lld\n", request->pts );

     direct_mutex_lock( &dummy_display_lock );

     direct_list_append( &dummy_display_list, &request->link );

     direct_waitqueue_signal( &dummy_display_wq );

     direct_mutex_unlock( &dummy_display_lock );

     return DFB_OK;
}

/**********************************************************************************************************************/

static DFBResult
dummyInitLayer( CoreLayer                  *layer,
                void                       *driver_data,
                void                       *layer_data,
                DFBDisplayLayerDescription *description,
                DFBDisplayLayerConfig      *config,
                DFBColorAdjustment         *adjustment )
{
     description->type             = DLTF_GRAPHICS;
     description->caps             = DLCAPS_SURFACE;
     description->surface_caps     = DSCAPS_SYSTEMONLY;
     description->surface_accessor = CSAID_CPU;

     direct_snputs( description->name, "Dummy", DFB_DISPLAY_LAYER_DESC_NAME_LENGTH );


     config->flags       = DLCONF_WIDTH | DLCONF_HEIGHT | DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE;
     config->width       = dfb_config->mode.width  ?: DUMMY_WIDTH;
     config->height      = dfb_config->mode.height ?: DUMMY_HEIGHT;
     config->pixelformat = dfb_config->mode.format ?: DUMMY_FORMAT;
     config->buffermode  = DLBM_FRONTONLY;

     return DFB_OK;
}

static DFBResult
dummyTestRegion( CoreLayer                  *layer,
                 void                       *driver_data,
                 void                       *layer_data,
                 CoreLayerRegionConfig      *config,
                 CoreLayerRegionConfigFlags *ret_failed )
{
     if (ret_failed)
          *ret_failed = DLCONF_NONE;

     return DFB_OK;
}

static DFBResult
dummySetRegion( CoreLayer                  *layer,
                void                       *driver_data,
                void                       *layer_data,
                void                       *region_data,
                CoreLayerRegionConfig      *config,
                CoreLayerRegionConfigFlags  updated,
                CoreSurface                *surface,
                CorePalette                *palette,
                CoreSurfaceBufferLock      *left_lock,
                CoreSurfaceBufferLock      *right_lock )
{
     return DFB_OK;
}

static DFBResult
dummyRemoveRegion( CoreLayer *layer,
                   void      *driver_data,
                   void      *layer_data,
                   void      *region_data )
{
     direct_mutex_lock( &dummy_display_lock );

     while (dummy_display_list)
          direct_waitqueue_wait( &dummy_display_wq, &dummy_display_lock );

     if (dummy_display_task) {
          Task_Done( dummy_display_task );

          dummy_display_task = NULL;
     }

     direct_mutex_unlock( &dummy_display_lock );

     return DFB_OK;
}

static DFBResult
dummyFlipRegion( CoreLayer             *layer,
                 void                  *driver_data,
                 void                  *layer_data,
                 void                  *region_data,
                 CoreSurface           *surface,
                 DFBSurfaceFlipFlags    flags,
                 const DFBRegion       *left_update,
                 CoreSurfaceBufferLock *left_lock,
                 const DFBRegion       *right_update,
                 CoreSurfaceBufferLock *right_lock )
{
     dfb_surface_flip( surface, false );

     return dummyDisplayRequest( surface, left_lock );
}

static DFBResult
dummyUpdateRegion( CoreLayer             *layer,
                   void                  *driver_data,
                   void                  *layer_data,
                   void                  *region_data,
                   CoreSurface           *surface,
                   const DFBRegion       *left_update,
                   CoreSurfaceBufferLock *left_lock,
                   const DFBRegion       *right_update,
                   CoreSurfaceBufferLock *right_lock )
{
     return dummyDisplayRequest( surface, left_lock );
}

static DisplayLayerFuncs dummyLayerFuncs = {
     .InitLayer     = dummyInitLayer,
     .TestRegion    = dummyTestRegion,
     .SetRegion     = dummySetRegion,
     .RemoveRegion  = dummyRemoveRegion,
     .FlipRegion    = dummyFlipRegion,
     .UpdateRegion  = dummyUpdateRegion
};

/**********************************************************************************************************************/

static void
system_get_info( CoreSystemInfo *info )
{
     info->type = CORE_ANY;
     info->caps = CSCAPS_ACCELERATION |
                  CSCAPS_SYSMEM_EXTERNAL |
                  CSCAPS_DISPLAY_TASKS |
                  CSCAPS_NOTIFY_DISPLAY |
                  CSCAPS_DISPLAY_PTS;

     direct_snputs( info->name, "Dummy", DFB_CORE_SYSTEM_INFO_NAME_LENGTH );
}

static DFBResult
system_initialize( CoreDFB *core, void **ret_data )
{
     CoreScreen *screen = dfb_screens_register( NULL, NULL, &dummyScreenFuncs );
     CoreLayer  *layer  = dfb_layers_register( screen, NULL, &dummyLayerFuncs );

     (void) layer;

     direct_mutex_init( &dummy_display_lock );
     direct_waitqueue_init( &dummy_display_wq );

     return DFB_OK;
}

static DFBResult
system_join( CoreDFB *core, void **ret_data )
{
     CoreScreen *screen = dfb_screens_register( NULL, NULL, &dummyScreenFuncs );
     CoreLayer  *layer  = dfb_layers_register( screen, NULL, &dummyLayerFuncs );

     (void) layer;

     return DFB_OK;
}

static DFBResult
system_shutdown( bool emergency )
{
     direct_mutex_deinit( &dummy_display_lock );
     direct_waitqueue_deinit( &dummy_display_wq );

     return DFB_OK;
}

static DFBResult
system_leave( bool emergency )
{
     return DFB_OK;
}

static DFBResult
system_suspend( void )
{
     return DFB_OK;
}

static DFBResult
system_resume( void )
{
     return DFB_OK;
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
system_get_accelerator( void )
{
     return dfb_config->accelerator;
}

static VideoMode *
system_get_modes( void )
{
     return NULL;
}

static VideoMode *
system_get_current_mode( void )
{
     return NULL;
}

static DFBResult
system_thread_init( void )
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
system_videoram_length( void )
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
system_auxram_length( void )
{
     return 0;
}

static void
system_get_busid( int *ret_bus, int *ret_dev, int *ret_func )
{
}

static int
system_surface_data_size( void )
{
     return 0;
}

static void
system_surface_data_init( CoreSurface *surface, void *data )
{
}

static void
system_surface_data_destroy( CoreSurface *surface, void *data )
{
}

static void
system_get_deviceid( unsigned int *ret_vendor_id,
                     unsigned int *ret_device_id )
{
}

