/*
   (c) Copyright 2008  Denis Oliver Kropp

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

#include <string.h>

#include <direct/messages.h>
#include <direct/thread.h>

#include <directfb.h>


static void *
TestThread( DirectThread *thread,
            void         *arg )
{
     DFBResult         ret;
     IDirectFBSurface *surface = arg;
     DFBRectangle      rect    = { 0, 0, 500, 1 };

     while (true) {
          void *data;
          int   pitch;

          ret = surface->Lock( surface, DSLF_WRITE, &data, &pitch );
          if (ret) {
               D_DERROR( ret, "DFBTest/Resize: Lock() failed!\n" );
               return NULL;
          }

          memset( data, 0, pitch * 400 );

          surface->Unlock( surface );



          data = alloca( pitch );

          ret = surface->Read( surface, &rect, data, pitch );
          if (ret) {
               D_DERROR( ret, "DFBTest/Resize: Read() failed!\n" );
               return NULL;
          }

          ret = surface->Write( surface, &rect, data, pitch );
          if (ret) {
               D_DERROR( ret, "DFBTest/Resize: Write() failed!\n" );
               return NULL;
          }
     }

     return NULL;
}

static void
TestResize( IDirectFB *dfb )
{
     DFBResult              ret;
     DirectThread          *thread;
     DFBWindowDescription   desc;
     IDirectFBDisplayLayer *layer;
     IDirectFBWindow       *window;
     IDirectFBSurface      *surface;

     dfb->GetDisplayLayer( dfb, DLID_PRIMARY, &layer );

     desc.flags       = DWDESC_WIDTH | DWDESC_HEIGHT | DWDESC_PIXELFORMAT;
     desc.width       = 500;
     desc.height      = 500;
     desc.pixelformat = DSPF_ARGB;

     ret = layer->CreateWindow( layer, &desc, &window );
     if (ret) {
          D_DERROR( ret, "DFBTest/Resize: CreateWindow() failed!\n" );
          return;
     }

     ret = window->GetSurface( window, &surface );
     if (ret) {
          D_DERROR( ret, "DFBTest/Resize: GetSurface() failed!\n" );
          return;
     }

     thread = direct_thread_create( DTT_DEFAULT, TestThread, surface, "Test" );

     while (true) {
          ret = window->Resize( window, 500, 400 );
          if (ret)
               D_DERROR( ret, "DFBTest/Resize: Resize() failed!\n" );

          ret = window->Resize( window, 500, 500 );
          if (ret)
               D_DERROR( ret, "DFBTest/Resize: Resize() failed!\n" );
     }

     surface->Release( surface );
     window->Release( window );
}


int
main( int argc, char *argv[] )
{
     DFBResult  ret;
     IDirectFB *dfb;

     /* Initialize DirectFB. */
     ret = DirectFBInit( &argc, &argv );
     if (ret) {
          D_DERROR( ret, "DFBTest/Resize: DirectFBInit() failed!\n" );
          return ret;
     }


     /* Create super interface. */
     ret = DirectFBCreate( &dfb );
     if (ret) {
          D_DERROR( ret, "DFBTest/Resize: DirectFBCreate() failed!\n" );
          return ret;
     }



     TestResize( dfb );



     /* Shutdown DirectFB. */
     dfb->Release( dfb );


     return ret;
}

