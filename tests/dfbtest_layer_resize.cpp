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

extern "C" {
#include <direct/messages.h>
}

#include <++dfb.h>


int
main( int argc, char *argv[] )
{
     DFBResult              ret;
     bool                   quit = false;
     int                    i;
     IDirectFB              dfb;
     IDirectFBDisplayLayer  layer;
     IDirectFBSurface       surface;
     DFBDisplayLayerConfig  config;
     DFBDimension           sizes[2]  = { {400, 400}, {600, 600} };
     size_t                 num_sizes = D_ARRAY_SIZE(sizes);
     DFBInputEvent          evt;
     IDirectFBEventBuffer   keybuffer;

     /* Initialize DirectFB. */
     DirectFB::Init( &argc, &argv );

     /* Create super interface. */
     dfb = DirectFB::Create();

     layer = dfb.GetDisplayLayer( DLID_PRIMARY );

     /* Create an input buffer for key events */
     keybuffer = dfb.CreateInputEventBuffer( DICAPS_KEYS, DFB_TRUE );

     layer.SetCooperativeLevel( DLSCL_EXCLUSIVE );

     layer.GetConfiguration( &config );

     config.width  = sizes[0].w;
     config.height = sizes[0].h;

     layer.SetConfiguration( config );

     surface = layer.GetSurface();

     while (true) {
          surface.Clear( 0, 0, 0, 0xff );

          surface.SetColor( 0x00, 0xff, 0x00, 0xff );
          surface.DrawRectangle( 0, 0, config.width, config.height );

          surface.Flip( NULL, DSFLIP_NONE );


          keybuffer.WaitForEventWithTimeout( 3, 0 );

          /* Process keybuffer */
          while (keybuffer.GetEvent( DFB_EVENT(&evt) )) {
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

                      case DIKS_SPACE:
                      case DIKS_OK:
                          i++;

                          config.width  = sizes[i % num_sizes].w;
                          config.height = sizes[i % num_sizes].h;

                          layer.SetConfiguration( config );
                          break;

                      default:
                          break;
                  }
              }
          }
     }

     return 0;
}

