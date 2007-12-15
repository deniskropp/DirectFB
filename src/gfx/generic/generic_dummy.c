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

#include <config.h>

#include <stdio.h>

#include <directfb.h>

#include <core/gfxcard.h>

#include <direct/messages.h>


#include "yuvtbl.h"


void
gGetDriverInfo( GraphicsDriverInfo *info )
{
     D_INFO( "DirectFB/Genefx: No software fallbacks supported\n" );

     snprintf( info->name, DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH, "Software Driver" );
     snprintf( info->vendor, DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH, "directfb.org" );

     info->version.major = 0;
     info->version.minor = 0;
}

void
gGetDeviceInfo( GraphicsDeviceInfo *info )
{
     snprintf( info->name, DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "Software Rasterizer" );
     snprintf( info->vendor, DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "Dummy" );

     info->caps.accel    = DFXL_NONE;
     info->caps.flags    = 0;
     info->caps.drawing  = DSDRAW_NOFX;
     info->caps.blitting = DSBLIT_NOFX;
}

bool
gAcquire( CardState *state, DFBAccelerationMask accel )
{
     return false;
}

void
gRelease( CardState *state )
{
}

void
gFillRectangle( CardState *state, DFBRectangle *rect )
{
}

void
gDrawLine( CardState *state, DFBRegion *line )
{
}

void
gBlit( CardState *state, DFBRectangle *rect, int dx, int dy )
{
}

void
gStretchBlit( CardState *state, DFBRectangle *srect, DFBRectangle *drect )
{
}

