/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2003  Convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <sven@directfb.org> and
              Ville Syrjala <syrjala@sci.fi>

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

#include <iostream>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include <++dfb.h>

class Switcher {
public:
     Switcher() {
     }

     virtual ~Switcher() {
     }

     bool Init( int argc, char *argv[] ) {
          /* Parse the command line. */
          if (argc != 2 || !argv[1] || !argv[1][0] ||
              sscanf( argv[1], "%dx%d", &m_width, &m_height ) < 2) {
               std::cerr << std::endl;
               std::cerr << "Usage: " << argv[0] << " <width>x<height>" << std::endl;
               std::cerr << std::endl;
               return false;
          }

          /* Create the main interface. */
          m_dfb = DirectFB::Create();

          /* Get an interface to the primary layer. */
          m_layer = m_dfb.GetDisplayLayer( DLID_PRIMARY );

          return true;
     }

     void Run() {
          DFBDisplayLayerConfig config;

          /* This level allows window stack mode switches. */
          m_layer.SetCooperativeLevel( DLSCL_ADMINISTRATIVE );

          /* Get the current configuration. */
          m_layer.GetConfiguration( &config );

          /* Change width and height. */
          config.width  = m_width;
          config.height = m_height;

          /* Set the new configuration. */
          m_layer.SetConfiguration( config );
     }

private:
     IDirectFB             m_dfb;
     IDirectFBDisplayLayer m_layer;
     int                   m_width;
     int                   m_height;
};

int
main(int argc, char *argv[])
{
     Switcher app;

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
