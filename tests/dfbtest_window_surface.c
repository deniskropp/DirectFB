/*
   (c) Copyright 2011  Denis Oliver Kropp

   All rights reserved.

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

#include <config.h>

#include <direct/messages.h>
#include <direct/thread.h>

#include <directfb.h>


int
main( int argc, char *argv[] )
{
     DFBResult              ret;
     IDirectFB             *dfb;
     IDirectFBDisplayLayer *layer;

     /* Initialize DirectFB. */
     ret = DirectFBInit( &argc, &argv );
     if (ret) {
          D_DERROR( ret, "DFBTest/Window_Surface: DirectFBInit() failed!\n" );
          return ret;
     }


     /* Create super interface. */
     ret = DirectFBCreate( &dfb );
     if (ret) {
          D_DERROR( ret, "DFBTest/Window_Surface: DirectFBCreate() failed!\n" );
          return ret;
     }

     dfb->GetDisplayLayer( dfb, DLID_PRIMARY, &layer );


     while (true) {
          DFBWindowDescription   desc;
          IDirectFBWindow       *window;
          IDirectFBSurface      *surface;

          desc.flags  = DWDESC_POSX | DWDESC_POSY | DWDESC_WIDTH | DWDESC_HEIGHT | DWDESC_CAPS;
          desc.posx   = 150;
          desc.posy   = 150;
          desc.width  = 300;
          desc.height = 300;
          desc.caps   = DWCAPS_ALPHACHANNEL;
     
          ret = layer->CreateWindow( layer, &desc, &window );
          if (ret) {
               D_DERROR( ret, "DFBTest/Window_Surface: CreateWindow() failed!\n" );
               return ret;
          }
     
          window->GetSurface( window, &surface );

          D_INFO( "Created window and surface, going to release window... (in 2 seconds)\n" );
          usleep( 2000000 );

          D_INFO("Releasing window...\n");

          window->Release( window );

          D_INFO("Window released, going to release surface... (in 2 seconds)\n");
          usleep( 2000000 );

          D_INFO("Releasing surface...\n");

          surface->Release( surface );

          D_INFO("Surface released, done.\n");
          usleep( 5000000 );
     }


     /* Shutdown DirectFB. */
     dfb->Release( dfb );

     return ret;
}

