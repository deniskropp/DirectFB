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

#ifndef __DFB__CORE__SCREENS_H__
#define __DFB__CORE__SCREENS_H__

#include <directfb.h>

#include <core/coretypes.h>

typedef DFBEnumerationResult (*CoreScreenCallback) (CoreScreen *screen,
                                                    void       *ctx);

typedef struct {
   /** Driver Control **/

     /*
      * Return size of screen data (shared memory).
      */
     int       (*ScreenDataSize) ();

     /*
      * Called once by the master to initialize screen data and reset hardware.
      * Driver has to fill the screen description.
      */
     DFBResult (*InitScreen)     ( CoreScreen                  *screen,
                                   GraphicsDevice              *device,
                                   void                        *driver_data,
                                   void                        *screen_data,
                                   DFBScreenDescription        *description );

     /*
      * Called once by the master for each encoder.
      */
     DFBResult (*InitEncoder)    ( CoreScreen                  *screen,
                                   void                        *driver_data,
                                   void                        *screen_data,
                                   int                          encoder,
                                   DFBScreenEncoderDescription *description );

     /*
      * Called once by the master for each output.
      */
     DFBResult (*InitOutput)     ( CoreScreen                  *screen,
                                   void                        *driver_data,
                                   void                        *screen_data,
                                   int                          output,
                                   DFBScreenOutputDescription  *description );


   /** Power management **/

     /*
      * Switch between "on", "standby", "suspend" and "off".
      */
     DFBResult (*SetPowerMode)   ( CoreScreen             *screen,
                                   void                   *driver_data,
                                   void                   *screen_data,
                                   DFBScreenPowerMode      mode );


   /** Synchronization **/

     /*
      * Wait for the vertical retrace.
      */
     DFBResult (*WaitVSync)      ( CoreScreen             *screen,
                                   void                   *driver_data,
                                   void                   *screen_data );
} ScreenFuncs;


/*
 * Add a screen to a graphics device by pointing to a table containing
 * driver functions. The driver data will be passed to these functions.
 */
CoreScreen *dfb_screens_register( GraphicsDevice     *device,
                                  void               *driver_data,
                                  ScreenFuncs        *funcs );

/*
 * Replace the default implementation for the primary screen.
 */
CoreScreen *dfb_screens_register_primary( GraphicsDevice *device,
                                          void           *driver_data,
                                          ScreenFuncs    *funcs );

/*
 * Enumerate all registered screens by invoking the callback for each screen.
 */
void dfb_screens_enumerate( CoreScreenCallback  callback,
                            void               *ctx );

/*
 * Returns the screen with the specified ID.
 */
inline CoreScreen *dfb_screens_at( DFBScreenID screen_id );

#endif
