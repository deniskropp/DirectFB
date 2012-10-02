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

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <directfb.h>

#include <direct/mem.h>

#include <fusion/shmalloc.h>

#include <core/core.h>
#include <core/surface_pool.h>

#include <misc/conf.h>

#include "egl_primary.h"
#include "egl_system.h"


#include <core/core_system.h>

#define RASPBERRY_PI

DFB_CORE_SYSTEM( egl )

/**********************************************************************************************************************/

static EGLData *m_data;    /* FIXME: Fix Core System API to pass data in all functions. */

/**********************************************************************************************************************/


static DFBResult
InitEGL( EGLData *egl )
{
     int    err;
     EGLint iMajorVersion, iMinorVersion;
     EGLint ai32ContextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};

#ifdef RASPBERRY_PI
     static EGL_DISPMANX_WINDOW_T nativewindow;
     DISPMANX_ELEMENT_HANDLE_T dispman_element;
     DISPMANX_DISPLAY_HANDLE_T dispman_display;
     DISPMANX_UPDATE_HANDLE_T dispman_update;
     VC_RECT_T dst_rect;
     VC_RECT_T src_rect;

     bcm_host_init();

#endif     
     egl->eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);

     if (!eglInitialize(egl->eglDisplay, &iMajorVersion, &iMinorVersion))
          return DFB_INIT;

     egl->eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC) eglGetProcAddress("eglCreateImageKHR");
     if (egl->eglCreateImageKHR == NULL) {
          D_ERROR( "DirectFB/EGL: eglCreateImageKHR not found!\n" );
          return DFB_UNSUPPORTED;
     }

     egl->glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) eglGetProcAddress("glEGLImageTargetTexture2DOES");
     if (egl->glEGLImageTargetTexture2DOES == NULL) {
          D_ERROR( "DirectFB/EGL: glEGLImageTargetTexture2DOES not found!\n" );
          return DFB_UNSUPPORTED;
     }


     eglBindAPI(EGL_OPENGL_ES_API);
     if (!TestEGLError("eglBindAPI"))
          return DFB_INIT;

     EGLint pi32ConfigAttribs[] = {  EGL_RED_SIZE, 8,
                                     EGL_GREEN_SIZE, 8,
                                     EGL_BLUE_SIZE, 8,
                                     EGL_ALPHA_SIZE, 8,
                                     EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                                     EGL_NONE
                                  };

     int iConfigs;
     if (!eglChooseConfig(egl->eglDisplay, pi32ConfigAttribs, &egl->eglConfig, 1, &iConfigs) || (iConfigs != 1)) {
          D_ERROR("DirectFB/EGL: eglChooseConfig() failed.\n");
          return DFB_INIT;
     }
     
     egl->eglContext = eglCreateContext(egl->eglDisplay, egl->eglConfig, EGL_NO_CONTEXT, ai32ContextAttribs);
     if (!TestEGLError("eglCreateContext"))
          return DFB_INIT;
     
#ifdef RASPBERRY_PI
     graphics_get_display_size(0 /* LCD */, &egl->DisplayWidth, &egl->DisplayHeight);

     dst_rect.x = 0;
     dst_rect.y = 0;
     dst_rect.width = egl->DisplayWidth;
     dst_rect.height = egl->DisplayHeight;

     src_rect.x = 0;
     src_rect.y = 0;
     src_rect.width  = dst_rect.width  << 16; // ANDI: fixed point 16.16?
     src_rect.height = dst_rect.height << 16;

     dispman_display = vc_dispmanx_display_open( 0);
     dispman_update = vc_dispmanx_update_start( 0 );

     dispman_element = vc_dispmanx_element_add ( dispman_update, dispman_display,  0, &dst_rect, 0,
                                                 &src_rect, DISPMANX_PROTECTION_NONE, 0, 0, 0);
      
     nativewindow.element = dispman_element;
     nativewindow.width   = egl->DisplayWidth;
     nativewindow.height  = egl->DisplayHeight;
     vc_dispmanx_update_submit_sync( dispman_update );

#endif

     egl->eglSurface = eglCreateWindowSurface( egl->eglDisplay, egl->eglConfig, &nativewindow, NULL );
     if (!TestEGLError("eglCreateWindowSurface"))
          return DFB_INIT;


     eglMakeCurrent( egl->eglDisplay, egl->eglSurface, egl->eglSurface, egl->eglContext );
     if (!TestEGLError("eglMakeCurrent"))
          return DFB_INIT;

     eglSwapInterval( egl->eglDisplay, 1 );
     if (!TestEGLError("eglSwapInterval"))
          return DFB_INIT;

     if ((err = glGetError()) != 0) {
          D_ERROR( "DirectFB/EGL: Error at end of InitEGL! (error = %x)\n", err );
          //return DFB_FAILURE;
     }

     return DFB_OK;
}

/**********************************************************************************************************************/

static void
system_get_info( CoreSystemInfo *info )
{
     info->type = CORE_EGL;
     info->caps = CSCAPS_ACCELERATION;

     snprintf( info->name, DFB_CORE_SYSTEM_INFO_NAME_LENGTH, "EGL" );
}

static DFBResult
system_initialize( CoreDFB *core, void **ret_data )
{
     DFBResult            ret;
     EGLData            *data;
     EGLDataShared      *shared;
     FusionSHMPoolShared *pool;

     D_ASSERT( m_data == NULL );

     data = D_CALLOC( 1, sizeof(EGLData) );
     if (!data)
          return D_OOM();

     data->core = core;

     pool = dfb_core_shmpool( core );

     shared = SHCALLOC( pool, 1, sizeof(EGLDataShared) );
     if (!shared) {
          D_FREE( data );
          return D_OOSHM();
     }

     shared->shmpool = pool;

     data->shared = shared;

     ret = InitEGL( data );
     if (ret) {
          SHFREE( pool, shared );
          D_FREE( data );
          return ret;
     }

     *ret_data = m_data = data;

     data->screen = dfb_screens_register( NULL, data, eglPrimaryScreenFuncs );
     data->layer  = dfb_layers_register( data->screen, data, eglPrimaryLayerFuncs );

     dfb_surface_pool_initialize( core, eglSurfacePoolFuncs, &shared->pool );

     core_arena_add_shared_field( core, "egl", shared );

     return DFB_OK;
}

static DFBResult
system_join( CoreDFB *core, void **ret_data )
{
     DFBResult         ret;
     void             *tmp;
     EGLData       *data;
     EGLDataShared *shared;

     D_ASSERT( m_data == NULL );

     data = D_CALLOC( 1, sizeof(EGLData) );
     if (!data)
          return D_OOM();

     data->core = core;

     ret = core_arena_get_shared_field( core, "egl", &tmp );
     if (ret) {
          D_FREE( data );
          return ret;
     }

     data->shared = shared = tmp;


     *ret_data = m_data = data;

     data->screen = dfb_screens_register( NULL, data, eglPrimaryScreenFuncs );
     data->layer  = dfb_layers_register( data->screen, data, eglPrimaryLayerFuncs );

     dfb_surface_pool_join( core, shared->pool, eglSurfacePoolFuncs );

     return DFB_OK;
}

static DFBResult
system_shutdown( bool emergency )
{
     EGLDataShared *shared;

     D_ASSERT( m_data != NULL );

     shared = m_data->shared;
     D_ASSERT( shared != NULL );

     dfb_surface_pool_destroy( shared->pool );


     SHFREE( shared->shmpool, shared );

     D_FREE( m_data );
     m_data = NULL;

     return DFB_OK;
}

static DFBResult
system_leave( bool emergency )
{
     EGLDataShared *shared;

     D_ASSERT( m_data != NULL );

     shared = m_data->shared;
     D_ASSERT( shared != NULL );

     dfb_surface_pool_leave( shared->pool );


     D_FREE( m_data );
     m_data = NULL;

     return DFB_OK;
}

static DFBResult
system_suspend()
{
     D_ASSERT( m_data != NULL );

     return DFB_OK;
}

static DFBResult
system_resume()
{
     D_ASSERT( m_data != NULL );

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
system_get_accelerator()
{
     return dfb_config->accelerator;
}

static VideoMode *
system_get_modes()
{
     return NULL;
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

