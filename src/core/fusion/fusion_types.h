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

#ifndef __FUSION__TYPES_H__
#define __FUSION__TYPES_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <direct/types.h>


typedef enum {
     FUSION_SUCCESS   = 0,
     FUSION_FAILURE,
     FUSION_BUG,
     FUSION_UNIMPLEMENTED,
     FUSION_INVARG,
     FUSION_DESTROYED,
     FUSION_ACCESSDENIED,
     FUSION_PERMISSIONDENIED,
     FUSION_NOTEXISTENT,
     FUSION_LIMITREACHED,
     FUSION_INUSE,
     FUSION_TIMEOUT,
     FUSION_OUTOFSHAREDMEMORY
} FusionResult;

typedef struct _FusionReactor      FusionReactor;
typedef struct _FusionArena        FusionArena;

typedef struct _FusionObject       FusionObject;
typedef struct _FusionObjectPool   FusionObjectPool;


#ifdef __cplusplus
}
#endif

#endif /* __FUSION__TYPES_H__ */

