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

