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

#include <config.h>

#include <unistd.h>

#include <direct/messages.h>
#include <direct/thread.h>

#include <directfb.h>


static void *
window_loop1( DirectThread *thread,
              void         *ctx )
{
     DFBResult              ret;
     int                    i;
     DFBWindowDescription   desc;
     IDirectFBDisplayLayer *layer = ctx;
     IDirectFBWindow       *window;
     IDirectFBSurface      *surface;

     D_INFO( "DFBTest/Window_FlipOnce: First thread starting...\n" );

     desc.flags  = DWDESC_POSX | DWDESC_POSY | DWDESC_WIDTH | DWDESC_HEIGHT | DWDESC_CAPS;
     desc.posx   = 10;
     desc.posy   = 10;
     desc.width  = 400;
     desc.height = 400;
     desc.caps   = DWCAPS_ALPHACHANNEL;

     ret = layer->CreateWindow( layer, &desc, &window );
     if (ret) {
          D_DERROR( ret, "DFBTest/Window_FlipOnce: CreateWindow() failed!\n" );
          return NULL;
     }

     window->GetSurface( window, &surface );

     surface->Clear( surface, 0, 0, 0, 0 );
     window->SetOpacity( window, 0xff );

     sleep( 2 );

     for (i=0; i<10; i++) {
          window->BeginUpdates( window, NULL );

          surface->Clear( surface, 0xff, 0xff, 0xff, 0xff );

          sleep( 1 );

          surface->Clear( surface, 0xc0, 0x10, 0x40, 0xc0 );

          surface->Flip( surface, NULL, DSFLIP_ONCE );
     }

     surface->Release( surface );
     window->Release( window );

     D_INFO( "DFBTest/Window_FlipOnce: First thread stopped.\n" );

     return NULL;
}

static void *
window_loop2( DirectThread *thread,
              void         *ctx )
{
     DFBResult              ret;
     int                    i;
     DFBWindowDescription   desc;
     IDirectFBDisplayLayer *layer = ctx;
     IDirectFBWindow       *window;
     IDirectFBSurface      *surface;

     D_INFO( "DFBTest/Window_FlipOnce: Second thread starting...\n" );

     desc.flags  = DWDESC_POSX | DWDESC_POSY | DWDESC_WIDTH | DWDESC_HEIGHT | DWDESC_CAPS;
     desc.posx   = 150;
     desc.posy   = 150;
     desc.width  = 300;
     desc.height = 300;
     desc.caps   = DWCAPS_ALPHACHANNEL;

     ret = layer->CreateWindow( layer, &desc, &window );
     if (ret) {
          D_DERROR( ret, "DFBTest/Window_FlipOnce: CreateWindow() failed!\n" );
          return NULL;
     }

     window->GetSurface( window, &surface );

     surface->Clear( surface, 0, 0, 0, 0 );
     window->SetOpacity( window, 0xff );

     sleep( 1 );

     for (i=0; i<10; i++) {
          window->BeginUpdates( window, NULL );

          surface->Clear( surface, 0xff, 0xff, 0xff, 0xff );

          sleep( 3 );

          surface->Clear( surface, 0x10, 0x40, 0xc0, 0xc0 );

          surface->Flip( surface, NULL, DSFLIP_ONCE );
     }

     surface->Release( surface );
     window->Release( window );

     D_INFO( "DFBTest/Window_FlipOnce: Second thread stopped.\n" );

     return NULL;
}


int
main( int argc, char *argv[] )
{
     DFBResult              ret;
     IDirectFB             *dfb;
     IDirectFBDisplayLayer *layer;
     DirectThread          *thread1;
     DirectThread          *thread2;

     /* Initialize DirectFB. */
     ret = DirectFBInit( &argc, &argv );
     if (ret) {
          D_DERROR( ret, "DFBTest/Window_FlipOnce: DirectFBInit() failed!\n" );
          return ret;
     }


     /* Create super interface. */
     ret = DirectFBCreate( &dfb );
     if (ret) {
          D_DERROR( ret, "DFBTest/Window_FlipOnce: DirectFBCreate() failed!\n" );
          return ret;
     }

     dfb->GetDisplayLayer( dfb, DLID_PRIMARY, &layer );

     thread1 = direct_thread_create( DTT_DEFAULT, window_loop1, layer, "Window 1" );
     thread2 = direct_thread_create( DTT_DEFAULT, window_loop2, layer, "Window 2" );

     direct_thread_join( thread1 );
     direct_thread_join( thread2 );

     direct_thread_destroy( thread1 );
     direct_thread_destroy( thread2 );

     /* Shutdown DirectFB. */
     dfb->Release( dfb );

     return ret;
}

