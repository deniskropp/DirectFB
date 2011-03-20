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

#include <fusion/arena.h>
#include <fusion/shmalloc.h>

#include <core/core.h>
#include <core/surface_pool.h>

#include <misc/conf.h>

#include "pvr2d_primary.h"
#include "pvr2d_system.h"


#include <core/core_system.h>

DFB_CORE_SYSTEM( pvr2d )

/**********************************************************************************************************************/

static PVR2DData *m_data;    /* FIXME: Fix Core System API to pass data in all functions. */

/**********************************************************************************************************************/

static DFBResult
InitPVR2D( PVR2DData *pvr2d )
{
     PVR2DERROR       ePVR2DStatus;
     PVR2DDataShared *shared = pvr2d->shared;

     pvr2d->nDevices = PVR2DEnumerateDevices(0);
     pvr2d->pDevInfo = (PVR2DDEVICEINFO *) malloc(pvr2d->nDevices * sizeof(PVR2DDEVICEINFO));
     PVR2DEnumerateDevices(pvr2d->pDevInfo);
     pvr2d->nDeviceNum = pvr2d->pDevInfo[0].ulDevID;

     ePVR2DStatus = PVR2DCreateDeviceContext (pvr2d->nDeviceNum, &pvr2d->hPVR2DContext, 0);
     if (ePVR2DStatus) {
          D_ERROR( "DirectFB/PVR2D: PVR2DCreateDeviceContext() failed! (status %d)\n", ePVR2DStatus );
          return DFB_INIT;
     }

     ePVR2DStatus = PVR2DGetFrameBuffer( pvr2d->hPVR2DContext, PVR2D_FB_PRIMARY_SURFACE, &pvr2d->pFBMemInfo);
     if (ePVR2DStatus) {
          D_ERROR( "DirectFB/PVR2D: PVR2DGetFrameBuffer() failed! (status %d)\n", ePVR2DStatus );
          return DFB_INIT;
     }

     ePVR2DStatus = PVR2DGetScreenMode(pvr2d->hPVR2DContext, &pvr2d->eDisplayFormat, &pvr2d->lDisplayWidth, &pvr2d->lDisplayHeight, &pvr2d->lStride, &pvr2d->RefreshRate);
     if (ePVR2DStatus) {
          D_ERROR( "DirectFB/PVR2D: PVR2DGetScreenMode() failed! (status %d)\n", ePVR2DStatus );
          return DFB_INIT;
     }

     D_INFO( "DirectFB/PVR2D: Display %ldx%ld, format %d, stride %ld, refresh %d\n",
             pvr2d->lDisplayWidth, pvr2d->lDisplayHeight, pvr2d->eDisplayFormat, pvr2d->lStride, pvr2d->RefreshRate );

     shared->screen_size.w = pvr2d->lDisplayWidth;
     shared->screen_size.h = pvr2d->lDisplayHeight;

     return DFB_OK;
}

static DFBResult
InitEGL( PVR2DData *pvr2d )
{
     EGLint iMajorVersion, iMinorVersion;
     EGLint ai32ContextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};

     pvr2d->eglDisplay = eglGetDisplay((int)0);

     if (!eglInitialize(pvr2d->eglDisplay, &iMajorVersion, &iMinorVersion))
          return DFB_INIT;

     pvr2d->eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC) eglGetProcAddress("eglCreateImageKHR");
     if (pvr2d->eglCreateImageKHR == NULL) {
          D_ERROR( "DirectFB/PVR2D: eglCreateImageKHR not found!\n" );
          return DFB_UNSUPPORTED;
     }

     pvr2d->glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) eglGetProcAddress("glEGLImageTargetTexture2DOES");
     if (pvr2d->glEGLImageTargetTexture2DOES == NULL) {
          D_ERROR( "DirectFB/PVR2D: glEGLImageTargetTexture2DOES not found!\n" );
          return DFB_UNSUPPORTED;
     }


     eglBindAPI(EGL_OPENGL_ES_API);
     if (!TestEGLError("eglBindAPI"))
          return DFB_INIT;

     EGLint pi32ConfigAttribs[5];
     pi32ConfigAttribs[0] = EGL_SURFACE_TYPE;
     pi32ConfigAttribs[1] = EGL_WINDOW_BIT | EGL_PIXMAP_BIT;
     pi32ConfigAttribs[2] = EGL_RENDERABLE_TYPE;
     pi32ConfigAttribs[3] = EGL_OPENGL_ES2_BIT;
     pi32ConfigAttribs[4] = EGL_NONE;

     int iConfigs;
     if (!eglChooseConfig(pvr2d->eglDisplay, pi32ConfigAttribs, &pvr2d->eglConfig, 1, &iConfigs) || (iConfigs != 1)) {
          D_ERROR("DirectFB/PVR2D: eglChooseConfig() failed.\n");
          return DFB_INIT;
     }


     pvr2d->nativePixmap.ePixelFormat = 0;
     pvr2d->nativePixmap.eRotation    = 0;
     pvr2d->nativePixmap.lWidth       = pvr2d->lDisplayWidth;
     pvr2d->nativePixmap.lHeight      = pvr2d->lDisplayHeight;
     pvr2d->nativePixmap.lStride      = pvr2d->lStride;
     pvr2d->nativePixmap.lSizeInBytes = pvr2d->pFBMemInfo->ui32MemSize;
     pvr2d->nativePixmap.pvAddress    = pvr2d->pFBMemInfo->ui32DevAddr;
     pvr2d->nativePixmap.lAddress     = (long) pvr2d->pFBMemInfo->pBase;


     pvr2d->eglSurface = eglCreatePixmapSurface( pvr2d->eglDisplay, pvr2d->eglConfig, &pvr2d->nativePixmap, NULL );
     if (!TestEGLError("eglCreatePixmapSurface"))
          return DFB_INIT;


     pvr2d->eglContext = eglCreateContext(pvr2d->eglDisplay, pvr2d->eglConfig, NULL, ai32ContextAttribs);
     if (!TestEGLError("eglCreateContext"))
          return DFB_INIT;

     eglMakeCurrent( pvr2d->eglDisplay, pvr2d->eglSurface, pvr2d->eglSurface, pvr2d->eglContext );
     if (!TestEGLError("eglMakeCurrent"))
          return DFB_INIT;

     eglSwapInterval( pvr2d->eglDisplay, 1 );
     if (!TestEGLError("eglSwapInterval"))
          return DFB_INIT;

     return DFB_OK;
}

/**********************************************************************************************************************/

static void
system_get_info( CoreSystemInfo *info )
{
     info->type = CORE_PVR2D;
     info->caps = CSCAPS_ACCELERATION;

     snprintf( info->name, DFB_CORE_SYSTEM_INFO_NAME_LENGTH, "PVR2D" );
}

static DFBResult
system_initialize( CoreDFB *core, void **ret_data )
{
     DFBResult            ret;
     PVR2DData            *data;
     PVR2DDataShared      *shared;
     FusionSHMPoolShared *pool;

     D_ASSERT( m_data == NULL );

     data = D_CALLOC( 1, sizeof(PVR2DData) );
     if (!data)
          return D_OOM();

     data->core = core;

     pool = dfb_core_shmpool( core );

     shared = SHCALLOC( pool, 1, sizeof(PVR2DDataShared) );
     if (!shared) {
          D_FREE( data );
          return D_OOSHM();
     }

     shared->shmpool = pool;

     data->shared = shared;

     ret = InitPVR2D( data );
     if (ret) {
          SHFREE( pool, shared );
          D_FREE( data );
          return ret;
     }

     ret = InitEGL( data );
     if (ret) {
          SHFREE( pool, shared );
          D_FREE( data );
          return ret;
     }

     *ret_data = m_data = data;

     data->screen = dfb_screens_register( NULL, data, pvr2dPrimaryScreenFuncs );
     data->layer  = dfb_layers_register( data->screen, data, pvr2dPrimaryLayerFuncs );

     dfb_surface_pool_initialize( core, pvr2dSurfacePoolFuncs, &shared->pool );

     fusion_arena_add_shared_field( dfb_core_arena( core ), "pvr2d", shared );

     return DFB_OK;
}

static DFBResult
system_join( CoreDFB *core, void **ret_data )
{
     DFBResult         ret;
     void             *tmp;
     PVR2DData       *data;
     PVR2DDataShared *shared;

     D_ASSERT( m_data == NULL );

     data = D_CALLOC( 1, sizeof(PVR2DData) );
     if (!data)
          return D_OOM();

     data->core = core;

     ret = fusion_arena_get_shared_field( dfb_core_arena( core ), "pvr2d", &tmp );
     if (ret) {
          D_FREE( data );
          return ret;
     }

     data->shared = shared = tmp;


     *ret_data = m_data = data;

     data->screen = dfb_screens_register( NULL, data, pvr2dPrimaryScreenFuncs );
     data->layer  = dfb_layers_register( data->screen, data, pvr2dPrimaryLayerFuncs );

     dfb_surface_pool_join( core, shared->pool, pvr2dSurfacePoolFuncs );

     return DFB_OK;
}

static DFBResult
system_shutdown( bool emergency )
{
     PVR2DDataShared *shared;

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
     PVR2DDataShared *shared;

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

