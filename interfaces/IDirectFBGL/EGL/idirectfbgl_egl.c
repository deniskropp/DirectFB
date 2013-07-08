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

#include <stdarg.h>

#define EGL_EGLEXT_PROTOTYPES

#include <EGL/egl.h>

#include <directfbgl.h>

#include <idirectfb.h>

#include <direct/debug.h>

#include <core/surface.h>

#include <core/core.h>
#include <core/layers.h>
#include <core/layer_control.h>
#include <core/layer_context.h>
#include <core/layers_internal.h>
#include <core/windows_internal.h>
#include <core/wm.h>

#include <windows/idirectfbwindow.h>

#include <display/idirectfbsurface.h>


D_DEBUG_DOMAIN( IDFBGL_EGL, "IDirectFBGL/EGL", "IDirectFBGL EGL Implementation" );

static DirectResult
Probe( void *ctx, ... );

static DirectResult
Construct( void *interface, ... );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBGL, EGL )



static inline bool
TestEGLError( const char* pszLocation )
{
     EGLint iErr = eglGetError();
     if (iErr != EGL_SUCCESS) {
          D_ERROR( "DirectFB/EGL: %s failed (%d).\n", pszLocation, iErr );
          return false;
     }

     return true;
}


/*
 * private data struct of IDirectFBGL
 */
typedef struct {
     int                      ref;       /* reference counter */

     CoreDFB                 *core;

     IDirectFB               *dfb;
     IDirectFBSurface        *surface;

     EGLDisplay               eglDisplay;
     EGLConfig                eglConfig;
     EGLContext               eglContext;
     EGLSurface               eglSurface;
} IDirectFBGL_data;


static void
IDirectFBGL_Destruct( IDirectFBGL *thiz )
{
//     IDirectFBGL_data *data = thiz->priv;

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DirectResult
IDirectFBGL_AddRef( IDirectFBGL *thiz )
{
     DIRECT_INTERFACE_GET_DATA (IDirectFBGL);

     data->ref++;

     return DFB_OK;
}

static DirectResult
IDirectFBGL_Release( IDirectFBGL *thiz )
{
     DIRECT_INTERFACE_GET_DATA (IDirectFBGL)

     if (--data->ref == 0)
          IDirectFBGL_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBGL_Lock( IDirectFBGL *thiz )
{
     DIRECT_INTERFACE_GET_DATA (IDirectFBGL);

     D_DEBUG_AT( IDFBGL_EGL, "%s()\n", __FUNCTION__ );

     eglMakeCurrent( data->eglDisplay, data->eglSurface, data->eglSurface, data->eglContext );
     if (!TestEGLError( "eglMakeCurrent" ))
          return DFB_FAILURE;

     return DFB_OK;
}

static DFBResult
IDirectFBGL_Unlock( IDirectFBGL *thiz )
{
     DIRECT_INTERFACE_GET_DATA (IDirectFBGL);

     D_DEBUG_AT( IDFBGL_EGL, "%s()\n", __FUNCTION__ );

//     eglSwapBuffers(data->eglDisplay, data->eglSurface);

//     eglMakeCurrent( data->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
//     if (!TestEGLError( "eglMakeCurrent" ))
//          return DFB_FAILURE;

     return DFB_OK;
}

static DFBResult
IDirectFBGL_GetProcAddress( IDirectFBGL  *thiz,
                            const char   *name,
                            void        **ret_address )
{
     void *handle;

     DIRECT_INTERFACE_GET_DATA (IDirectFBGL);

     D_DEBUG_AT( IDFBGL_EGL, "%s()\n", __FUNCTION__ );

     if (!name)
          return DFB_INVARG;

     if (!ret_address)
          return DFB_INVARG;

     handle = (void*) eglGetProcAddress( name );
     if (!handle)
          return DFB_FAILURE;

     *ret_address = handle;

     return (*ret_address) ? DFB_OK : DFB_UNSUPPORTED;
}

static DFBResult
IDirectFBGL_GetAttributes( IDirectFBGL     *thiz,
                           DFBGLAttributes *attributes )
{
     DIRECT_INTERFACE_GET_DATA (IDirectFBGL);

     D_DEBUG_AT( IDFBGL_EGL, "%s()\n", __FUNCTION__ );

     if (!attributes)
          return DFB_INVARG;

     memset( attributes, 0, sizeof(DFBGLAttributes) );

     attributes->alpha_size    = 8;
     attributes->red_size      = 8;
     attributes->green_size    = 8;
     attributes->blue_size     = 8;
     attributes->buffer_size   = 32;
     attributes->depth_size    = 32;
     attributes->double_buffer = false; // FIXME: check surface caps here?
     attributes->stereo        = false; // FIXME: check surface caps here?

     return DFB_OK;
}

/* exported symbols */

static DirectResult
Probe( void *ctx, ... )
{
     D_DEBUG_AT( IDFBGL_EGL, "%s()\n", __FUNCTION__ );

     (void) ctx;

     /* ... */

     return DFB_OK;
}

static DirectResult
Construct( void *interface, ... )
{
     IDirectFBGL      *thiz = interface;
     IDirectFBSurface *surface;
     IDirectFB        *dfb;
     const char       *ver;
     const char       *extensions;
     EGLint            count = 0;
     EGLint            major, minor;

     EGLint config_attrs[] = {
          EGL_BUFFER_SIZE,	EGL_DONT_CARE,
          EGL_DEPTH_SIZE,		24,
          EGL_RED_SIZE,		8,
          EGL_GREEN_SIZE,		8,
          EGL_RED_SIZE,		8,
          EGL_ALPHA_SIZE,		8,
          EGL_RENDERABLE_TYPE,	EGL_OPENGL_ES2_BIT,
          EGL_NONE
     };
     EGLint context_attrs[] = {
          EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE, EGL_NONE
     };
     EGLint surface_attrs[] = {
          EGL_RENDER_BUFFER, EGL_BACK_BUFFER, EGL_NONE
     };

     D_DEBUG_AT( IDFBGL_EGL, "%s()\n", __FUNCTION__ );

     va_list tag;
     va_start(tag, interface);
     surface = va_arg(tag, IDirectFBSurface *);
     dfb = va_arg(tag, IDirectFB *);
     va_end( tag );

     /* Allocate interface data. */
     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBGL );

     /* Initialize interface data. */
     data->ref     = 1;
     data->surface = surface;
     data->dfb     = dfb;


     data->eglDisplay = eglGetDisplay( (EGLNativeDisplayType) DLID_PRIMARY );
     if (data->eglDisplay == EGL_NO_DISPLAY) {
          D_ERROR( "DirectFB/EGL: eglGetDisplay( dfb ) failed!\n" );
          goto error;
     }

     D_INFO("calling eglInitialise()\n");
     if (!eglInitialize( data->eglDisplay, &major, &minor )) {
          D_ERROR( "DirectFB/EGL: eglInitialize() failed!\n" );
          goto error;
     }

     ver        = eglQueryString( data->eglDisplay, EGL_VERSION );
     extensions = eglQueryString( data->eglDisplay, EGL_EXTENSIONS );

     D_INFO( "DirectFB/EGL: v%d.%d, EGL_VERSION = %s, EGL_EXTENSIONS = %s\n", major, minor, ver, extensions );


     if (!eglChooseConfig( data->eglDisplay, config_attrs, &data->eglConfig, 1, &count ) || count == 0) {
          D_ERROR( "DirectFB/EGL: No suitable EGL config, count = %d!\n", count );
          goto error;
     }

     data->eglSurface = eglCreateWindowSurface( data->eglDisplay, data->eglConfig, (EGLNativeWindowType) surface, surface_attrs );
     if (!TestEGLError("eglCreateWindowSurface")) {
          D_ERROR( "DirectFB/EGL: eglCreateWindowSurface() failed!\n" );
          goto error;
     }

     eglBindAPI( EGL_OPENGL_ES_API );

     data->eglContext = eglCreateContext( data->eglDisplay, data->eglConfig, EGL_NO_CONTEXT, context_attrs );
     if (!TestEGLError("eglCreateContext")) {
          D_ERROR( "DirectFB/EGL: eglCreateContext() failed!\n" );
          goto error;
     }

     eglSwapInterval(data->eglDisplay, (EGLint)1);

     /* Assign interface pointers. */
     thiz->AddRef         = IDirectFBGL_AddRef;
     thiz->Release        = IDirectFBGL_Release;
     thiz->Lock           = IDirectFBGL_Lock;
     thiz->Unlock         = IDirectFBGL_Unlock;
     thiz->GetProcAddress = IDirectFBGL_GetProcAddress;
     thiz->GetAttributes  = IDirectFBGL_GetAttributes;

     return DFB_OK;


error:
     if (data->eglSurface)
          eglDestroySurface( data->eglDisplay, data->eglSurface );

     if (data->eglContext)
          eglDestroyContext( data->eglDisplay, data->eglContext );

     if (data->eglDisplay)
          eglTerminate( data->eglDisplay );

     DIRECT_DEALLOCATE_INTERFACE( thiz );

     return DFB_FAILURE;
}

