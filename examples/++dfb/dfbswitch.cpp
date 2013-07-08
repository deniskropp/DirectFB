/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

   This file is subject to the terms and conditions of the MIT License:

   Permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation
   files (the "Software"), to deal in the Software without restriction,
   including without limitation the rights to use, copy, modify, merge,
   publish, distribute, sublicense, and/or sell copies of the Software,
   and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
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

     static DFBEnumerationResult VideoModeCallback( int   width,
                                                    int   height,
                                                    int   bpp,
                                                    void *callbackdata )
     {
          printf( "  - %dx%d\n", width, height );
     }

     bool Init( int argc, char *argv[] ) {
          bool list = false;

          /* Parse the command line. */
          if (argc != 2 || !argv[1] || !argv[1][0] ||
              (sscanf( argv[1], "%dx%d", &m_width, &m_height ) < 2 &&
               !(list = !strcmp( argv[1], "-l" )) ))
          {
               std::cerr << std::endl;
               std::cerr << "Usage: " << argv[0] << " <width>x<height>" << std::endl;
               std::cerr << std::endl;
               return false;
          }

          /* Create the main interface. */
          m_dfb = DirectFB::Create();

          if (list) {
               printf( "\nVideo Modes\n" );
               m_dfb.EnumVideoModes( VideoModeCallback, NULL );
               return false;
          }

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
