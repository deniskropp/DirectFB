/*
   TI Davinci driver

   (c) Copyright 2007  Telio AG

   Written by Denis Oliver Kropp <dok@directfb.org>

   Code is derived from VMWare driver.

   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

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

//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <asm/types.h>

#include <stdio.h>
#include <sys/mman.h>

#include <directfb.h>

#include <core/screens.h>

#include <direct/debug.h>
#include <direct/messages.h>

#include <sys/ioctl.h>

#include "davincifb.h"

#include "davinci_gfxdriver.h"
#include "davinci_screen.h"


D_DEBUG_DOMAIN( Davinci_Screen, "Davinci/Screen", "TI Davinci Screen" );

/**********************************************************************************************************************/

static DFBResult
davinciInitScreen( CoreScreen           *screen,
                   CoreGraphicsDevice   *device,
                   void                 *driver_data,
                   void                 *screen_data,
                   DFBScreenDescription *description )
{
     D_DEBUG_AT( Davinci_Screen, "%s()\n", __FUNCTION__ );

     /* Set the screen capabilities. */
     description->caps = DSCCAPS_VSYNC;

     /* Set the screen name. */
     snprintf( description->name, DFB_SCREEN_DESC_NAME_LENGTH, "TI Davinci Screen" );

     return DFB_OK;
}

static DFBResult
davinciGetScreenSize( CoreScreen *screen,
                      void       *driver_data,
                      void       *screen_data,
                      int        *ret_width,
                      int        *ret_height )
{
     D_DEBUG_AT( Davinci_Screen, "%s()\n", __FUNCTION__ );

     D_ASSERT( ret_width != NULL );
     D_ASSERT( ret_height != NULL );

     /* FIXME */
     *ret_width  = 720;
     *ret_height = 576;

     return DFB_OK;
}

static DFBResult
davinciWaitVSync( CoreScreen *screen,
                  void       *driver_data,
                  void       *screen_data )
{
     DavinciDriverData *ddrv = driver_data;

     D_DEBUG_AT( Davinci_Screen, "%s()\n", __FUNCTION__ );

     ioctl( ddrv->fb[OSD0].fd, FBIO_WAITFORVSYNC );

     return DFB_OK;
}

ScreenFuncs davinciScreenFuncs = {
     InitScreen:    davinciInitScreen,
     GetScreenSize: davinciGetScreenSize,
     WaitVSync:     davinciWaitVSync
};

