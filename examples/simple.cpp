/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de>,
              Andreas Hundt <andi@convergence.de> and
              Sven Neumann <sven@convergence.de>

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include <++dfb.h>

#include "dfbapp.h"

class Simple : public DFBApp {
public:
     Simple() {
          m_keydown = false;
     }

     virtual ~Simple() {
     }

private:
     /* render callback */
     virtual void Render( IDirectFBSurface &surface ) {
          if (m_keydown)
               surface.Clear( 0xff, 0xff, 0xff );
          else
               surface.Clear( 0x00, 0x00, 0x00 );
     }

     /* event callback, return true to exit application */
     virtual bool HandleEvent( DFBInputEvent &event ) {
          switch (event.type) {
               case DIET_KEYPRESS:
                    if (event.key_symbol == DIKS_ESCAPE)
                         return true;

                    m_keydown = true;
                    break;

               case DIET_KEYRELEASE:
                    m_keydown = false;
                    break;

               default:
                    break;
          }

          return false;
     }

     bool m_keydown;
};

int
main( int argc, char *argv[] )
{
     Simple app;

     try {
          /* Initialize DirectFB command line parsing. */
          DirectFB::Init( &argc, &argv );

          /* Parse remaining arguments and run. */
          if (app.Init( argc, argv ))
               app.Run();
     }
     catch (DFBException *ex) {
          /*
           * Exception has been caught, destructor of 'app' will deinitialize
           * anything at return time (below) that got initialized until now.
           */
          std::cerr << std::endl;
          std::cerr << "Caught exception!" << std::endl;
          std::cerr << "  -- " << ex << std::endl;
     }

     return 0;
}

