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

extern "C" {
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

#include <direct/messages.h>
}

#include <direct/ToString.h>

#include <egl/dfbegl.h>


D_LOG_DOMAIN( DFBEGL_Library, "DFBEGL/Library", "DirectFB EGL Library" );


namespace DirectFB {

namespace EGL {

#define EGL_LOOKUP_SYMBOL(sym)                                                                      \
     do {                                                                                           \
          this->sym = (__typeof__(this->sym))dlsym( handle, #sym );                                 \
          if (!this->sym) this->sym = (__typeof__(this->sym))(this->eglGetProcAddress)( #sym );     \
          D_DEBUG_AT( DFBEGL_Library, "  -> %-20s : %p\n", #sym, this->sym );                       \
     } while (0)


Library::Library()
{
     D_DEBUG_AT( DFBEGL_Library, "EGL::Library::%s( %p )\n",
                 __FUNCTION__, this );

     memset( this, 0, sizeof(*this) );
}

Library::~Library()
{
     D_DEBUG_AT( DFBEGL_Library, "EGL::Library::%s( %p )\n",
                 __FUNCTION__, this );

     if (handle)
          dlclose( handle );
}

DFBResult
Library::Init( const Direct::String &filename, bool global, bool now )
{
     D_DEBUG_AT( DFBEGL_Library, "EGL::Library::%s( %p, '%s', global %s, now %s )\n",
                 __FUNCTION__, this, *filename, *ToString<bool>(global), *ToString<bool>(now) );

     if (handle) {
          dlclose( handle );

          memset( this, 0, sizeof(*this) );
     }

     handle = dlopen( *filename, (global ? RTLD_GLOBAL : RTLD_LOCAL) | (now ? RTLD_NOW : RTLD_LAZY) );
     if (!handle) {
          D_ERROR( "DFBEGL/Library: Could not dlopen '%s' (%s)\n", *filename, dlerror() );
          return DFB_IO;
     }

     this->eglGetDisplay = (void* (*)(EGLNativeDisplayType)) Lookup( "eglGetDisplay" );

     D_DEBUG_AT( DFBEGL_Library, "  -> eglGetDisplay at 0x%08lx\n", (long) this->eglGetDisplay );

     if (!this->eglGetDisplay) {
          D_ERROR( "DFBEGL/Library: Could not lookup 'eglGetDisplay' (%s)\n", dlerror() );
          dlclose( handle );
          handle = NULL;
          return DFB_IO;
     }

     return DFB_OK;
}

DFBResult
Library::Load()
{
     D_DEBUG_AT( DFBEGL_Library, "EGL::Library::%s( %p )\n", __FUNCTION__, this );

     if (!handle)
          return DFB_IO;

     this->eglGetProcAddress = (void (* (*)(const char*))()) dlsym( handle, "eglGetProcAddress" );

     D_DEBUG_AT( DFBEGL_Library, "  -> eglGetProcAddress at 0x%08lx\n", (long) this->eglGetProcAddress );

     if (!this->eglGetProcAddress) {
          D_ERROR( "DFBEGL/Library: Could not lookup 'eglGetProcAddress' (%s)\n", dlerror() );
          return DFB_IO;
     }

     EGL_LOOKUP_SYMBOL( eglGetError );
     EGL_LOOKUP_SYMBOL( eglInitialize );
     EGL_LOOKUP_SYMBOL( eglTerminate );

     EGL_LOOKUP_SYMBOL( eglQueryString );

     EGL_LOOKUP_SYMBOL( eglGetConfigs );
     EGL_LOOKUP_SYMBOL( eglChooseConfig );
     EGL_LOOKUP_SYMBOL( eglGetConfigAttrib );

     EGL_LOOKUP_SYMBOL( eglCreateWindowSurface );
     EGL_LOOKUP_SYMBOL( eglCreatePbufferSurface );
     EGL_LOOKUP_SYMBOL( eglCreatePixmapSurface );
     EGL_LOOKUP_SYMBOL( eglDestroySurface );
     EGL_LOOKUP_SYMBOL( eglQuerySurface );

     EGL_LOOKUP_SYMBOL( eglBindAPI );
     EGL_LOOKUP_SYMBOL( eglQueryAPI );

     EGL_LOOKUP_SYMBOL( eglWaitClient );

     EGL_LOOKUP_SYMBOL( eglReleaseThread );

     EGL_LOOKUP_SYMBOL( eglCreatePbufferFromClientBuffer );

     EGL_LOOKUP_SYMBOL( eglSurfaceAttrib );
     EGL_LOOKUP_SYMBOL( eglBindTexImage );
     EGL_LOOKUP_SYMBOL( eglReleaseTexImage );

     EGL_LOOKUP_SYMBOL( eglSwapInterval );

     EGL_LOOKUP_SYMBOL( eglCreateContext );
     EGL_LOOKUP_SYMBOL( eglDestroyContext );
     EGL_LOOKUP_SYMBOL( eglMakeCurrent );

     EGL_LOOKUP_SYMBOL( eglGetCurrentContext );
     EGL_LOOKUP_SYMBOL( eglGetCurrentSurface );
     EGL_LOOKUP_SYMBOL( eglGetCurrentDisplay );
     EGL_LOOKUP_SYMBOL( eglQueryContext );

     EGL_LOOKUP_SYMBOL( eglWaitGL );
     EGL_LOOKUP_SYMBOL( eglWaitNative );
     EGL_LOOKUP_SYMBOL( eglSwapBuffers );
     EGL_LOOKUP_SYMBOL( eglCopyBuffers );

     EGL_LOOKUP_SYMBOL( eglCreateImageKHR );
     EGL_LOOKUP_SYMBOL( eglDestroyImageKHR );

     return DFB_OK;
}

void *
Library::Lookup( const Direct::String &symbol )
{
     void *ret = NULL;

     D_DEBUG_AT( DFBEGL_Library, "EGL::Library::%s( %p, '%s' )\n",
                 __FUNCTION__, this, *symbol );

     if (handle)
          ret = dlsym( handle, *symbol );
     else
          D_DEBUG_AT( DFBEGL_Library, "  -> NOT LOADED\n" );


     D_DEBUG_AT( DFBEGL_Library, "  => %p\n", ret );

     if (!ret && this->eglGetProcAddress) {
          D_DEBUG_AT( DFBEGL_Library, "  -> trying eglGetProcAddress...\n" );
          ret = (void*) this->eglGetProcAddress( *symbol );
     }


     D_DEBUG_AT( DFBEGL_Library, "  => %p\n", ret );

     return ret;
}



}

}

