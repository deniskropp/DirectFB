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

#include "android_system.h"

#include <core/core_system.h>

#include <android/window.h>

DFB_CORE_SYSTEM( android )


/**********************************************************************************************************************/

AndroidData *m_data;    /* FIXME: Fix Core System API to pass data in all functions. */

// FIXME
extern AndroidNativeData native_data;

/**********************************************************************************************************************/

static inline void crashme()
{
     char* a = 0;
     *a = 0;
}

static void drawit(ANativeWindow_Buffer* buffer)
{
    LOGI("width=%d height=%d stride=%d format=%d", buffer->width, buffer->height,
            buffer->stride, buffer->format);    
                                                
                if (buffer->format == WINDOW_FORMAT_RGB_565)
          {D_INFO("#################### 565\n");
                        uint16_t* pixels16 = (uint16_t*)buffer->bits;
                        int  xx;
                        int  yy;
                        uint16_t value=0xffff;
                        for (yy = 0; yy < buffer->height; yy++) {
        uint16_t*  line16 = (uint16_t*)pixels16;
        for (xx = 0; xx < buffer->width; xx++) {
                                *line16++=value;
                                value += 33;
                                 
                                }
                                pixels16 += buffer->stride;
                        }
                }   
                else
                {D_INFO("#################### 8888 %d %d %d\n", buffer->format, buffer->width, buffer->height);
                        uint32_t* pixels32 = (uint32_t*)buffer->bits;
                        uint32_t value;
                        int  xx;
                        int  yy;
                        //value = 0x30ff0000;//+native_data.x;
                        value=0x30000030;
                        for (yy = 0; yy < buffer->height; yy++) {
        uint32_t*  line32 = (uint32_t*)pixels32;
        for (xx = 0; xx < buffer->width; xx++) {
//                              *line32++=0x000000ff; // red (R)  
//                              *line32++=0x0000ff00; // green (G)
//                              *line32++=0x30ff0000; // blue (B)
//                              value = 0x30ff0000 + buffer->height<<255 + buffer->width;
             
             //                   *line32++=value;
              //                  value += 33;
             *line32++=value;
                                }
                                pixels32 += buffer->stride;
                        }
                }
}  
   
   
/**
 * Just the current frame in the display.
 */
static void engine_draw_frame() {
    ANativeWindow_Buffer buffer;     
    sleep(3);           
    D_INFO("########### locking buffer\n");
    if (ANativeWindow_lock(native_data.app->window, &buffer, NULL) < 0) {
        LOGW("Unable to lock window buffer");
        return; 
    }           
                D_INFO("########### drawing buffer\n");
                drawit(&buffer);
                D_INFO("####################1\n");
    ANativeWindow_unlockAndPost(native_data.app->window);
    D_INFO("####################2\n");
}  

static DFBResult
InitLocal( AndroidData *android )
{
D_INFO("########## system: init local\n");
sleep(10);
     /*
      * Here specify the attributes of the desired configuration.
      * Below, we select an EGLConfig with at least 8 bits per color
      * component compatible with on-screen windows
      */
     const EGLint attribs[] = {
             EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
             EGL_BLUE_SIZE, 8,
             EGL_GREEN_SIZE, 8,
             EGL_RED_SIZE, 8,
             EGL_ALPHA_SIZE, 8,
             EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
             EGL_NATIVE_VISUAL_ID, HAL_PIXEL_FORMAT_RGBA_8888,    // DSPF_ARGB
             EGL_NONE
     };

     static const EGLint ctx_attribs[] = {
          EGL_CONTEXT_CLIENT_VERSION, 2,
          EGL_NONE
     };
     D_INFO("######1111111111111#############\n");
     D_INFO("########### set buffers geometry %p\n", native_data.app->activity);
     ANativeWindow_setBuffersGeometry(native_data.app->window, 64, 64, WINDOW_FORMAT_RGBA_8888);
     D_INFO("########### set buffers geometry %p\n", native_data.app->activity);
	//ANativeActivity_setWindowFlags(native_data.app->activity, (uint32_t)(AWINDOW_FLAG_FULLSCREEN), 0);	
     D_INFO("########### set buffers geometry %p\n", native_data.app->activity);
engine_draw_frame();
//engine_draw_frame();
sleep(10);
     EGLint w, h, dummy, format;
     EGLint numConfigs;
     EGLConfig config;
     EGLSurface surface;
     EGLContext context;

     EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

     eglInitialize(display, 0, 0);
     D_INFO("######222222222222222#############\n");
engine_draw_frame();
     /* Here, the application chooses the configuration it desires. In this
      * sample, we have a very simplified selection process, where we pick
      * the first EGLConfig that matches our criteria */
     eglChooseConfig(display, attribs, &config, 1, &numConfigs);
     D_INFO("######333333333333333333#############\n");
engine_draw_frame();
     /* EGL_NATIVE_VISUAL_ID is an attribute of the EGLConfig that is
      * guaranteed to be accepted by ANativeWindow_setBuffersGeometry().
      * As soon as we picked a EGLConfig, we can safely reconfigure the
      * ANativeWindow buffers to match, using EGL_NATIVE_VISUAL_ID. */
     eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
//eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
D_INFO("########### set buffers geometry %p\n", native_data.app->window);
ANativeWindow_setBuffersGeometry(native_data.app->window, 1280, 720, format);
//D_INFO("########### set window flags %p\n", native_data.app->activity);
//ANativeActivity_setWindowFlags(native_data.app->activity, (uint32_t)(AWINDOW_FLAG_FULLSCREEN /*| AWINDOW_FLAG_SHOW_WHEN_LOCKED | AWINDOW_FLAG_SHOW_WALLPAPER | AWINDOW_FLAG_TURN_SCREEN_ON | AWINDOW_FLAG_KEEP_SCREEN_ON*/), 0);

//     ANativeWindow_setBuffersGeometry( native_data.app->window, 0, 0, format);

//     ANativeActivity_setWindowFlags( native_data.app->window, AWINDOW_FLAG_FULLSCREEN | AWINDOW_FLAG_KEEP_SCREEN_ON , 0 );
//eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
D_INFO("############### creating context 1\n");
     surface = eglCreateWindowSurface(display, config, native_data.app->window, NULL);
     D_INFO("############### creating context 2\n");
     
     context = eglCreateContext(display, config, NULL, ctx_attribs);
D_INFO("############### creating context 3 curr\n");

     if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {
         D_INFO("Unable to eglMakeCurrent");
         //return -1;
     }

     /*eglQuerySurface(display, surface, EGL_WIDTH, &w);
     eglQuerySurface(display, surface, EGL_HEIGHT, &h);

     android->dpy = display;
     android->ctx = context;
     android->surface = surface;
     android->shared->screen_size.w = w;
     android->shared->screen_size.h = h;     

     if (strstr(glGetString(GL_RENDERER),"SGX"))
          android->shared->native_pixelformat = HAL_PIXEL_FORMAT_RGBA_8888; //ANativeWindow_getFormat(native_data.app->window);
     else
          android->shared->native_pixelformat = ANativeWindow_getFormat(native_data.app->window);
*/
     // Initialize GL state.
//     glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
  //   glEnable(GL_CULL_FACE);
//     glShadeModel(GL_SMOOTH);
  //   glDisable(GL_DEPTH_TEST);



     // Just fill the screen with a color.
  //   glClearColor( .5, .5, .5, .5 );
  //   glClear( GL_COLOR_BUFFER_BIT );
     D_INFO("######8888888888888888888#############\n");
engine_draw_frame();
  //   eglSwapBuffers( android->dpy, android->surface );
sleep(10);
     return DFB_OK;
}

/**********************************************************************************************************************/

static void
system_get_info( CoreSystemInfo *info )
{
     info->type = CORE_ANDROID;
     info->caps = CSCAPS_ACCELERATION | CSCAPS_ALWAYS_INDIRECT;

     snprintf( info->name, DFB_CORE_SYSTEM_INFO_NAME_LENGTH, "Android" );
}

static DFBResult
system_initialize( CoreDFB *core, void **ret_data )
{
     DFBResult            ret;
     AndroidData         *android;
     AndroidDataShared   *shared;
     FusionSHMPoolShared *pool;
D_INFO("######### system initialize\n");
     D_ASSERT( m_data == NULL );

     android = D_CALLOC( 1, sizeof(AndroidData) );
     if (!android)
          return D_OOM();

     android->core = core;

     pool = dfb_core_shmpool( core );

     shared = SHCALLOC( pool, 1, sizeof(AndroidDataShared) );
     if (!shared) {
          D_FREE( android );
          return D_OOSHM();
     }

     shared->shmpool = pool;

     android->shared = shared;

     m_data = android;

     ret = InitLocal( android );
     if (ret)
          return ret;

     *ret_data = m_data;

     dfb_surface_pool_initialize( core, &androidSurfacePoolFuncs, &shared->pool );

     android->screen   = dfb_screens_register( NULL, android, androidScreenFuncs );
     android->layer    = dfb_layers_register( android->screen, android, androidLayerFuncs );
     android->java_vm  = native_data.app->activity->vm;
     android->app_path = D_STRDUP( native_data.app->activity->internalDataPath) ;

     core_arena_add_shared_field( core, "android", shared );

     return DFB_OK;
}

static DFBResult
system_join( CoreDFB *core, void **ret_data )
{
     DFBResult          ret;
     void              *tmp;
     AndroidData       *android;
     AndroidDataShared *shared;

     D_ASSERT( m_data == NULL );

     android = D_CALLOC( 1, sizeof(AndroidData) );
     if (!android)
          return D_OOM();

     android->core = core;

     ret = core_arena_get_shared_field( core, "android", &tmp );
     if (ret) {
          D_FREE( android );
          return ret;
     }

     android->shared = shared = tmp;

     ret = InitLocal( android );
     if (ret)
          return ret;

     *ret_data = m_data = android;

     dfb_surface_pool_join( core, shared->pool, &androidSurfacePoolFuncs );

     android->screen = dfb_screens_register( NULL, android, androidScreenFuncs );
     android->layer  = dfb_layers_register( android->screen, android, androidLayerFuncs );

     return DFB_OK;
}

static DFBResult
system_shutdown( bool emergency )
{
     DFBResult   ret;

     // for now just crash until we implement a clean shutdown
     crashme();

     AndroidDataShared *shared;
     D_ASSERT( m_data != NULL );

     shared = m_data->shared;
     D_ASSERT( shared != NULL );

     dfb_surface_pool_destroy( shared->pool );

     /* cleanup EGL related stuff */
     eglDestroyContext( m_data->dpy, m_data->ctx );
     eglTerminate( m_data->dpy );

     SHFREE( shared->shmpool, shared );

     D_FREE( m_data );
     m_data = NULL;

     return DFB_OK;
}

static DFBResult
system_leave( bool emergency )
{
     DFBResult   ret;

     AndroidDataShared *shared;
     D_ASSERT( m_data != NULL );

     shared = m_data->shared;
     D_ASSERT( shared != NULL );

     dfb_surface_pool_leave( shared->pool );

     /* cleanup EGL related stuff */
     eglDestroyContext( m_data->dpy, m_data->ctx );
     eglTerminate( m_data->dpy );

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
     return 0;
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

