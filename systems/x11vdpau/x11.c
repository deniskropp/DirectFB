/*
   (c) Copyright 2001-2010  The world wide DirectFB Open Source Community (directfb.org)
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

#include <fusion/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <directfb.h>

#include <fusion/fusion.h>
#include <fusion/shmalloc.h>
#include <fusion/lock.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/layers.h>
#include <core/layers_internal.h>
#include <core/palette.h>
#include <core/surface.h>
#include <core/system.h>

#include <gfx/convert.h>

#include <misc/conf.h>

#include <direct/messages.h>


#include "primary.h"
#include "x11.h"
#include "x11vdpau_surface_pool.h"

#include <core/core_system.h>

D_DEBUG_DOMAIN( X11_Core, "X11/Core", "Main X11 system functions" );

DFB_CORE_SYSTEM( x11vdpau )


static VideoMode modes[] = {
     { .xres =  320, .yres =  200 },
     { .xres =  320, .yres =  240 },
     { .xres =  512, .yres =  384 },
     { .xres =  640, .yres =  480 },
     { .xres =  768, .yres =  576 },

     { .xres = 1024, .yres =  576 },     // 16:9
     { .xres = 1024, .yres =  600 },     // Where does that mode come from? :-)
     { .xres = 1024, .yres =  768 },     // 4:3

     { .xres = 1280, .yres =  720 },     // 16:9
     { .xres = 1280, .yres =  960 },     // 4:3
     { .xres = 1280, .yres = 1024 },     // 5:4

     { .xres = 1440, .yres =  810 },     // 16:9
     { .xres = 1440, .yres = 1080 },     // 4:3

     { .xres = 1600, .yres =  900 },     // 16:9, obviously :)
     { .xres = 1600, .yres = 1200 },     // 4:3

     { .xres = 1920, .yres = 1080 },     // 16:9
     { .xres = 1920, .yres = 1200 },     // 16:10

     { .xres = 0, .yres = 0 }
};

/**********************************************************************************************************************/

static FusionCallHandlerResult X11_VDPAU_Dispatch( int           caller,
                                                   int           call_arg,
                                                   void         *call_ptr,
                                                   void         *ctx,
                                                   unsigned int  serial,
                                                   int          *ret_val );

/**********************************************************************************************************************/

static DFBResult
InitLocal( DFBX11 *x11, DFBX11Shared *shared, CoreDFB *core )
{
     int i, n, d;

     XInitThreads();

     x11->shared = shared;
     x11->core   = core;

     x11->display = XOpenDisplay(getenv("DISPLAY"));
     if (!x11->display) {
          D_ERROR("X11: Error in XOpenDisplay for '%s'\n", getenv("DISPLAY"));
          return DFB_INIT;
     }

     x11->screenptr = DefaultScreenOfDisplay(x11->display);
     x11->screennum = DefaultScreen(x11->display);
     d              = DefaultDepthOfScreen(x11->screenptr);

     for (i=0; i<x11->screenptr->ndepths; i++) {
          const Depth *depth = &x11->screenptr->depths[i];

          for (n=0; n<depth->nvisuals; n++) {
               Visual *visual = &depth->visuals[n];

               D_DEBUG_AT( X11_Core, "[Visual %d] ID 0x%02lx, depth %d, RGB 0x%06lx/0x%06lx/0x%06lx, %d bpRGB, %d entr.\n",
                        n, visual->visualid, depth->depth,
                        visual->red_mask, visual->green_mask, visual->blue_mask,
                        visual->bits_per_rgb, visual->map_entries );

               if (depth->depth != d)
                    continue;

               switch (depth->depth) {
                    case 32:
                         if (visual->red_mask   == 0xff0000 &&
                             visual->green_mask == 0x00ff00 &&
                             visual->blue_mask  == 0x0000ff &&
                             !x11->visuals[DFB_PIXELFORMAT_INDEX(DSPF_ARGB)])
                              x11->visuals[DFB_PIXELFORMAT_INDEX(DSPF_ARGB)] = visual;
                         break;

                    case 24:
                         if (visual->red_mask   == 0xff0000 &&
                             visual->green_mask == 0x00ff00 &&
                             visual->blue_mask  == 0x0000ff &&
                             !x11->visuals[DFB_PIXELFORMAT_INDEX(DSPF_RGB32)])
                              x11->visuals[DFB_PIXELFORMAT_INDEX(DSPF_RGB32)] = visual;
                         break;

                    case 16:
                         if (visual->red_mask   == 0xf800 &&
                             visual->green_mask == 0x07e0 &&
                             visual->blue_mask  == 0x001f &&
                             !x11->visuals[DFB_PIXELFORMAT_INDEX(DSPF_RGB16)])
                              x11->visuals[DFB_PIXELFORMAT_INDEX(DSPF_RGB16)] = visual;
                         break;

                    case 15:
                         if (visual->red_mask   == 0x7c00 &&
                             visual->green_mask == 0x03e0 &&
                             visual->blue_mask  == 0x001f &&
                             !x11->visuals[DFB_PIXELFORMAT_INDEX(DSPF_RGB555)])
                              x11->visuals[DFB_PIXELFORMAT_INDEX(DSPF_RGB555)] = visual;
                         break;
               }
          }
     }


     /*
      * VDPAU
      */
     VdpStatus status;

     status = vdp_device_create_x11( x11->display, x11->screennum, &x11->vdp.device, &x11->vdp.GetProcAddress );
     if (status) {
          D_ERROR( "DirectFB/X11/VDPAU: vdp_device_create_x11() failed (status %d)!\n", status );
          return DFB_INIT;
     }

     struct {
          VdpFuncId   name;
          void      **func;
     } funcs[] = {
          { VDP_FUNC_ID_GET_ERROR_STRING, (void**) &x11->vdp.GetErrorString },
          { VDP_FUNC_ID_GET_API_VERSION, (void**) &x11->vdp.GetApiVersion },
          { VDP_FUNC_ID_GET_INFORMATION_STRING, (void**) &x11->vdp.GetInformationString },
          { VDP_FUNC_ID_PRESENTATION_QUEUE_TARGET_CREATE_X11, (void**) &x11->vdp.PresentationQueueTargetCreateX11 },
          { VDP_FUNC_ID_PRESENTATION_QUEUE_TARGET_DESTROY, (void**) &x11->vdp.PresentationQueueTargetDestroy },
          { VDP_FUNC_ID_OUTPUT_SURFACE_CREATE, (void**) &x11->vdp.OutputSurfaceCreate },
          { VDP_FUNC_ID_OUTPUT_SURFACE_DESTROY, (void**) &x11->vdp.OutputSurfaceDestroy },
          { VDP_FUNC_ID_OUTPUT_SURFACE_GET_BITS_NATIVE, (void**) &x11->vdp.OutputSurfaceGetBitsNative },
          { VDP_FUNC_ID_OUTPUT_SURFACE_PUT_BITS_NATIVE, (void**) &x11->vdp.OutputSurfacePutBitsNative },
          { VDP_FUNC_ID_OUTPUT_SURFACE_RENDER_OUTPUT_SURFACE, (void**) &x11->vdp.OutputSurfaceRenderOutputSurface },
          { VDP_FUNC_ID_PRESENTATION_QUEUE_CREATE, (void**) &x11->vdp.PresentationQueueCreate },
          { VDP_FUNC_ID_PRESENTATION_QUEUE_DESTROY, (void**) &x11->vdp.PresentationQueueDestroy },
          { VDP_FUNC_ID_PRESENTATION_QUEUE_DISPLAY, (void**) &x11->vdp.PresentationQueueDisplay },
          { VDP_FUNC_ID_PRESENTATION_QUEUE_GET_TIME, (void**) &x11->vdp.PresentationQueueGetTime },
          { VDP_FUNC_ID_PRESENTATION_QUEUE_BLOCK_UNTIL_SURFACE_IDLE, (void**) &x11->vdp.PresentationQueueBlockUntilSurfaceIdle },
     };

     for (i=0; i<D_ARRAY_SIZE(funcs); i++) {
          status = x11->vdp.GetProcAddress( x11->vdp.device, funcs[i].name, funcs[i].func );
          if (status) {
               D_ERROR( "DirectFB/X11/VDPAU: GetProcAddress( %u ) failed (status %d, '%s')!\n", funcs[i].name, status,
                        (i > 0) ? x11->vdp.GetErrorString( status ) : "" );
               return DFB_INIT;
          }
     }

     x11->vdp.GetApiVersion( &x11->vdp.api_version );
     x11->vdp.GetInformationString( &x11->vdp.information_string );

     D_INFO( "DirectFB/X11/VDPAU: API version %d - '%s'\n", x11->vdp.api_version, x11->vdp.information_string );
     
     
     x11->screen = dfb_screens_register( NULL, x11, x11PrimaryScreenFuncs );

     dfb_layers_register( x11->screen, x11, x11PrimaryLayerFuncs );
     dfb_layers_register( x11->screen, x11, x11PrimaryLayerFuncs );
     dfb_layers_register( x11->screen, x11, x11PrimaryLayerFuncs );

     return DFB_OK;
}

/**********************************************************************************************************************/

static void
system_get_info( CoreSystemInfo *info )
{
     info->type = CORE_X11VDPAU;
     info->caps = CSCAPS_ACCELERATION | CSCAPS_PREFER_SHM;

     D_DEBUG_AT( X11_Core, "%s()\n", __FUNCTION__ );

     snprintf( info->name, DFB_CORE_SYSTEM_INFO_NAME_LENGTH, "X11" );
}

static DFBResult
system_initialize( CoreDFB *core, void **data )
{
     DFBResult     ret;
     DFBX11       *x11;
     DFBX11Shared *shared;

     D_DEBUG_AT( X11_Core, "%s()\n", __FUNCTION__ );

     x11 = D_CALLOC( 1, sizeof(DFBX11) );
     if (!x11)
          return D_OOM();

     shared = SHCALLOC( dfb_core_shmpool( core ), 1, sizeof(DFBX11Shared) );
     if (!shared) {
          D_FREE( x11 );
          return D_OOSHM();
     }

     /*
      * Local init (master and slave)
      */
     ret = InitLocal( x11, shared, core );
     if (ret) {
          SHFREE( dfb_core_shmpool( core ), shared );
          D_FREE( x11 );
          return ret;
     }


     /*
      * Shared init (master only)
      */
     shared->screen_size.w = dfb_config->mode.width  ?: x11->screenptr->width;
     shared->screen_size.h = dfb_config->mode.height ?: x11->screenptr->height;

     fusion_skirmish_init( &shared->lock, "X11 System", dfb_core_world(core) );

     fusion_call_init( &shared->call, X11_VDPAU_Dispatch, x11, dfb_core_world(core) );


     /*
      * Must be set before initializing the pools!
      */
     *data = x11;


     /*
      * Master init
      */
     dfb_surface_pool_initialize( core, x11vdpauSurfacePoolFuncs, &shared->vdpau_pool );

     core_arena_add_shared_field( core, "x11", shared );

     return DFB_OK;
}

static DFBResult
system_join( CoreDFB *core, void **data )
{
     DFBResult     ret;
     void         *ptr;
     DFBX11       *x11;
     DFBX11Shared *shared;

     D_DEBUG_AT( X11_Core, "%s()\n", __FUNCTION__ );

     x11 = D_CALLOC( 1, sizeof(DFBX11) );
     if (!x11)
          return D_OOM();

     core_arena_get_shared_field( core, "x11", &ptr );
     shared = ptr;


     /*
      * Local init (master and slave)
      */
     ret = InitLocal( x11, shared, core );
     if (ret) {
          D_FREE( x11 );
          return ret;
     }


     /*
      * Must be set before joining the pools!
      */
     *data = x11;


     /*
      * Slave init
      */
     if (shared->vdpau_pool)
          dfb_surface_pool_join( core, shared->vdpau_pool, x11vdpauSurfacePoolFuncs );

     return DFB_OK;
}

static DFBResult
system_shutdown( bool emergency )
{
     DFBX11       *x11    = dfb_system_data();
     DFBX11Shared *shared = x11->shared;

     D_DEBUG_AT( X11_Core, "%s()\n", __FUNCTION__ );

     /*
      * Master deinit
      */
     if (shared->vdpau_pool)
          dfb_surface_pool_destroy( shared->vdpau_pool );


     /*
      * Shared deinit (master only)
      */
     fusion_call_destroy( &shared->call );

     fusion_skirmish_prevail( &shared->lock );

     fusion_skirmish_destroy( &shared->lock );


     SHFREE( dfb_core_shmpool( x11->core ), shared );


     /*
      * Local deinit (master and slave)
      */
     if (x11->display)
          XCloseDisplay( x11->display );

     D_FREE( x11 );

     return DFB_OK;
}

static DFBResult
system_leave( bool emergency )
{
     DFBX11       *x11    = dfb_system_data();
     DFBX11Shared *shared = x11->shared;

     D_DEBUG_AT( X11_Core, "%s()\n", __FUNCTION__ );

     /*
      * Slave deinit
      */
     if (shared->vdpau_pool)
          dfb_surface_pool_leave( shared->vdpau_pool );


     /*
      * Local deinit (master and slave)
      */
     if (x11->display)
          XCloseDisplay( x11->display );

     D_FREE( x11 );

     return DFB_OK;
}

static DFBResult
system_suspend( void )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
system_resume( void )
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
system_get_accelerator( void )
{
     return dfb_config->accelerator;
}

static VideoMode *
system_get_modes( void )
{
     return modes;
}

static VideoMode *
system_get_current_mode( void )
{
     return &modes[0];   /* FIXME */
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
     /* Return zero because shared surface data is unneeded. */
     return 0;
}

static void
system_surface_data_init( CoreSurface *surface, void *data )
{
     /* Ignore since unneeded. */
}

static void
system_surface_data_destroy( CoreSurface *surface, void *data )
{
     /* Ignore since unneeded. */
}

static void
system_get_deviceid( unsigned int *ret_vendor_id,
                     unsigned int *ret_device_id )
{
}

/**********************************************************************************************************************/

static int
X11_VDPAU_Dispatch_OutputSurfaceCreate( DFBX11                        *x11,
                                        DFBX11CallOutputSurfaceCreate *create )
{
     DFBX11VDPAU *vdp = &x11->vdp;

     VdpStatus        status;
     VdpOutputSurface surface;

     status = vdp->OutputSurfaceCreate( vdp->device, create->rgba_format,
                                        create->width, create->height, &surface );
     if (status) {
          D_ERROR( "DirectFB/X11/VDPAU: OutputSurfaceCreate( format %u, size %dx%d ) failed (status %d, '%s'!\n",
                   create->rgba_format, create->width, create->height, status, vdp->GetErrorString( status ) );
          return 0;
     }

     return (int) surface;
}

static int
X11_VDPAU_Dispatch_OutputSurfaceDestroy( DFBX11                         *x11,
                                         DFBX11CallOutputSurfaceDestroy *destroy )
{
     DFBX11VDPAU *vdp = &x11->vdp;

     VdpStatus status;

     status = vdp->OutputSurfaceDestroy( destroy->surface );
     if (status)
          D_ERROR( "DirectFB/X11/VDPAU: OutputSurfaceDestroy( %u ) failed (status %d, '%s'!\n",
                   destroy->surface, status, vdp->GetErrorString( status ) );

     return (int) status;
}

static int
X11_VDPAU_Dispatch_OutputSurfaceGetBitsNative( DFBX11                               *x11,
                                               DFBX11CallOutputSurfaceGetBitsNative *get )
{
     if (!fusion_is_shared( dfb_core_world(x11->core), get->ptr )) {
          D_ERROR( "DirectFB/X11/VDPAU: Pointer (%p) is not shared, discarding OutputSurfaceGetBitsNative()!\n", get->ptr );
          return 0;
     }


     DFBX11VDPAU *vdp = &x11->vdp;

     VdpStatus status;

     status = vdp->OutputSurfaceGetBitsNative( get->surface,
                                               &get->source_rect,
                                               &get->ptr,
                                               &get->pitch );
     if (status)
          D_ERROR( "DirectFB/X11/VDPAU: OutputSurfaceGetBitsNative( surface %u ) failed (status %d, '%s'!\n",
                   get->surface, status, vdp->GetErrorString( status ) );

     return (int) status;
}

static int
X11_VDPAU_Dispatch_OutputSurfacePutBitsNative( DFBX11                               *x11,
                                               DFBX11CallOutputSurfacePutBitsNative *put )
{
     if (!fusion_is_shared( dfb_core_world(x11->core), put->ptr )) {
          D_ERROR( "DirectFB/X11/VDPAU: Pointer (%p) is not shared, discarding OutputSurfacePutBitsNative()!\n", put->ptr );
          return 0;
     }


     DFBX11VDPAU *vdp = &x11->vdp;

     VdpStatus status;

     status = vdp->OutputSurfacePutBitsNative( put->surface,
                                               (void const * const *) &put->ptr,
                                               &put->pitch,
                                               &put->destination_rect );
     if (status)
          D_ERROR( "DirectFB/X11/VDPAU: OutputSurfacePutBitsNative( surface %u ) failed (status %d, '%s'!\n",
                   put->surface, status, vdp->GetErrorString( status ) );

     return (int) status;
}

static int
X11_VDPAU_Dispatch_OutputSurfaceRenderOutputSurface( DFBX11                                     *x11,
                                                     DFBX11CallOutputSurfaceRenderOutputSurface *render )
{
     DFBX11VDPAU *vdp = &x11->vdp;

     VdpStatus status;

     status = vdp->OutputSurfaceRenderOutputSurface( render->destination_surface,
                                                     &render->destination_rect,
                                                     render->source_surface,
                                                     &render->source_rect,
                                                     &render->color,
                                                     &render->blend_state,
                                                     render->flags );
     if (status)
          D_ERROR( "DirectFB/X11/VDPAU: OutputSurfaceRenderOutputSurface( dest %u, source %u ) failed (status %d, '%s'!\n",
                   render->destination_surface, render->source_surface, status, vdp->GetErrorString( status ) );

     return (int) status;
}

static int
X11_VDPAU_Dispatch_PresentationQueueDisplay( DFBX11                             *x11,
                                             DFBX11CallPresentationQueueDisplay *display )
{
     DFBX11VDPAU *vdp = &x11->vdp;

     VdpStatus status;

     status = vdp->PresentationQueueDisplay( display->presentation_queue,
                                             display->surface,
                                             display->clip_width,
                                             display->clip_height,
                                             display->earliest_presentation_time );
     if (status)
          D_ERROR( "DirectFB/X11/VDPAU: PresentationQueueDisplay( queue %u, surface %u ) failed (status %d, '%s'!\n",
                   display->presentation_queue, display->surface, status, vdp->GetErrorString( status ) );

     return (int) status;
}

static FusionCallHandlerResult
X11_VDPAU_Dispatch( int           caller,
                    int           call_arg,
                    void         *call_ptr,
                    void         *ctx,
                    unsigned int  serial,
                    int          *ret_val )
{
     DFBX11 *x11 = ctx;

     XLockDisplay( x11->display );

     switch (call_arg) {
          case X11_VDPAU_OUTPUT_SURFACE_CREATE:
               *ret_val = X11_VDPAU_Dispatch_OutputSurfaceCreate( x11, call_ptr );
               break;

          case X11_VDPAU_OUTPUT_SURFACE_DESTROY:
               *ret_val = X11_VDPAU_Dispatch_OutputSurfaceDestroy( x11, call_ptr );
               break;

          case X11_VDPAU_OUTPUT_SURFACE_GET_BITS_NATIVE:
               *ret_val = X11_VDPAU_Dispatch_OutputSurfaceGetBitsNative( x11, call_ptr );
               break;

          case X11_VDPAU_OUTPUT_SURFACE_PUT_BITS_NATIVE:
               *ret_val = X11_VDPAU_Dispatch_OutputSurfacePutBitsNative( x11, call_ptr );
               break;

          case X11_VDPAU_OUTPUT_SURFACE_RENDER_OUTPUT_SURFACE:
               *ret_val = X11_VDPAU_Dispatch_OutputSurfaceRenderOutputSurface( x11, call_ptr );
               break;

          case X11_VDPAU_PRESENTATION_QUEUE_DISPLAY:
               *ret_val = X11_VDPAU_Dispatch_PresentationQueueDisplay( x11, call_ptr );
               break;

          default:
               D_BUG( "unknown call" );
               *ret_val = DFB_BUG;
               break;
     }

     XUnlockDisplay( x11->display );

     return FCHR_RETURN;
}

