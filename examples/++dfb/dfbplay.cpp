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


class DFBPlay {
public:
     DFBPlay() {
     }

     virtual ~DFBPlay() {
     }

public:
     bool Init( int argc, char *argv[] ) {
          int         n        = 0;
          bool        full     = false;
          const char *filename = NULL;

          while (++n < argc) {
               if (!strcmp( argv[n], "-f" )) {
                    full = true;
               }
               else if (filename) {
                    filename = NULL;
                    break;
               }
               else
                    filename = argv[n];
          }

          if (!filename) {
               fprintf( stderr, "\nUsage: %s [-f] <filename>\n", argv[0] );
               return false;
          }

          m_dfb = DirectFB::Create();

          if (full)
               m_dfb.SetCooperativeLevel( DFSCL_FULLSCREEN );


          DFBSurfaceDescription desc;

          m_video = m_dfb.CreateVideoProvider( filename );

          m_video.GetSurfaceDescription( &desc );

          if (desc.flags & DSDESC_CAPS) {
               desc.caps  = (DFBSurfaceCapabilities)( desc.caps | DSCAPS_PRIMARY | DSCAPS_FLIPPING );
          }
          else {
               desc.flags = (DFBSurfaceDescriptionFlags)( desc.flags | DSDESC_CAPS );
               desc.caps  = (DFBSurfaceCapabilities)( DSCAPS_PRIMARY | DSCAPS_FLIPPING );
          }

          try {
               m_surface = m_dfb.CreateSurface( desc );
          }
          catch (DFBException *ex) {
               desc.flags = (DFBSurfaceDescriptionFlags)( desc.flags & ~DSDESC_PIXELFORMAT );

               m_surface = m_dfb.CreateSurface( desc );
          }

          m_video.PlayTo( m_surface, NULL, FrameCallback, this );

          m_events = m_dfb.CreateInputEventBuffer( DICAPS_KEYS );

          return true;
     }

     void Run() {
          while (m_video.GetStatus() == DVSTATE_PLAY) {
               DFBInputEvent event;

               m_events.WaitForEventWithTimeout( 0, 100 );

               while (m_events.GetEvent( DFB_EVENT(&event) )) {
                    switch (event.type) {
                         case DIET_KEYPRESS:
                              switch (event.key_symbol) {
                                   case DIKS_SMALL_Q:
                                   case DIKS_ESCAPE:
                                        return;

                                   default:
                                        break;
                              }
                              break;

                         default:
                              break;
                    }
               }
          }
     }

private:
     static void FrameCallback( void *ctx ) {
          DFBPlay *thiz = (DFBPlay*) ctx;

          thiz->m_surface.Flip();
     }

private:
     IDirectFB              m_dfb;
     IDirectFBVideoProvider m_video;
     IDirectFBSurface       m_surface;
     IDirectFBEventBuffer   m_events;
};

/**********************************************************************************************************************/

int
main( int argc, char *argv[] )
{
     DFBPlay app;

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

