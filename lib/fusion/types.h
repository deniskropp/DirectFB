/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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

#ifndef __FUSION__TYPES_H__
#define __FUSION__TYPES_H__

#include <fusion/build.h>

#if FUSION_BUILD_MULTI

#include <linux/fusion.h>

#if FUSION_API_MAJOR != 3
#error Need major API version 3!
#else
#if FUSION_API_MINOR < 2
#error Insufficient minor API version, need 3.2 at least!
#endif
#endif

#else
typedef unsigned long FusionID;

typedef enum {
     FCEF_NONE     = 0x00000000,
     FCEF_ONEWAY   = 0x00000001,
     FCEF_ALL      = 0x00000001
} FusionCallExecFlags;

#endif

#define FCEF_NODIRECT 0x80000000

#include <direct/types.h>


typedef struct __Fusion_FusionConfig         FusionConfig;

typedef struct __Fusion_FusionArena          FusionArena;
typedef struct __Fusion_FusionReactor        FusionReactor;
typedef struct __Fusion_FusionWorld          FusionWorld;
typedef struct __Fusion_FusionWorldShared    FusionWorldShared;

typedef struct __Fusion_FusionObject         FusionObject;
typedef struct __Fusion_FusionObjectPool     FusionObjectPool;

typedef struct __Fusion_FusionSHM            FusionSHM;
typedef struct __Fusion_FusionSHMShared      FusionSHMShared;

typedef struct __Fusion_FusionSHMPool        FusionSHMPool;
typedef struct __Fusion_FusionSHMPoolShared  FusionSHMPoolShared;
typedef struct __Fusion_FusionHash           FusionHash;

#endif

