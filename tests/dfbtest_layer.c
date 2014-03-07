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

#include <unistd.h>

#include <direct/messages.h>

#include <directfb.h>

static int                   quit = 0;
static IDirectFBEventBuffer *keybuffer;

int
main( int argc, char *argv[] )
{
     DFBResult              ret;
     int                    i;
     int                    x, y;
     int                    dx, dy;
     int                    sw3, sh3;
     int                    opacity       = 255;
     int                    opacity_delta = -1;
     IDirectFB             *dfb;
     IDirectFBDisplayLayer *layer;
     IDirectFBSurface      *surface;
     DFBDisplayLayerConfig  config;
     DFBDimension           size;
     DFBInputEvent          evt;

     /* Initialize DirectFB. */
     ret = DirectFBInit( &argc, &argv );
     if (ret) {
          D_DERROR( ret, "DFBTest/Layer: DirectFBInit() failed!\n" );
          return ret;
     }


     /* Create super interface. */
     ret = DirectFBCreate( &dfb );
     if (ret) {
          D_DERROR( ret, "DFBTest/Layer: DirectFBCreate() failed!\n" );
          return ret;
     }

     dfb->GetDisplayLayer( dfb, DLID_PRIMARY, &layer );

     /* Create an input buffer for key events */
     dfb->CreateInputEventBuffer( dfb, DICAPS_KEYS,
                                  DFB_TRUE, &keybuffer);

     layer->SetCooperativeLevel( layer, DFSCL_EXCLUSIVE );

     layer->GetConfiguration( layer, &config );


     config.options    = DLOP_OPACITY | DLOP_SRC_COLORKEY;
     config.buffermode = DLBM_FRONTONLY;

     layer->SetConfiguration( layer, &config );
     layer->SetSrcColorKey( layer, 0x00, 0xff, 0x00 );



     ret = layer->GetSurface( layer, &surface );
     if (ret) {
          D_DERROR( ret, "DFBTest/Layer: GetSurface() failed!\n" );
          dfb->Release( dfb );
          return ret;
     }

     surface->GetSize( surface, &size.w, &size.h );

     sw3 = ((size.w / 3) + 1) & ~1;
     sh3 = ((size.h / 3) + 1) & ~1;


     surface->Clear( surface, 0, 0, 0, 0xff );

     for (i=0; i<10; i++) {
          surface->SetColor( surface, 0xff - i*16, 0xff - i*16, 0xff - i*16, 0xff );
          surface->DrawRectangle( surface, i, i, size.w - i*2, size.h - i*2 );
     }

     surface->FillRectangle( surface, 10, size.h/2, size.w - 20, 1 );
     surface->FillRectangle( surface, size.w/2, 10, 1, size.h - 20 );

     surface->SetColor( surface, 0xff, 0x00, 0x00, 0xff );
     surface->FillRectangle( surface, size.w/3, size.h/3, size.w/3, size.h/3 );

     surface->SetColor( surface, 0x00, 0xff, 0x00, 0xff );
     surface->FillRectangle( surface, size.w/3 + size.w/9, size.h/3 + size.h/9, size.w/9, size.h/9 );

     surface->Flip( surface, NULL, DSFLIP_NONE );

#ifdef BUILD_AUTOMATION
     sleep( 2 );
#else
     sleep( 12 );
#endif
     layer->SetSourceRectangle( layer, 0, 0, size.w - sw3, size.h - sh3 );

     layer->SetScreenPosition( layer, 100, 100 );
     layer->SetScreenRectangle( layer, 100, 100, size.w - sw3, size.h - sh3 );

     sleep( 2 );
#ifdef BUILD_AUTOMATION
     sleep( 20 );
     quit = 1;
#endif

     for (x=0, y=0, dx=1, dy=1; !quit ; x+=dx, y+=dy) {
          layer->SetOpacity( layer, opacity );

          if (opacity == 255)
               opacity_delta = -1;
          else if (opacity == 0)
               opacity_delta = 1;

          opacity += opacity_delta;


          layer->SetSourceRectangle( layer, x, y, size.w - sw3, size.h - sh3 );

          surface->Flip( surface, NULL, DSFLIP_UPDATE );

          if (dx > 0) {
               if (x == size.w/3) {
                    dx = -1;

                    usleep( 500000 );
               }
          }
          else if (x == 0) {
               dx = 1;

               usleep( 500000 );
          }

          if (dy > 0) {
               if (y == size.h/3) {
                    dy = -1;

                    usleep( 500000 );
               }
          }
          else if (y == 0) {
               dy = 1;

               usleep( 500000 );
          }

          usleep( 10000 );

          /* Process keybuffer */
          while (keybuffer->GetEvent( keybuffer, DFB_EVENT(&evt)) == DFB_OK)
          {
              if (evt.type == DIET_KEYPRESS) {
                  switch (DFB_LOWER_CASE(evt.key_symbol)) {
                      case DIKS_ESCAPE:
                      case DIKS_SMALL_Q:
                      case DIKS_BACK:
                      case DIKS_STOP:
                      case DIKS_EXIT:
                          /* Quit main loop & test thread */
                          quit = 1;
                          break;
                      default:
                          break;
                  }
              }
          }

     }

     surface->Release( surface );
     layer->Release( layer );

     /* Shutdown DirectFB. */
     dfb->Release( dfb );

     return ret;
}

