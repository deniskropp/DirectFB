/*
   (c) Copyright 2001-2008  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

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

#ifndef __GL_GFXDRIVER_H__
#define __GL_GFXDRIVER_H__

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <GL/glx.h>

#include <x11/glx_surface_pool.h>

typedef struct {
     /* validation flags */
     int                    v_flags;

     /** Add shared data here... **/
} GLDeviceData;


typedef struct {
     /* Server connection and main visual */
     Display                 *display;
     XVisualInfo             *visual;

     /* Every thread needs its own context! */
     pthread_key_t            context_key;
     bool                     context_key_valid;

     DFBSurfaceBlittingFlags  blittingflags;

     /* Flush every bunch of commands to avoid issue with the XServer... */
     unsigned int             calls;

     /** Add local data here... **/
} GLDriverData;


#endif
