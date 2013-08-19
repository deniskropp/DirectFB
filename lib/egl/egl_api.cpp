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


#define DFBEGL_ENABLE_MANGLE

#include <egl/dfbegl.h>

#include <EGL/egldfbunmangle.h>


extern "C" {
#include <direct/messages.h>
}

#include <direct/ToString.h>




extern "C" {

EGLint eglGetError (void)
{
     return dfbeglGetError();
}

EGLDisplay eglGetDisplay (EGLNativeDisplayType native_display)
{
     return dfbeglGetDisplay( native_display );
}

EGLBoolean eglInitialize (EGLDisplay dpy, EGLint *major, EGLint *minor)
{
     return dfbeglInitialize( dpy, major, minor );
}

EGLBoolean eglTerminate (EGLDisplay dpy)
{
     return dfbeglTerminate( dpy );
}

const char* eglQueryString (EGLDisplay dpy, EGLint name)
{
     return dfbeglQueryString( dpy, name );
}

EGLBoolean eglGetConfigs (EGLDisplay dpy,
                          EGLConfig *configs, EGLint config_size, EGLint *num_configs)
{
     return dfbeglGetConfigs( dpy, configs, config_size, num_configs );
}

EGLBoolean eglChooseConfig (EGLDisplay dpy, const EGLint *attrib_list,
                            EGLConfig *configs, EGLint config_size, EGLint *num_config)
{
     return dfbeglChooseConfig( dpy, attrib_list, configs, config_size, num_config );
}

EGLBoolean eglGetConfigAttrib (EGLDisplay dpy, EGLConfig conf,
                               EGLint attribute, EGLint *value)
{
     return dfbeglGetConfigAttrib( dpy, conf, attribute, value );
}

EGLSurface eglCreatePixmapSurface (EGLDisplay dpy, EGLConfig conf,
                                   EGLNativePixmapType pixmap, const EGLint *attrib)
{
     return dfbeglCreatePixmapSurface( dpy, conf, pixmap, attrib );
}

EGLSurface eglCreateWindowSurface (EGLDisplay dpy, EGLConfig conf,
                                   EGLNativeWindowType win, const EGLint *attrib)
{
     return dfbeglCreateWindowSurface( dpy, conf, win, attrib );
}

EGLSurface eglCreatePbufferSurface (EGLDisplay dpy, EGLConfig conf, const EGLint *attrib)
{
     return dfbeglCreatePbufferSurface( dpy, conf, attrib );
}

EGLSurface eglCreatePbufferFromClientBuffer (EGLDisplay dpy,
                                             EGLenum buftype, EGLClientBuffer buffer,
                                             EGLConfig conf, const EGLint *attrib)
{
     return dfbeglCreatePbufferFromClientBuffer( dpy, buftype, buffer, conf, attrib );
}

EGLBoolean eglDestroySurface (EGLDisplay dpy, EGLSurface surf)
{
     return dfbeglDestroySurface( dpy, surf );
}

EGLBoolean eglSurfaceAttrib (EGLDisplay dpy, EGLSurface surf,
                             EGLint attribute, EGLint value)
{
     return dfbeglSurfaceAttrib( dpy, surf, attribute, value );
}

EGLBoolean eglQuerySurface (EGLDisplay dpy, EGLSurface surf,
                            EGLint attribute, EGLint *value)
{
     return dfbeglQuerySurface( dpy, surf, attribute, value );
}

EGLBoolean eglBindTexImage (EGLDisplay dpy, EGLSurface surface, EGLint buffer)
{
     return dfbeglBindTexImage( dpy, surface, buffer );
}

EGLBoolean eglReleaseTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer)
{
     return dfbeglReleaseTexImage( dpy, surface, buffer );
}


EGLBoolean eglBindAPI (EGLenum api)
{
     return dfbeglBindAPI( api );
}

EGLenum eglQueryAPI (void)
{
     return dfbeglQueryAPI();
}

EGLContext eglCreateContext (EGLDisplay dpy,
                             EGLConfig config, EGLContext share_context,
                             const EGLint *attrib_list)
{
     return dfbeglCreateContext( dpy, config, share_context, attrib_list );
}

EGLBoolean eglDestroyContext (EGLDisplay dpy, EGLContext ctx)
{
     return dfbeglDestroyContext( dpy, ctx );
}

EGLBoolean eglMakeCurrent (EGLDisplay dpy,
                           EGLSurface draw, EGLSurface read, EGLContext ctx)
{
     return dfbeglMakeCurrent( dpy, draw, read, ctx );
}

EGLContext eglGetCurrentContext (void)
{
     return dfbeglGetCurrentContext();
}

EGLSurface eglGetCurrentSurface (EGLint which)
{
     return dfbeglGetCurrentSurface( which );
}

EGLDisplay eglGetCurrentDisplay (void)
{
     return dfbeglGetCurrentDisplay();
}

EGLBoolean eglQueryContext (EGLDisplay dpy,
                            EGLContext ctx, EGLint attribute, EGLint *value)
{
     return dfbeglQueryContext( dpy, ctx, attribute, value );
}

EGLBoolean eglWaitClient (void)
{
     return dfbeglWaitClient();
}

EGLBoolean eglWaitGL (void)
{
     return dfbeglWaitGL();
}

EGLBoolean eglWaitNative (EGLint engine)
{
     return dfbeglWaitNative( engine );
}

EGLBoolean eglSwapBuffers (EGLDisplay dpy, EGLSurface surf)
{
     return dfbeglSwapBuffers( dpy, surf );
}

EGLBoolean eglCopyBuffers (EGLDisplay dpy,
                           EGLSurface src, EGLNativePixmapType dst)
{
     return dfbeglCopyBuffers( dpy, src, dst );
}

EGLBoolean eglSwapInterval (EGLDisplay dpy, EGLint interval)
{
     return dfbeglSwapInterval( dpy, interval );
}

EGLBoolean eglReleaseThread (void)
{
     return dfbeglReleaseThread();
}

__eglMustCastToProperFunctionPointerType
eglGetProcAddress( const char *procname )
{
     return dfbeglGetProcAddress( procname );
}



}

