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

#include <config.h>

#include <directfb.h>

#include <core/coredefs.h>

#include <core/screen.h>
#include <core/screens_internal.h>


DFBResult
dfb_screen_get_info( CoreScreen           *screen,
                     DFBScreenID          *ret_id,
                     DFBScreenDescription *ret_desc )
{
     CoreScreenShared *shared;

     DFB_ASSERT( screen != NULL );
     DFB_ASSERT( screen->shared != NULL );

     shared = screen->shared;

     if (ret_id)
          *ret_id = shared->screen_id;

     if (ret_desc)
          *ret_desc = shared->description;

     return DFB_OK;
}

DFBResult
dfb_screen_suspend( CoreScreen *screen )
{
     DFB_ASSERT( screen != NULL );

     return DFB_OK;
}

DFBResult
dfb_screen_resume( CoreScreen *screen )
{
     DFB_ASSERT( screen != NULL );

     return DFB_OK;
}

DFBResult
dfb_screen_set_powermode( CoreScreen         *screen,
                          DFBScreenPowerMode  mode )
{
     ScreenFuncs *funcs;

     DFB_ASSERT( screen != NULL );
     DFB_ASSERT( screen->funcs != NULL );

     funcs = screen->funcs;

     if (funcs->SetPowerMode)
          return funcs->WaitVSync( screen,
                                   screen->driver_data, screen->screen_data );

     return DFB_UNSUPPORTED;
}

DFBResult
dfb_screen_wait_vsync( CoreScreen *screen )
{
     ScreenFuncs *funcs;

     DFB_ASSERT( screen != NULL );
     DFB_ASSERT( screen->funcs != NULL );

     funcs = screen->funcs;

     if (funcs->WaitVSync)
          return funcs->WaitVSync( screen,
                                   screen->driver_data, screen->screen_data );

     return DFB_UNSUPPORTED;
}

