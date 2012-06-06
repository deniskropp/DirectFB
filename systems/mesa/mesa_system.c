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

#include <fcntl.h>
#include <sys/mman.h>

#include <directfb.h>

#include <direct/mem.h>

#include <fusion/shmalloc.h>

#include <core/core.h>
#include <core/surface_pool.h>

#include <misc/conf.h>

#include "mesa_system.h"
#include "vt.h"

#include <core/core_system.h>


DFB_CORE_SYSTEM( mesa )


static const char device_name[] = "/dev/dri/card0";

/**********************************************************************************************************************/

MesaData *m_data;    /* FIXME: Fix Core System API to pass data in all functions. */

/**********************************************************************************************************************/

static DFBResult
InitLocal( MesaData *mesa )
{
     DFBResult   ret;
     const char *ver;
     const char *extensions;
     EGLint      major, minor;
     
     setenv( "EGL_PLATFORM","drm",0 );

     mesa->fd = open( device_name, O_RDWR );
     if (mesa->fd < 0) {
          ret = errno2result( errno );
          D_PERROR( "DirectFB/Mesa: Failed to open '%s'!\n", device_name );
          return ret;
     }

     mesa->gbm = gbm_create_device( mesa->fd );

     mesa->dpy = eglGetDisplay( mesa->gbm );
     if (mesa->dpy == EGL_NO_DISPLAY) {
          D_ERROR( "DirectFB/Mesa: eglGetDisplay() failed!\n" );
          close( mesa->fd );
          return DFB_INIT;
     }

     if (!eglInitialize(mesa->dpy, &major, &minor)) {
          D_ERROR( "DirectFB/Mesa: eglInitialize() failed!\n" );
          close( mesa->fd );
          return DFB_INIT;
     }

     ver = eglQueryString(mesa->dpy, EGL_VERSION);
     D_INFO( "DirectFB/Mesa: EGL_VERSION = %s\n", ver );

     extensions = eglQueryString(mesa->dpy, EGL_EXTENSIONS);
     D_INFO( "DirectFB/Mesa: EGL_EXTENSIONS = %s\n", extensions );

     if (!strstr(extensions, "EGL_KHR_surfaceless_opengl")) {
          D_ERROR( "DirectFB/Mesa: No support for EGL_KHR_surfaceless_opengl!\n" );
          close( mesa->fd );
          return DFB_UNSUPPORTED;
     }

//     if (!setup_kms(fd, &kms))
//        return -1;

     eglBindAPI(EGL_OPENGL_API);


     mesa->ctx = eglCreateContext( mesa->dpy, NULL, EGL_NO_CONTEXT, NULL );

     eglMakeCurrent( mesa->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, mesa->ctx );

     return DFB_OK;
}

/**********************************************************************************************************************/

static void
system_get_info( CoreSystemInfo *info )
{
     info->type = CORE_MESA;
     info->caps = CSCAPS_ACCELERATION | CSCAPS_SECURE_FUSION | CSCAPS_ALWAYS_INDIRECT;

     snprintf( info->name, DFB_CORE_SYSTEM_INFO_NAME_LENGTH, "Mesa" );
}

static DFBResult
system_initialize( CoreDFB *core, void **ret_data )
{
     DFBResult            ret;
     MesaData            *mesa;
     MesaDataShared      *shared;
     FusionSHMPoolShared *pool;

     D_ASSERT( m_data == NULL );

     mesa = D_CALLOC( 1, sizeof(MesaData) );
     if (!mesa)
          return D_OOM();

     mesa->core = core;

     pool = dfb_core_shmpool( core );

     shared = SHCALLOC( pool, 1, sizeof(MesaDataShared) );
     if (!shared) {
          D_FREE( mesa );
          return D_OOSHM();
     }

     shared->shmpool = pool;

     mesa->shared = shared;

     m_data = mesa;
     
     if (dfb_config->vt) {
          ret = dfb_vt_initialize();
          if (ret) 
               return DFB_INIT;
     }

     ret = InitLocal( mesa );
     if (ret) {
          if (dfb_config->vt)
               dfb_vt_shutdown( false );

          return ret;
     }
     *ret_data = m_data;

     dfb_surface_pool_initialize( core, &mesaSurfacePoolFuncs, &shared->pool );

     mesa->screen = dfb_screens_register( NULL, mesa, mesaScreenFuncs );
     mesa->layer  = dfb_layers_register( mesa->screen, mesa, mesaLayerFuncs );


     core_arena_add_shared_field( core, "mesa", shared );

     return DFB_OK;
}

static DFBResult
system_join( CoreDFB *core, void **ret_data )
{
     DFBResult       ret;
     void           *tmp;
     MesaData       *mesa;
     MesaDataShared *shared;

     D_ASSERT( m_data == NULL );

     if (dfb_config->vt) {
          ret = dfb_vt_join();
          if (ret) 
               return DFB_INIT;
     }

     mesa = D_CALLOC( 1, sizeof(MesaData) );
     if (!mesa)
          return D_OOM();

     mesa->core = core;

     ret = core_arena_get_shared_field( core, "mesa", &tmp );
     if (ret) {
          D_FREE( mesa );
          return ret;
     }

     mesa->shared = shared = tmp;

     ret = InitLocal( mesa );
     if (ret)
          return ret;

     *ret_data = m_data = mesa;

     dfb_surface_pool_join( core, shared->pool, &mesaSurfacePoolFuncs );

     mesa->screen = dfb_screens_register( NULL, mesa, mesaScreenFuncs );
     mesa->layer  = dfb_layers_register( mesa->screen, mesa, mesaLayerFuncs );

     return DFB_OK;
}

static DFBResult
system_shutdown( bool emergency )
{
     DFBResult   ret;

     MesaDataShared *shared;

     D_ASSERT( m_data != NULL );

     shared = m_data->shared;
     D_ASSERT( shared != NULL );

     dfb_surface_pool_destroy( shared->pool );

     /* cleanup EGL related stuff */
     eglDestroyContext( m_data->dpy, m_data->ctx );
     eglTerminate( m_data->dpy );

     /* close drm fd */
     close( m_data->fd );

     if (dfb_config->vt)
          dfb_vt_shutdown( emergency );

     SHFREE( shared->shmpool, shared );
     
     D_FREE( m_data );
     m_data = NULL;

     return DFB_OK;
}

static DFBResult
system_leave( bool emergency )
{
     DFBResult   ret;

     MesaDataShared *shared;

     D_ASSERT( m_data != NULL );

     shared = m_data->shared;
     D_ASSERT( shared != NULL );

     dfb_surface_pool_leave( shared->pool );

     if (dfb_config->vt) {
          ret = dfb_vt_leave( emergency );
          if (ret)
               return ret;
     }

     D_FREE( m_data );
     m_data = NULL;

     return DFB_OK;
}

static DFBResult
system_suspend( void )
{
     D_ASSERT( m_data != NULL );

     return DFB_OK;
}

static DFBResult
system_resume( void )
{
     D_ASSERT( m_data != NULL );

     return DFB_OK;
}

static volatile void *
system_map_mmio( unsigned int    offset,
                 int             length )
{
     D_ASSERT( m_data != NULL );

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
     return dfb_config->video_phys + offset;
}

static void *
system_video_memory_virtual( unsigned int offset )
{
     D_ASSERT( m_data != NULL );

     return NULL;
}

static unsigned int
system_videoram_length( void )
{
     return dfb_config->video_length;
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
     return;
}

static void
system_surface_data_destroy( CoreSurface *surface, void *data )
{
     /* Ignore since unneeded. */
     return;
}

static void
system_get_deviceid( unsigned int *ret_vendor_id,
                     unsigned int *ret_device_id )
{
     return;
}

