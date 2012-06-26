/*
   (c) Copyright 2001-2012  The world wide DirectFB Open Source Community (directfb.org)
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

#ifndef __ANDROID_SURFACE_POOL_GR_H__
#define __ANDROID_SURFACE_POOL_GR_H__

#include "hardware/gralloc.h"
#include "hardware/hardware.h"
#include "system/window.h"

typedef struct {
     int             magic;
} FBOPoolData;

typedef struct {
     int                  magic;

     CoreDFB             *core;

     AndroidData         *data;
} FBOPoolLocalData;

typedef struct {
     int                    magic;

     int                    pitch;
     int                    size;

     EGLImageKHR            image;

     GLuint                 texture;
     GLuint                 fbo;
     GLuint                 color_rb;
     GLuint                 depth_rb;

     int                    fb_ready;

     ANativeWindowBuffer_t *win_buf;
     const hw_module_t     *hw_mod;
     gralloc_module_t      *gralloc_mod;
     alloc_device_t        *alloc_mod;

     int                    layer_flip;
} FBOAllocationData;

#endif

