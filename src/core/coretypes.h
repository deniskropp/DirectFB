/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#ifndef __CORETYPES_H__
#define __CORETYPES_H__

#include <dfb_types.h>

typedef struct __DFB_CoreDFB               CoreDFB;
typedef struct __DFB_CoreDFBShared         CoreDFBShared;

typedef struct _CoreCleanup                CoreCleanup;

typedef struct _CoreFont                   CoreFont;
typedef struct _CorePalette                CorePalette;
typedef struct _CoreSurface                CoreSurface;

typedef struct _SurfaceBuffer              SurfaceBuffer;
typedef struct _SurfaceManager             SurfaceManager;

typedef struct _CardState                  CardState;

typedef struct _Chunk                      Chunk;


typedef struct _InputDevice                InputDevice;
typedef struct _GraphicsDevice             GraphicsDevice;

typedef struct __DFB_CoreScreen            CoreScreen;

typedef struct __DFB_CoreLayer             CoreLayer;
typedef struct __DFB_CoreLayerContext      CoreLayerContext;
typedef struct __DFB_CoreLayerRegion       CoreLayerRegion;
typedef struct __DFB_CoreLayerRegionConfig CoreLayerRegionConfig;

typedef struct __DFB_CoreWindow            CoreWindow;
typedef struct __DFB_CoreWindowStack       CoreWindowStack;

#endif

