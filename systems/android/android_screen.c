/*
   (c) Copyright 2001-2010  The world wide DirectFB Open Source Community (directfb.org)
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

#include <core/layers.h>
#include <core/screens.h>

#include <misc/conf.h>


#include "android_system.h"


typedef struct {
} AndroidScreenData;

/**********************************************************************************************************************/

static int
androidScreenDataSize( void )
{
     return sizeof(AndroidScreenData);
}

static DFBResult
androidInitScreen( CoreScreen           *screen,
                CoreGraphicsDevice   *device,
                void                 *driver_data,
                void                 *screen_data,
                DFBScreenDescription *description )
{
     AndroidData       *android = driver_data;
     AndroidScreenData *data = screen_data;

     description->caps = DSCCAPS_NONE;

     direct_snputs( description->name, "Android", DFB_SCREEN_DESC_NAME_LENGTH );


     return DFB_OK;
}

static DFBResult
androidGetScreenSize( CoreScreen *screen,
                   void       *driver_data,
                   void       *screen_data,
                   int        *ret_width,
                   int        *ret_height )
{
     AndroidData       *android = driver_data;
     AndroidScreenData *data = screen_data;

     *ret_width  = android->shared->screen_size.w;
     *ret_height = android->shared->screen_size.h;

     return DFB_OK;
}

static const ScreenFuncs _androidScreenFuncs = {
     .ScreenDataSize = androidScreenDataSize,
     .InitScreen     = androidInitScreen,
     .GetScreenSize  = androidGetScreenSize
};

const ScreenFuncs *androidScreenFuncs = &_androidScreenFuncs;

