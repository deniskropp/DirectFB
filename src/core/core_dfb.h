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

#ifndef __CORE_DFB_H__
#define __CORE_DFB_H__

#include <fusion/types.h>
#include <fusion/lock.h>
#include <fusion/object.h>

#include <directfb.h>

#include <core/surface.h>


/**********************************************************************************************************************
 * CoreDFB
 */

/*
 * CoreDFB Calls
 */

typedef enum {
     CORE_DFB_CREATE_SURFACE  = 1,
} CoreDFBCall;

/*
 * CORE_DFB_CREATE_SURFACE
 */
typedef struct {
     CoreSurfaceConfig        config;
     CoreSurfaceTypeFlags     type;
     unsigned long            resource_id;
} CoreDFBCreateSurface;



DFBResult CoreDFB_CreateSurface( CoreDFB                  *core,
                                 const CoreSurfaceConfig  *config,
                                 CoreSurfaceTypeFlags      type,
                                 unsigned long             resource_id,
                                 CorePalette              *palette,
                                 CoreSurface             **ret_surface );

#endif

