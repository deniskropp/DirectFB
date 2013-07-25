/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
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
#include <string.h>

extern "C" {
#include <direct/messages.h>
}

#include <direct/ToString.h>

#include <egl/dfbegl.h>



D_LOG_DOMAIN( DFBEGL_Api, "DFBEGL/API", "DirectFB EGL API" );

// FIXME: add API debug prints

/*****************************************************************************/


namespace DirectFB {

Direct::TLSObject2<EGL::TLS> EGLTLS;


extern "C" {

EGLint eglGetError (void)
{
     EGL::TLS *tls = EGLTLS.Get();

     EGLint error = tls->GetError();
     tls->SetError( EGL_SUCCESS );
     return error;
}

EGLDisplay eglGetDisplay (EGLNativeDisplayType native_display)
{
     D_LOG( DFBEGL_Api, DEBUG_1, "%s()\n", __FUNCTION__ );

     EGL::TLS *tls = EGLTLS.Get();

     EGL::Display *display = (EGL::Display*) EGL_NO_DISPLAY;

     EGLint error = EGL::Core::GetInstance().GetDisplay( native_display, display );

     DFB_EGL_RETURN (error, display);
}

EGLBoolean eglInitialize (EGLDisplay dpy, EGLint *major, EGLint *minor)
{
     D_LOG( DFBEGL_Api, DEBUG_1, "%s()\n", __FUNCTION__ );

     EGL::TLS *tls = EGLTLS.Get();

     if (!dpy)
          DFB_EGL_RETURN (EGL_BAD_DISPLAY, EGL_FALSE);

     EGLint _major = 0, _minor = 0;

     EGL::Display *display = (EGL::Display *) dpy;

     display->GetVersion( _major, _minor );

     if (major)
          *major = _major;
     if (minor)
          *minor = _minor;

     EGLint error = display->eglInitialise();

     DFB_EGL_RETURN (error, error == EGL_SUCCESS);
}

EGLBoolean eglTerminate (EGLDisplay dpy)
{
     D_LOG( DFBEGL_Api, DEBUG_1, "%s()\n", __FUNCTION__ );

     EGL::TLS *tls = EGLTLS.Get();

     if (!dpy)
          DFB_EGL_RETURN (EGL_BAD_DISPLAY, EGL_FALSE);


     EGL::Display *display = (EGL::Display *) dpy;

     // FIXME: Context: Check if context bound!

     EGLint error = display->Terminate();

     DFB_EGL_RETURN (error, error == EGL_SUCCESS);
}

const char* eglQueryString (EGLDisplay dpy, EGLint name)
{
     D_LOG( DFBEGL_Api, DEBUG_1, "%s()\n", __FUNCTION__ );

     EGL::TLS *tls = EGLTLS.Get();

     if (!dpy)
          DFB_EGL_RETURN (EGL_BAD_DISPLAY, NULL);


     EGL::Display *display = (EGL::Display *) dpy;
     const char   *result  = NULL;

     EGLint error = display->QueryString( name, result );

     DFB_EGL_RETURN (error, result);
}

EGLBoolean eglGetConfigs (EGLDisplay dpy,
                          EGLConfig *configs, EGLint config_size, EGLint *num_configs)
{
     EGL::TLS *tls = EGLTLS.Get();

     if (!dpy)
          DFB_EGL_RETURN (EGL_BAD_DISPLAY, EGL_FALSE);


     EGL::Display *display = (EGL::Display *) dpy;

     EGLint error = display->GetConfigs( (EGL::Config**) configs, config_size, num_configs );

     DFB_EGL_RETURN (error, error == EGL_SUCCESS);
}

EGLBoolean eglChooseConfig (EGLDisplay dpy, const EGLint *attrib_list,
                            EGLConfig *configs, EGLint config_size, EGLint *num_config)
{
     EGL::TLS *tls = EGLTLS.Get();

     if (!dpy)
          DFB_EGL_RETURN (EGL_BAD_DISPLAY, EGL_FALSE);


     EGL::Display *display = (EGL::Display *) dpy;

     EGLint error = display->ChooseConfig( attrib_list, (EGL::Config**) configs, config_size, num_config );

     DFB_EGL_RETURN (error, error == EGL_SUCCESS);
}

EGLBoolean eglGetConfigAttrib (EGLDisplay dpy, EGLConfig conf,
                               EGLint attribute, EGLint *value)
{
     EGL::TLS *tls = EGLTLS.Get();

     if (!dpy)
          DFB_EGL_RETURN (EGL_BAD_DISPLAY, EGL_FALSE);


     EGL::Config *config = (EGL::Config *) conf;

     EGLint error = config->GetAttrib( attribute, value );

     DFB_EGL_RETURN (error, error == EGL_SUCCESS);
}

EGLSurface eglCreatePixmapSurface (EGLDisplay dpy, EGLConfig conf,
                                   EGLNativePixmapType pixmap, const EGLint *attrib)
{
     EGL::TLS *tls = EGLTLS.Get();

     if (!dpy)
          DFB_EGL_RETURN (EGL_BAD_DISPLAY, EGL_NO_SURFACE);

     if (!pixmap)
          DFB_EGL_RETURN (EGL_BAD_NATIVE_PIXMAP, EGL_NO_SURFACE);


     EGL::Display *display = (EGL::Display *) dpy;
     EGL::Config  *config  = (EGL::Config *) conf;
     EGL::Surface *surface;

     EGLint error = display->CreatePixmapSurface( config, pixmap, attrib, &surface );
     if (error != EGL_SUCCESS)
          DFB_EGL_RETURN (error, EGL_NO_SURFACE);

     D_LOG( DFBEGL_Api, VERBOSE, "New DFBSurfaceID %u from Native Pixmap 0x%08lx\n", surface->GetID(), (long) pixmap );

     DFB_EGL_RETURN (EGL_SUCCESS, surface);
}

EGLSurface eglCreateWindowSurface (EGLDisplay dpy, EGLConfig conf,
                                   EGLNativeWindowType win, const EGLint *attrib)
{
     EGL::TLS *tls = EGLTLS.Get();

     if (!dpy)
          DFB_EGL_RETURN (EGL_BAD_DISPLAY, EGL_NO_SURFACE);


     EGL::Display *display = (EGL::Display *) dpy;
     EGL::Config  *config  = (EGL::Config *) conf;
     EGL::Surface *surface;

     EGLint error = display->CreateWindowSurface( config, win, attrib, &surface );
     if (error != EGL_SUCCESS)
          DFB_EGL_RETURN (error, EGL_NO_SURFACE);

     D_LOG( DFBEGL_Api, VERBOSE, "New DFBSurfaceID %u from Native Window 0x%08lx\n", surface->GetID(), (long) win );

     DFB_EGL_RETURN (EGL_SUCCESS, surface);
}

EGLSurface eglCreatePbufferSurface (EGLDisplay dpy, EGLConfig conf, const EGLint *attrib)
{
     EGL::TLS *tls = EGLTLS.Get();

     if (!dpy)
          DFB_EGL_RETURN (EGL_BAD_DISPLAY, EGL_NO_SURFACE);


     EGL::Display *display = (EGL::Display *) dpy;
     EGL::Config  *config  = (EGL::Config *) conf;
     EGL::Surface *surface;

     EGLint error = display->CreatePbufferSurface( config, attrib, &surface );
     if (error != EGL_SUCCESS)
          DFB_EGL_RETURN (error, EGL_NO_SURFACE);

     D_LOG( DFBEGL_Api, VERBOSE, "New DFBSurfaceID %u from Pbuffer\n", surface->GetID() );

     DFB_EGL_RETURN (EGL_SUCCESS, surface);
}

EGLSurface eglCreatePbufferFromClientBuffer (EGLDisplay dpy,
                                             EGLenum buftype, EGLClientBuffer buffer,
                                             EGLConfig conf, const EGLint *attrib)
{
     EGL::TLS *tls = EGLTLS.Get();

     if (!dpy)
          DFB_EGL_RETURN (EGL_BAD_DISPLAY, EGL_NO_SURFACE);

     if (!buftype || !buffer)
          DFB_EGL_RETURN (EGL_BAD_PARAMETER, EGL_NO_SURFACE);


     EGL::Display *display = (EGL::Display *) dpy;
     EGL::Config  *config  = (EGL::Config *) conf;
     EGL::Surface *surface;

     EGLint error = display->CreatePbufferFromClientBuffer( buftype, buffer, config, attrib, &surface );
     if (error != EGL_SUCCESS)
          DFB_EGL_RETURN (error, EGL_NO_SURFACE);

     D_LOG( DFBEGL_Api, VERBOSE, "New DFBSurfaceID %u from PbufferFromClientBuffer %p (type %s)\n",
            surface->GetID(), buffer, *ToString<EGL::EGLInt>( buftype ) );

     DFB_EGL_RETURN (EGL_SUCCESS, surface);
}

EGLBoolean eglDestroySurface (EGLDisplay dpy, EGLSurface surf)
{
     EGL::TLS *tls = EGLTLS.Get();

     if (!dpy)
          DFB_EGL_RETURN (EGL_BAD_DISPLAY, EGL_FALSE);

     if (!surf)
          DFB_EGL_RETURN (EGL_BAD_SURFACE, EGL_FALSE);


     EGL::Surface *surface = (EGL::Surface*)surf;

     delete surface;

     DFB_EGL_RETURN (EGL_SUCCESS, EGL_TRUE);
}

EGLBoolean eglSurfaceAttrib (EGLDisplay dpy, EGLSurface surf,
                             EGLint attribute, EGLint value)
{
     EGL::TLS *tls = EGLTLS.Get();

     if (!dpy)
          DFB_EGL_RETURN (EGL_BAD_DISPLAY, EGL_FALSE);

     if (!surf)
          DFB_EGL_RETURN (EGL_BAD_SURFACE, EGL_FALSE);


     EGL::Surface *surface = (EGL::Surface*) surf;

     EGLint error = surface->SetAttrib( attribute, value );

     DFB_EGL_RETURN (error, error == EGL_SUCCESS);
}

EGLBoolean eglQuerySurface (EGLDisplay dpy, EGLSurface surf,
                            EGLint attribute, EGLint *value)
{
     EGL::TLS *tls = EGLTLS.Get();

     if (!dpy)
          DFB_EGL_RETURN (EGL_BAD_DISPLAY, EGL_FALSE);

     if (!surf)
          DFB_EGL_RETURN (EGL_BAD_SURFACE, EGL_FALSE);

     if (!value)
          DFB_EGL_RETURN (EGL_BAD_PARAMETER, EGL_FALSE);


     EGL::Surface *surface = (EGL::Surface*) surf;

     EGLint error = surface->GetAttrib( attribute, *value );

     DFB_EGL_RETURN (error, error == EGL_SUCCESS);
}

EGLBoolean eglBindTexImage (EGLDisplay dpy, EGLSurface surface, EGLint buffer)
{
     EGL::TLS *tls = EGLTLS.Get();

     DFB_EGL_RETURN (EGL_BAD_PARAMETER, EGL_FALSE);
}

EGLBoolean eglReleaseTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer)
{
     EGL::TLS *tls = EGLTLS.Get();

     DFB_EGL_RETURN (EGL_BAD_PARAMETER, EGL_FALSE);
}


EGLBoolean eglBindAPI (EGLenum api)
{
     EGL::TLS *tls = EGLTLS.Get();

     tls->SetAPI( api );

     DFB_EGL_RETURN (EGL_SUCCESS, EGL_TRUE);
}

EGLenum eglQueryAPI (void)
{
     EGL::TLS *tls = EGLTLS.Get();

     return tls->GetAPI();
}

EGLContext eglCreateContext (EGLDisplay dpy,
                             EGLConfig config, EGLContext share_context,
                             const EGLint *attrib_list)
{
     EGL::TLS *tls = EGLTLS.Get();

     if (!dpy)
          DFB_EGL_RETURN (EGL_BAD_DISPLAY, EGL_NO_CONTEXT);


     EGL::Display *display = (EGL::Display *) dpy;
     EGL::Context *context;

     EGLint error = display->CreateContext( tls->GetAPI(), (EGL::Config *) config, (EGL::Context *) share_context, attrib_list, &context );
     if (error != EGL_SUCCESS)
          DFB_EGL_RETURN (error, EGL_NO_CONTEXT);

     DFB_EGL_RETURN (EGL_SUCCESS, (EGLContext) context);
}

EGLBoolean eglDestroyContext (EGLDisplay dpy, EGLContext ctx)
{
     EGL::TLS *tls = EGLTLS.Get();

     EGL::Context *context = (EGL::Context*) ctx;

     if (!dpy)
          DFB_EGL_RETURN (EGL_BAD_DISPLAY, EGL_FALSE);

     if (!ctx)
          DFB_EGL_RETURN (EGL_BAD_CONTEXT, EGL_FALSE);

     delete context;

     DFB_EGL_RETURN (EGL_SUCCESS, EGL_TRUE);
}

EGLBoolean eglMakeCurrent (EGLDisplay dpy,
                           EGLSurface draw, EGLSurface read, EGLContext ctx)
{
     EGL::TLS *tls = EGLTLS.Get();

     EGL::Context *context = (EGL::Context*) ctx;

     if (!dpy)
          DFB_EGL_RETURN (EGL_BAD_DISPLAY, EGL_FALSE);

     if ((ctx != NULL) != ((draw != NULL) || (read != NULL))) // TODO: support surface less context
          DFB_EGL_RETURN (EGL_BAD_MATCH, EGL_FALSE);


     EGL::Context *current  = tls->GetContext();
     EGL::Surface *egl_draw = (EGL::Surface*) draw;
     EGL::Surface *egl_read = (EGL::Surface*) read;

     if (current) {
          if (current == context &&
              current->GetSurface( EGL_DRAW ) == egl_draw &&
              current->GetSurface( EGL_READ ) == egl_read)
          {
               DFB_EGL_RETURN (EGL_SUCCESS, EGL_TRUE);
          }

          current->Unbind();
     }

     if (context) {
          EGLint error = context->Bind( egl_draw, egl_read );
          if (error != EGL_SUCCESS) {
               tls->SetContext( NULL );
               DFB_EGL_RETURN (error, EGL_FALSE);
          }

          tls->SetContext( context );
     }
     else
          tls->SetContext( NULL );

     DFB_EGL_RETURN (EGL_SUCCESS, EGL_TRUE);
}

EGLContext eglGetCurrentContext (void)
{
     EGL::TLS *tls = EGLTLS.Get();

     DFB_EGL_RETURN (EGL_SUCCESS, tls->GetContext());
}

EGLSurface eglGetCurrentSurface (EGLint which)
{
     EGL::TLS *tls = EGLTLS.Get();

     EGL::Context *context;

     if (which != EGL_DRAW && which != EGL_READ)
          DFB_EGL_RETURN (EGL_BAD_PARAMETER, EGL_NO_SURFACE);

     context = tls->GetContext();
     if (context)
          DFB_EGL_RETURN (EGL_SUCCESS, context->GetSurface(which));

     DFB_EGL_RETURN (EGL_BAD_CONTEXT, EGL_NO_SURFACE);
}

EGLDisplay eglGetCurrentDisplay (void)
{
     EGL::TLS *tls = EGLTLS.Get();

     EGL::Context *context;

     context = tls->GetContext();
     if (context)
          DFB_EGL_RETURN (EGL_SUCCESS, context->GetConfig()->GetDisplay());

     DFB_EGL_RETURN (EGL_SUCCESS, EGL_NO_DISPLAY);
}

EGLBoolean eglQueryContext (EGLDisplay dpy,
                            EGLContext ctx, EGLint attribute, EGLint *value)
{
     EGL::TLS *tls = EGLTLS.Get();

     if (!dpy)
          DFB_EGL_RETURN (EGL_BAD_DISPLAY, EGL_FALSE);

     if (!ctx)
          DFB_EGL_RETURN (EGL_BAD_CONTEXT, EGL_FALSE);

     if (!value)
          DFB_EGL_RETURN (EGL_BAD_PARAMETER, EGL_FALSE);


     EGL::Context *context = (EGL::Context *) ctx;

     EGLint error = context->GetAttrib( attribute, value );

     DFB_EGL_RETURN (error, error == EGL_SUCCESS);
}

EGLBoolean eglWaitClient (void)
{
     EGL::TLS *tls = EGLTLS.Get();

     DFB_EGL_RETURN (EGL_SUCCESS, EGL_TRUE);
}

EGLBoolean eglWaitGL (void)
{
     EGL::TLS *tls = EGLTLS.Get();

     DFB_EGL_RETURN (EGL_SUCCESS, EGL_TRUE);
}

EGLBoolean eglWaitNative (EGLint engine)
{
     EGL::TLS *tls = EGLTLS.Get();

     DFB_EGL_RETURN (EGL_SUCCESS, EGL_TRUE);
}

EGLBoolean eglSwapBuffers (EGLDisplay dpy, EGLSurface surf)
{
     EGL::TLS *tls = EGLTLS.Get();

     if (!dpy)
          DFB_EGL_RETURN (EGL_BAD_DISPLAY, EGL_FALSE);

     if (!surf)
          DFB_EGL_RETURN (EGL_BAD_SURFACE, EGL_FALSE);


     EGL::Display *display = (EGL::Display *) dpy;
     EGL::Surface *surface = (EGL::Surface*) surf;

     EGLint error = display->SwapBuffers( surface );

     DFB_EGL_RETURN (EGL_SUCCESS, error == EGL_SUCCESS);
}

EGLBoolean eglCopyBuffers (EGLDisplay dpy,
                           EGLSurface src, EGLNativePixmapType dst)
{
     EGL::TLS *tls = EGLTLS.Get();

     if (!dpy)
          DFB_EGL_RETURN (EGL_BAD_DISPLAY, EGL_FALSE);

     if (!src)
          DFB_EGL_RETURN (EGL_BAD_SURFACE, EGL_FALSE);

     if (!dst)
          DFB_EGL_RETURN (EGL_BAD_NATIVE_PIXMAP, EGL_FALSE);


     EGL::Display *display = (EGL::Display *) dpy;
     EGL::Surface *source  = (EGL::Surface*) src;

     EGLint error = display->CopyBuffers( source, dst );
     if (error != EGL_SUCCESS)
          DFB_EGL_RETURN (error, EGL_FALSE);

     DFB_EGL_RETURN (EGL_SUCCESS, EGL_TRUE);
}

EGLBoolean eglSwapInterval (EGLDisplay dpy, EGLint interval)
{
     EGL::TLS *tls = EGLTLS.Get();

     if (!dpy)
          DFB_EGL_RETURN (EGL_BAD_DISPLAY, EGL_FALSE);

     DFB_EGL_RETURN (EGL_SUCCESS, EGL_TRUE);
}

EGLBoolean eglReleaseThread (void)
{
     EGL::TLS *tls = EGLTLS.Get();

     DFB_EGL_RETURN (EGL_SUCCESS, EGL_TRUE);
}

/*****************************************************************************/

class Symbols : public std::map<std::string,__eglMustCastToProperFunctionPointerType>
{
public:
     Symbols()
     {
          // FIXME: fill with all functions
     }
};

__eglMustCastToProperFunctionPointerType
eglGetProcAddress( const char *procname )
{
     __eglMustCastToProperFunctionPointerType addr = NULL;

     if (procname) {
          D_LOG( DFBEGL_Api, DEBUG_1, "%s( '%s' )\n", __FUNCTION__, procname );

          static Symbols symbols;

#if 0
          EGL::TLS     *tls = EGLTLS.Get();
          EGL::Context *context;

          context = tls->GetContext();
          if (context) {
               D_LOG( DFBEGL_Api, DEBUG_1, "  -> checking context at %p\n", context );

               DFBResult ret = context->GetProcAddress( procname, (void**) &addr );
               if (ret == DFB_OK)
                    return addr;
          }
          else
               D_LOG( DFBEGL_Api, DEBUG_1, "  -> no context bound\n" );
#endif

          try {
               addr = (__eglMustCastToProperFunctionPointerType) EGL::Core::Call<EGL::Core::GetProcAddress>( procname )( procname );

               D_DEBUG_AT( DFBEGL_Api, "  => %p\n", addr );

               return addr;
          }
          catch (std::bad_function_call &e) {
               D_DEBUG_AT( DFBEGL_Api, "  => %s\n", e.what() );
          }


          Symbols::const_iterator it = symbols.find( procname );

          if (it != symbols.end())
               return (*it).second;
     }

     return addr;
}



}


}

