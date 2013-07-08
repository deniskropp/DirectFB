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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>

#include <stdlib.h>
#include <stdio.h>

#include <++dfb.h>

#include "dfbapp.h"
#include "dfbimage.h"

class DFBShow : public DFBApp {
public:
     DFBShow() {
     }

     virtual ~DFBShow() {
     }

private:
     /* called after initialization */
     virtual bool Setup( int width, int height ) {
          m_image.LoadImage( m_filename );

          return true;
     }

     /* render callback */
     virtual void Render( IDirectFBSurface &surface ) {
          int x = ((int) surface.GetWidth()  - (int) m_image.GetWidth())  / 2;
          int y = ((int) surface.GetHeight() - (int) m_image.GetHeight()) / 2;

          surface.Clear();

          m_image.PrepareTarget( surface );

          surface.Blit( m_image, NULL, x, y );
     }

     bool ParseArgs( int argc, char *argv[] ) {
          /* Parse the command line. */
          if (argc != 2 || !argv[1] || !argv[1][0]) {
               std::cerr << std::endl;
               std::cerr << "Usage: " << argv[0] << " <filename>" << std::endl;
               std::cerr << std::endl;
               return false;
          }

          /* Create the main interface. */
          m_filename = argv[1];

          return true;
     }

private:
     std::string m_filename;
     DFBImage    m_image;
};

int
main( int argc, char *argv[] )
{
     DFBShow app;

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

