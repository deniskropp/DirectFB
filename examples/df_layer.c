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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include <directfb.h>

IDirectFB *dfb;

DFBEnumerationResult enum_layers_callback( unsigned int                 id,
                                           DFBDisplayLayerCapabilities  caps,
                                           void                        *data )
{
     IDirectFBDisplayLayer **layer = (IDirectFBDisplayLayer **)data;

     printf( "\nLayer %d:\n", id );

     if (caps & DLCAPS_SURFACE)
          printf( "  - Has a surface.\n" );

     if (caps & DLCAPS_ALPHACHANNEL)
          printf( "  - Supports blending based on alpha channel.\n" );

     if (caps & DLCAPS_COLORKEYING)
          printf( "  - Supports color keying.\n" );

     if (caps & DLCAPS_FLICKER_FILTERING)
          printf( "  - Supports flicker filtering.\n" );

     if (caps & DLCAPS_INTERLACED_VIDEO)
          printf( "  - Can natively display interlaced video.\n" );

     if (caps & DLCAPS_OPACITY)
          printf( "  - Supports blending based on global alpha factor.\n" );

     if (caps & DLCAPS_SCREEN_LOCATION)
          printf( "  - Can be positioned on the screen.\n" );

     /* We take the first layer not being the primary */
     if (id != DLID_PRIMARY) {
          DFBResult ret;

          ret = dfb->GetDisplayLayer( dfb, id, layer );
          if (ret)
               DirectFBError( "dfb->GetDisplayLayer failed", ret );
          else
               return DFENUM_CANCEL;
     }

     return DFENUM_OK;
}

int main( int argc, char *argv[] )
{
     DFBResult               ret;
     IDirectFBSurface       *videosurface;
     IDirectFBVideoProvider *videoprovider;
     IDirectFBDisplayLayer  *videolayer = NULL;
     DFBDisplayLayerConfig   dlc;
     DFBSurfaceDescription   dsc;

     IDirectFBInputBuffer   *events;

     /* Initialize DirectFB */
     ret = DirectFBInit( &argc, &argv );
     if (ret)
          DirectFBErrorFatal( "DirectFBInit failed", ret );

     if (argc < 2) {
          printf( "\nUsage: %s <video file or device>\n\n", argv[0] );
          return -1;
     }

     /* Create the super interface */
     ret = DirectFBCreate( &dfb );
     if (ret)
          DirectFBErrorFatal( "DirectFBCreate failed", ret );

     /* Enumerate display layers */
     ret = dfb->EnumDisplayLayers( dfb, enum_layers_callback, &videolayer );
     if (ret)
          DirectFBErrorFatal( "dfb->EnumDisplayLayers failed", ret );

     if (!videolayer) {
          printf( "\nNo additional layers have been found.\n" );
          dfb->Release( dfb );
          return -1;
     }

     /* Set the screen location of the video layer */
     videolayer->SetScreenLocation( videolayer, 0.0f, 0.0f, 0.2f, 0.2f );

     /* Get the surface of the video layer */
     ret = videolayer->GetSurface( videolayer, &videosurface );
     if (ret)
          DirectFBErrorFatal( "videolayer->GetSurface failed", ret );

     /* Create a videoprovider for the file or device */
     ret = dfb->CreateVideoProvider( dfb, argv[1], &videoprovider );
     if (ret)
          DirectFBErrorFatal( "dfb->CreateVideoProvider failed", ret );

     /* Set the layer's pixel resolution to that of the video */
     videoprovider->GetSurfaceDescription( videoprovider, &dsc );

     dlc.flags       = DLCONF_WIDTH | DLCONF_HEIGHT | DLCONF_PIXELFORMAT;
     dlc.width       = dsc.width;
     dlc.height      = dsc.height;
     dlc.pixelformat = DSPF_YUY2;

     ret = videolayer->SetConfiguration( videolayer, &dlc );
     if (ret)
          DirectFBErrorFatal( "videolayer->SetConfiguration failed", ret );

     /* Have the video decoded into the surface of the layer */
     ret = videoprovider->PlayTo( videoprovider, videosurface, NULL, NULL, NULL );
     if (ret)
          DirectFBErrorFatal( "videoprovider->PlayTo failed", ret );

     /* Create an input buffer for any device that has keys */
     ret = dfb->CreateInputBuffer( dfb, DICAPS_KEYS, &events );
     if (ret)
          DirectFBErrorFatal( "dfb->CreateInputBuffer failed", ret );

     while (1) {
          double t;
          float  w, h;
          struct timeval tv;

          /* Wait max. 20ms for an event, if one arrived then quit */
          if (events->WaitForEventWithTimeout( events, 0, 20000000 ) == DFB_OK)
               break;

          gettimeofday( &tv, NULL );

          t = tv.tv_sec + tv.tv_usec / 1000000.0;

          w = ( (float)sin(4*t) + 1.0f ) / 3.0f + 0.2f;
          h = ( (float)cos(3*t) + 1.0f ) / 3.0f + 0.2f;

          videolayer->SetScreenLocation( videolayer,
                                         0.5f - w/2.0f, 0.5f - h/2.0f, w, h );
     }

     /* Shutdown */
     events->Release( events );
     videoprovider->Release( videoprovider );
     videosurface->Release( videosurface );
     videolayer->Release( videolayer );
     dfb->Release( dfb );

     return 0;
}
