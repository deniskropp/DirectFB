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

