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

#ifndef ___DFBEGL__library__H___
#define ___DFBEGL__library__H___

#include "dfbegl.h"


#ifdef __cplusplus
extern "C" {
#endif


// C wrappers


#ifdef __cplusplus
}

#include <direct/Map.h>
#include <direct/String.h>
#include <direct/TLSObject.h>

#include <core/CoreSurface.h>
#include <core/Graphics.h>


#include <list>
#include <map>


namespace DirectFB {


namespace EGL {


class Symbols
{
public:
     Symbols() {
          ClearTable();
     }

     void ClearTable() {
          memset( this, 0, sizeof(Symbols) );
     }

     EGLint (*eglGetError)(void);

     EGLDisplay (*eglGetDisplay)(EGLNativeDisplayType display_id);
     EGLBoolean (*eglInitialize)(EGLDisplay dpy, EGLint *major, EGLint *minor);
     EGLBoolean (*eglTerminate)(EGLDisplay dpy);

     const char * (*eglQueryString)(EGLDisplay dpy, EGLint name);

     EGLBoolean (*eglGetConfigs)(EGLDisplay dpy, EGLConfig *configs,
                                 EGLint config_size, EGLint *num_config);
     EGLBoolean (*eglChooseConfig)(EGLDisplay dpy, const EGLint *attrib_list,
                                   EGLConfig *configs, EGLint config_size,
                                   EGLint *num_config);
     EGLBoolean (*eglGetConfigAttrib)(EGLDisplay dpy, EGLConfig config,
                                      EGLint attribute, EGLint *value);

     EGLSurface (*eglCreateWindowSurface)(EGLDisplay dpy, EGLConfig config,
                                          EGLNativeWindowType win,
                                          const EGLint *attrib_list);
     EGLSurface (*eglCreatePbufferSurface)(EGLDisplay dpy, EGLConfig config,
                                           const EGLint *attrib_list);
     EGLSurface (*eglCreatePixmapSurface)(EGLDisplay dpy, EGLConfig config,
                                          EGLNativePixmapType pixmap,
                                          const EGLint *attrib_list);
     EGLBoolean (*eglDestroySurface)(EGLDisplay dpy, EGLSurface surface);
     EGLBoolean (*eglQuerySurface)(EGLDisplay dpy, EGLSurface surface,
                                   EGLint attribute, EGLint *value);

     EGLBoolean (*eglBindAPI)(EGLenum api);
     EGLenum (*eglQueryAPI)(void);

     EGLBoolean (*eglWaitClient)(void);

     EGLBoolean (*eglReleaseThread)(void);

     EGLSurface (*eglCreatePbufferFromClientBuffer)(EGLDisplay dpy, EGLenum buftype, EGLClientBuffer buffer,
                                                    EGLConfig config, const EGLint *attrib_list);

     EGLBoolean (*eglSurfaceAttrib)(EGLDisplay dpy, EGLSurface surface,
                                    EGLint attribute, EGLint value);
     EGLBoolean (*eglBindTexImage)(EGLDisplay dpy, EGLSurface surface, EGLint buffer);
     EGLBoolean (*eglReleaseTexImage)(EGLDisplay dpy, EGLSurface surface, EGLint buffer);


     EGLBoolean (*eglSwapInterval)(EGLDisplay dpy, EGLint interval);


     EGLContext (*eglCreateContext)(EGLDisplay dpy, EGLConfig config,
                                    EGLContext share_context,
                                    const EGLint *attrib_list);
     EGLBoolean (*eglDestroyContext)(EGLDisplay dpy, EGLContext ctx);
     EGLBoolean (*eglMakeCurrent)(EGLDisplay dpy, EGLSurface draw,
                                  EGLSurface read, EGLContext ctx);

     EGLContext (*eglGetCurrentContext)(void);
     EGLSurface (*eglGetCurrentSurface)(EGLint readdraw);
     EGLDisplay (*eglGetCurrentDisplay)(void);
     EGLBoolean (*eglQueryContext)(EGLDisplay dpy, EGLContext ctx,
                                   EGLint attribute, EGLint *value);

     EGLBoolean (*eglWaitGL)(void);
     EGLBoolean (*eglWaitNative)(EGLint engine);
     EGLBoolean (*eglSwapBuffers)(EGLDisplay dpy, EGLSurface surface);
     EGLBoolean (*eglCopyBuffers)(EGLDisplay dpy, EGLSurface surface,
                                  EGLNativePixmapType target);

     EGLImageKHR (*eglCreateImageKHR) (EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list);
     EGLBoolean  (*eglDestroyImageKHR) (EGLDisplay dpy, EGLImageKHR image);

     /* This is a generic function pointer type, whose name indicates it must
      * be cast to the proper type *and calling convention* before use.
      */
     typedef void (*__eglMustCastToProperFunctionPointerType)(void);

     /* Now, define eglGetProcAddress using the generic function ptr. type */
     __eglMustCastToProperFunctionPointerType (*eglGetProcAddress)(const char *procname);
};

class Library : public Symbols
{
private:
     Direct::String  filename;
     bool            global;
     bool            now;

     void           *handle;

public:
     Library();
     ~Library();

     DFBResult  Init( const Direct::String &filename, bool global = false, bool now = false );
     DFBResult  Load();
     DFBResult  Unload();
     void      *Lookup( const Direct::String &symbol );

private:
     DFBResult  Open();
};


}

}


#endif

#endif

