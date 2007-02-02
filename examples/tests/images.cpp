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

extern "C" {
     #include <direct/clock.h>

     const char *dfb_pixelformat_name( DFBSurfacePixelFormat format );
}

class TestBed {
public:
     TestBed() {
     }

     virtual ~TestBed() {
     }

     bool Init( int argc, char *argv[] ) {
          m_dfb = DirectFB::Create();

          return true;
     }

     void Run() {
          RunTest( "data/test32x32.dfiff", 32, 32, DSPF_ARGB );
          RunTest( "data/test32x32x16.dfiff", 32, 32, DSPF_RGB16 );
          RunTest( "data/test32x32.png", 32, 32, DSPF_ARGB );
          RunTest( "data/test32x32.gif", 32, 32, DSPF_ARGB );
          RunTest( "data/test32x32.jpeg", 32, 32, DSPF_ARGB );
          RunTest( "data/test32x32.ppm", 32, 32, DSPF_ARGB );
          RunTest( "data/test32x32.bmp", 32, 32, DSPF_ARGB );
          RunTest( "data/test32x32.tif", 32, 32, DSPF_ARGB );
     }

private:
     void RunTest( const char *filename, int width, int height, DFBSurfacePixelFormat format ) {
          DFBSurfaceDescription desc;
          IDirectFBSurface      surface;

          desc.flags       = (DFBSurfaceDescriptionFlags)( DSDESC_WIDTH  |
                                                           DSDESC_HEIGHT |
                                                           DSDESC_PIXELFORMAT );
          desc.width       = width;
          desc.height      = height;
          desc.pixelformat = format;

          surface = m_dfb.CreateSurface( desc );

          long long t1 = direct_clock_get_millis();

          for (int i=0; i<10000; i++) {
               IDirectFBImageProvider provider = m_dfb.CreateImageProvider( filename );

               provider.RenderTo( surface, NULL );
               provider = 0;
          }

          long long t2 = direct_clock_get_millis();

          int diff = t2 - t1;

          printf( "Loaded %dx%d image '%s' as %s 10000x in %d.%03d seconds (%d images/sec).\n",
                  width, height, filename, dfb_pixelformat_name( format ),
                  diff / 1000, diff % 1000, 10000000 / diff );
     }

private:
     IDirectFB m_dfb;
};

int
main( int argc, char *argv[] )
{
     TestBed app;

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

