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



#ifndef __CORETYPES_H__
#define __CORETYPES_H__

#include <dfb_types.h>

typedef struct __DFB_CoreDFB                 CoreDFB;
typedef struct __DFB_CoreDFBShared           CoreDFBShared;


typedef struct __DFB_DFBClipboardCore        DFBClipboardCore;
typedef struct __DFB_DFBColorHashCore        DFBColorHashCore;
typedef struct __DFB_DFBGraphicsCore         DFBGraphicsCore;
typedef struct __DFB_DFBInputCore            DFBInputCore;
typedef struct __DFB_DFBLayerCore            DFBLayerCore;
typedef struct __DFB_DFBScreenCore           DFBScreenCore;
typedef struct __DFB_DFBSystemCore           DFBSystemCore;
typedef struct __DFB_DFBWMCore               DFBWMCore;


typedef struct __DFB_DFBGraphicsCore         CoreGraphicsDevice; /* FIXME */

typedef struct __DFB_CoreGraphicsState       CoreGraphicsState;
typedef struct __DFB_CoreGraphicsStateClient CoreGraphicsStateClient;


typedef struct _CoreCleanup                  CoreCleanup;

typedef struct _CoreFont                     CoreFont;
typedef struct _CoreGlyphData                CoreGlyphData;
typedef struct _CorePalette                  CorePalette;

typedef struct _CardState                    CardState;


typedef struct __DFB_DFBFontManager          DFBFontManager;
typedef struct __DFB_DFBFontCache            DFBFontCache;
typedef struct __DFB_DFBFontCacheRow         DFBFontCacheRow;


typedef struct __DFB_CoreGraphicsSerial      CoreGraphicsSerial;

typedef struct __DFB_CoreScreen              CoreScreen;

typedef struct __DFB_CoreInputDevice         CoreInputDevice;

typedef struct __DFB_CoreLayer               CoreLayer;
typedef struct __DFB_CoreLayerContext        CoreLayerContext;
typedef struct __DFB_CoreLayerRegion         CoreLayerRegion;
typedef struct __DFB_CoreLayerRegionConfig   CoreLayerRegionConfig;

typedef struct __DFB_CoreSurface             CoreSurface;
typedef struct __DFB_CoreSurfaceAccessor     CoreSurfaceAccessor;
typedef struct __DFB_CoreSurfaceAllocation   CoreSurfaceAllocation;
typedef struct __DFB_CoreSurfaceBuffer       CoreSurfaceBuffer;
typedef struct __DFB_CoreSurfaceBufferLock   CoreSurfaceBufferLock;
typedef struct __DFB_CoreSurfaceClient       CoreSurfaceClient;
typedef struct __DFB_CoreSurfacePool         CoreSurfacePool;
typedef struct __DFB_CoreSurfacePoolBridge   CoreSurfacePoolBridge;
typedef struct __DFB_CoreSurfacePoolTransfer CoreSurfacePoolTransfer;

typedef struct __DFB_CoreWindow              CoreWindow;
typedef struct __DFB_CoreWindowConfig        CoreWindowConfig;
typedef struct __DFB_CoreWindowStack         CoreWindowStack;


typedef unsigned int CoreSurfacePoolID;
typedef unsigned int CoreSurfacePoolBridgeID;



#ifdef __cplusplus

#include <direct/Types++.h>

namespace DirectFB {
namespace Util {
     class FPS;
}
class Renderer;
class Task;
class Throttle;
class SurfaceTask;
class DisplayTask;
}
#define DFB_Util_FPS               DirectFB::Util::FPS
#define DFB_Renderer               DirectFB::Renderer
#define DFB_Task                   DirectFB::Task
#define DFB_TaskList               Direct::List<DirectFB::Task*>
#define DFB_TaskListLocked         Direct::ListLocked<DirectFB::Task*>
#define DFB_TaskListSimple         Direct::ListSimple<DirectFB::Task*>
#define DFB_Throttle               DirectFB::Throttle
#define DFB_SurfaceTask            DirectFB::SurfaceTask
#define DFB_SurfaceTaskList        Direct::List<DirectFB::SurfaceTask*>
#define DFB_SurfaceTaskListLocked  Direct::ListLocked<DirectFB::SurfaceTask*>
#define DFB_SurfaceTaskListSimple  Direct::ListSimple<DirectFB::SurfaceTask*>
#define DFB_DisplayTask            DirectFB::DisplayTask
#define DFB_DisplayTaskList        Direct::List<DirectFB::DisplayTask*>
#define DFB_DisplayTaskListLocked  Direct::ListLocked<DirectFB::DisplayTask*>
#define DFB_DisplayTaskListSimple  Direct::ListSimple<DirectFB::DisplayTask*>
#else
typedef void DFB_Util_FPS;
typedef void DFB_Renderer;
typedef void DFB_Task;
typedef void DFB_TaskList;
typedef void DFB_TaskListLocked;
typedef void DFB_TaskListSimple;
typedef void DFB_Throttle;
typedef void DFB_SurfaceTask;
typedef void DFB_SurfaceTaskList;
typedef void DFB_SurfaceTaskListLocked;
typedef void DFB_SurfaceTaskListSimple;
typedef void DFB_DisplayTask;
typedef void DFB_DisplayTaskList;
typedef void DFB_DisplayTaskListLocked;
typedef void DFB_DisplayTaskListSimple;
#endif


#endif

