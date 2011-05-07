/*
   (c) Copyright 2001-2011  The world wide DirectFB Open Source Community (directfb.org)
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

#ifndef __CORE__CORE_SURFACE_H__
#define __CORE__CORE_SURFACE_H__


#include <core/surface.h>


/**********************************************************************************************************************
 * CoreSurface
 */

/*
 * CoreSurface Calls
 */
typedef enum {
     CORE_SURFACE_SET_CONFIG       = 1,
     CORE_SURFACE_LOCK_BUFFER      = 2,
     CORE_SURFACE_UNLOCK_BUFFER    = 3,
     CORE_SURFACE_FLIP             = 4,
     CORE_SURFACE_SET_PALETTE      = 5,
} CoreSurfaceCall;

/*
 * CORE_SURFACE_SET_CONFIG
 */
typedef struct {
     CoreSurfaceConfig      config;
     CoreSurfaceConfigFlags flags;
} CoreSurfaceSetConfig;

/*
 * CORE_SURFACE_LOCK_BUFFER
 */
typedef struct {
     CoreSurfaceBufferRole  role;
     CoreSurfaceAccessFlags access;
} CoreSurfaceLockBuffer;

/*
 * CORE_SURFACE_UNLOCK_BUFFER
 */
typedef struct {
     CoreSurfaceBufferRole  role;
     CoreSurfaceAccessFlags access;
} CoreSurfaceUnlockBuffer;

/*
 * CORE_SURFACE_FLIP
 */
typedef struct {
     bool                   swap;
} CoreSurfaceFlip;

/*
 * CORE_SURFACE_SET_PALETTE
 */
typedef struct {
     u32                    object_id;
} CoreSurfaceSetPalette;



DFBResult CoreSurface_SetConfig   ( CoreSurface             *surface,
                                    const CoreSurfaceConfig *config,
                                    CoreSurfaceConfigFlags   flags );

DFBResult CoreSurface_LockBuffer  ( CoreSurface             *surface,
                                    CoreSurfaceBufferRole    role,
                                    CoreSurfaceAccessorID    accessor,
                                    CoreSurfaceAccessFlags   access,
                                    CoreSurfaceBufferLock   *ret_lock );

DFBResult CoreSurface_UnlockBuffer( CoreSurface             *surface,
                                    CoreSurfaceBufferLock   *lock );

DFBResult CoreSurface_Flip        ( CoreSurface             *surface,
                                    bool                     swap );

DFBResult CoreSurface_SetPalette  ( CoreSurface             *surface,
                                    CorePalette             *palette );


#endif

