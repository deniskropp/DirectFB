/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#include <asm/types.h>

typedef struct _CoreCleanup             CoreCleanup;

typedef struct _CoreFont                CoreFont;
typedef struct _CorePalette             CorePalette;
typedef struct _CoreSurface             CoreSurface;
typedef struct _CoreThread              CoreThread;
typedef struct _CoreWindow              CoreWindow;
typedef struct _CoreWindowStack         CoreWindowStack;

typedef struct _SurfaceBuffer           SurfaceBuffer;
typedef struct _SurfaceManager          SurfaceManager;

typedef struct _CardState               CardState;

typedef struct _Tree                    Tree;
typedef struct _Chunk                   Chunk;

typedef __u32 unichar;



typedef struct _DisplayLayer            DisplayLayer;
typedef struct _InputDevice             InputDevice;
typedef struct _FBDev                   FBDev;
typedef struct _VirtualTerminal         VirtualTerminal;
typedef struct _GraphicsDevice          GraphicsDevice;



#endif

