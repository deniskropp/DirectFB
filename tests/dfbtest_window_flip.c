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

#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include <direct/messages.h>
#include <direct/thread.h>

#include <directfb.h>
#include <directfb_util.h>


typedef struct {
     int       magic;

     int       frames;
     float     fps;
     long long fps_time;
     char      fps_string[20];
} FPSData;

static inline void
fps_init( FPSData *data )
{
     D_ASSERT( data != NULL );

     memset( data, 0, sizeof(FPSData) );

     data->fps_time = direct_clock_get_millis();

     D_MAGIC_SET( data, FPSData );
}

static inline void
fps_count( FPSData *data,
           int      interval )
{
     long long diff;
     long long now = direct_clock_get_millis();

     D_MAGIC_ASSERT( data, FPSData );

     data->frames++;

     diff = now - data->fps_time;
     if (diff >= interval) {
          data->fps = data->frames * 1000 / (float) diff;

          snprintf( data->fps_string, sizeof(data->fps_string), "%.1f", data->fps );

          data->fps_time = now;
          data->frames   = 0;
     }
}


static int
show_usage( const char *prg )
{
     fprintf( stderr, "Usage: %s <url>\n", prg );

     return -1;
}


static IDirectFBFont *font;


typedef struct {
     IDirectFBWindow        *window;
     IDirectFBSurface       *surface;
     int                     index;
     FPSData                 fps;
     DFBDimension            resolution;

     int                     anim_dirx;
     int                     anim_diry;
     int                     anim_x;
     int                     anim_y;
} App;

static DFBResult
app_init( App                   *app,
          IDirectFBDisplayLayer *layer,
          int                    x,
          int                    y,
          int                    width,
          int                    height,
          int                    index )
{
     DFBResult             ret;
     DFBWindowDescription  desc;
     IDirectFBWindow      *window;
     IDirectFBSurface     *surface;

     desc.flags  = DWDESC_WIDTH | DWDESC_HEIGHT | DWDESC_POSX | DWDESC_POSY | DWDESC_CAPS;
     desc.width  = width;
     desc.height = height;
     desc.posx   = x;
     desc.posy   = y;
     desc.caps   = DWCAPS_NONE; //| DWCAPS_ALPHACHANNEL | DWCAPS_DOUBLEBUFFER;

     /* Create a surface for the image. */
     ret = layer->CreateWindow( layer, &desc, &window );
     if (ret) {
          D_DERROR( ret, "DFBTest/WindowFlip: IDirectFBDisplayLayer::CreateWindow() failed!\n" );
          return ret;
     }

     /* Get the surface. */
     ret = window->GetSurface( window, &surface );
     if (ret) {
          D_DERROR( ret, "DFBTest/WindowFlip: IDirectFBWindow::GetSurface() failed!\n" );
          return ret;
     }

     surface->Clear( surface, 0, 0, 0, 0 );

     window->SetOpacity( window, 0xff );

     surface->Flip( surface, NULL, DSFLIP_NONE );


     app->window       = window;
     app->surface      = surface;
     app->index        = index;
     app->resolution.w = width;
     app->resolution.h = height;

     app->anim_dirx    = 5;
     app->anim_diry    = 5;
     app->anim_x       = 0;
     app->anim_y       = 0;

     fps_init( &app->fps );

     return DFB_OK;
}

static void
app_update( App *app )
{
     static const DFBColor colors[3] = {
          { 0xff, 0x30, 0xc0, 0xff },
          { 0xff, 0xff, 0xff, 0x30 },
          { 0xff, 0x30, 0xff, 0xc0 }
     };

     static const DFBPoint offsets[3] = {
          { 10, 10 },
          { 30, 20 },
          { 50, 30 }
     };

     IDirectFBSurface *surface = app->surface;

     surface->Clear( surface, 0xff, 0xff, 0xff, 0x20 );

     surface->SetColor( surface, colors[app->index].r, colors[app->index].g, colors[app->index].b, colors[app->index].a );
     surface->FillRectangle( surface, app->anim_x, app->anim_y, 40, 300 );

     surface->SetFont( surface, font );
     surface->SetColor( surface, colors[app->index].r/2, colors[app->index].g/2, colors[app->index].b/2, colors[app->index].a );
     surface->DrawString( surface, app->fps.fps_string, -1,
                          offsets[app->index].x + 30,
                          offsets[app->index].y + 30, DSTF_TOPLEFT );

     surface->Flip( surface, NULL, DSFLIP_WAITFORSYNC );


     app->anim_x += app->anim_dirx;
     if (app->anim_x >= app->resolution.w - 40)
          app->anim_dirx = -5;
     else if (app->anim_x <= 0)
          app->anim_dirx = 5;

     app->anim_y += app->anim_diry;
     if (app->anim_y >= app->resolution.h - 300)
          app->anim_diry = -5;
     else if (app->anim_y <= 0)
          app->anim_diry = 5;
          
          
     fps_count( &app->fps, 1000 );
}

void *
app_thread( DirectThread *thread,
            void         *arg );

void *
app_thread( DirectThread *thread,
            void         *arg )
{
     App *app = arg;

     while (true) {
          app_update( app );

     //     usleep( 10000 );
     }
}

int
main( int argc, char *argv[] )
{
     int                    i;
     DFBResult              ret;
     IDirectFB             *dfb;
     IDirectFBScreen       *screen = NULL;
     IDirectFBDisplayLayer *layer  = NULL;
     App                    apps[3];
     DFBDisplayLayerConfig  config;

     /* Parse arguments. */
     for (i=1; i<argc; i++) {
          if (!strcmp( argv[i], "-h" ))
               return show_usage( argv[0] );
          //else
          //     return show_usage( argv[0] );
     }

     /* Initialize DirectFB. */
     ret = DirectFBInit( &argc, &argv );
     if (ret) {
          D_DERROR( ret, "DFBTest/WindowFlip: DirectFBInit() failed!\n" );
          return ret;
     }

     /* Create super interface. */
     ret = DirectFBCreate( &dfb );
     if (ret) {
          D_DERROR( ret, "DFBTest/WindowFlip: DirectFBCreate() failed!\n" );
          return ret;
     }

     /* Load the font. */
     DFBFontDescription desc = { .flags = DFDESC_HEIGHT, .height = 36 };
     ret = dfb->CreateFont( dfb, DATADIR "/decker.dgiff", &desc, &font );
     if (ret) {
          D_DERROR( ret, "DFBTest/WindowFlip: IDirectFB::CreateFont( " DATADIR "/decker.dgiff ) failed!\n" );
          goto out;
     }

     /* Get primary screen. */
     ret = dfb->GetScreen( dfb, DSCID_PRIMARY, &screen );
     if (ret) {
          D_DERROR( ret, "DFBTest/WindowFlip: IDirectFB::GetScreen( PRIMARY ) failed!\n" );
          goto out;
     }

     /* Get primary layer. */
     ret = dfb->GetDisplayLayer( dfb, DLID_PRIMARY, &layer );
     if (ret) {
          D_DERROR( ret, "DFBTest/WindowFlip: IDirectFB::GetDisplayLayer( PRIMARY ) failed!\n" );
          goto out;
     }

     
     layer->GetConfiguration( layer, &config );

     app_init( &apps[0], layer, 50, 50, config.width-400, config.height-100, 0 );
//     app_init( &apps[1], layer, 0, 0, 300, 700, 1 );
//     app_init( &apps[2], layer, width, height, 2 );

//     while (1) {
//          app_update( &apps[0] );
//          app_update( &apps[1] );
//          app_update( &apps[2] );
//     }
     direct_thread_create( DTT_DEFAULT, app_thread, &apps[0], "App 0" );
//     direct_thread_create( DTT_DEFAULT, app_thread, &apps[1], "App 1" );
//     direct_thread_create( DTT_DEFAULT, app_thread, &apps[2], "App 2" );

     while (true) {
          sleep( 10 );

//          IDirectFBSurface *surface;

//          layer->GetSurface( layer, &surface );

//          surface->Dump( surface, "/", "dfbtest_tree" );
     }


out:
     /* Shutdown DirectFB. */
     dfb->Release( dfb );

     return ret;
}

