/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
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

#if FUSION_BUILD_MULTI && FUSION_BUILD_KERNEL

#include <linux/fusion.h>

#define FUSION_API_MAJOR_REQUIRED 8
#define FUSION_API_MINOR_REQUIRED 0

#if FUSION_API_MAJOR_REQUIRED > FUSION_API_MAJOR_PROVIDED
#error Major version of Fusion Kernel Module too low! Upgrade your kernel.
#else
#if FUSION_API_MAJOR_REQUIRED == FUSION_API_MAJOR_PROVIDED
#if FUSION_API_MINOR_REQUIRED > FUSION_API_MINOR_PROVIDED
#error Minor version of Fusion Kernel Module too low! Upgrade your kernel.
#endif
#endif
#endif

#else
typedef unsigned long FusionID;

#define FUSION_ID_MASTER 1L

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

