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

#ifndef __CORE__CORE_SURFACE_INTERNAL_H__
#define __CORE__CORE_SURFACE_INTERNAL_H__


#include <fusion/call.h>

#include <core/CoreSurface.h>


/**********************************************************************************************************************
 * CoreSurface internal
 */

DirectResult dfb_surface_call( CoreSurface          *surface,
                               CoreSurfaceCall       call,
                               void                 *arg,
                               size_t                len,
                               FusionCallExecFlags   flags,
                               int                  *ret_val );

FusionCallHandlerResult CoreSurface_Dispatch( int           caller,   /* fusion id of the caller */
                                              int           call_arg, /* optional call parameter */
                                              void         *call_ptr, /* optional call parameter */
                                              void         *ctx,      /* optional handler context */
                                              unsigned int  serial,
                                              int          *ret_val );


#endif

