/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

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

#include <directfb.h>

#include <string.h>   /* for `memset()'  */
#include <time.h>     /* for `clock()'   */
#include <stdio.h>    /* for `fprintf()' */
#include <stdlib.h>   /* for `rand()'    */
#include <unistd.h>   /* for `sleep()'   */
#include <math.h>     /* for `sqrt()'    */

/* the super interface */
IDirectFB *dfb;

/* the primary surface (surface of primary layer) */
IDirectFBSurface *primary;

/* our "Press any key..." screen */
IDirectFBSurface *intro;

IDirectFBImageProvider *provider;

/* Input interfaces: device and its buffer */
IDirectFBInputDevice *keyboard;
IDirectFBInputBuffer *key_events;

/* macro for a safe call to DirectFB functions */
#define DFBCHECK(x...)                                                     \
               err = x;                                                    \
               if (err != DFB_OK) {                                        \
                    fprintf( stderr, "%s <%d>:\n\t", __FILE__, __LINE__ ); \
                    DirectFBErrorFatal( #x, err );                         \
               }

static int screen_width, screen_height;


void shutdown()
{
     /* release our interfaces to shutdown DirectFB */
     primary->Release( primary );
     key_events->Release( key_events );
     keyboard->Release( keyboard );
     dfb->Release( dfb );
}


#define SURFACEMANAGER_TEST_SURFACES 200

void surfacemanager_test()
{
     int i;
     int width, height;
     unsigned long t;
     IDirectFBSurface      *surfaces[SURFACEMANAGER_TEST_SURFACES];
     IDirectFBSurface      *surface;
     DFBResult             ret;
     DFBSurfaceDescription dsc;

     provider->GetSurfaceDescription (provider, &dsc);

     dsc.flags = DSDESC_WIDTH | DSDESC_HEIGHT;
     
     for (i=0; i<SURFACEMANAGER_TEST_SURFACES; i++) {
          dsc.width = rand()%500 + 100;
          dsc.height = rand()%500 + 100;

          ret = dfb->CreateSurface( dfb, &dsc, &surfaces[i] );
          if (ret) {
               int j;

               DirectFBError( "surfacemanager_test: "
                              "unable to create surface", ret );

               for (j=0; j<i; j++)
                    surfaces[j]->Release( surfaces[j] );

               return;
          }

          provider->RenderTo( provider, surfaces[i] );
     }

     
     t = clock();
     for (i=0; i<SURFACEMANAGER_TEST_SURFACES*100; i++) {
          surface = surfaces[rand()%SURFACEMANAGER_TEST_SURFACES];
          surface->GetSize (surface, &width, &height);
          primary->Blit( primary, 
                         surface, NULL, 
                         (screen_width - width) / 2, 
                         (screen_height - height) / 2 );
     }
     t = clock() - t;

     printf( "surfacemanager_test: clock diff %d\n", (int)t );

     for (i=0; i<SURFACEMANAGER_TEST_SURFACES; i++)
          surfaces[i]->Release( surfaces[i] );
}

int main( int argc, char *argv[] )
{
     DFBResult err;

     DFBCHECK(DirectFBInit( &argc, &argv ));

     /* create the super interface */
     DFBCHECK(DirectFBCreate( &dfb ));

     /* get an interface to the primary keyboard
        and create an input buffer for it */
     DFBCHECK(dfb->GetInputDevice( dfb, DIDID_KEYBOARD, &keyboard ));
     DFBCHECK(keyboard->CreateInputBuffer( keyboard, &key_events ));

     /* set our cooperative level to DFSCL_FULLSCREEN
        for exclusive access to the primary layer */
     DFBCHECK(dfb->SetCooperativeLevel( dfb, DFSCL_FULLSCREEN ));

     /* get the primary surface, i.e. the surface of the primary
        layer we have exclusive access to */
     {
          DFBSurfaceDescription dsc;

          memset( &dsc, 0, sizeof(DFBSurfaceDescription) );

          dsc.flags = DSDESC_CAPS;
          dsc.caps = DSCAPS_PRIMARY;

          DFBCHECK(dfb->CreateSurface( dfb, &dsc, &primary ));
          primary->GetSize( primary, &screen_width, &screen_height );
     }

     DFBCHECK(dfb->CreateImageProvider( dfb, DATADIR"/examples/melted.png",
                                        &provider ));
     
     surfacemanager_test();
//     surfacemanager_test();

     shutdown();

     return 0;
}

