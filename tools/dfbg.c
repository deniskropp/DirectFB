/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <neo@directfb.org>.
              
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

#include <directfb.h>

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#ifndef bool
typedef enum {
     false = 0,
     true = !false
} bool;
#endif

/******************************************************************************/

static IDirectFB             *dfb   = NULL;
static IDirectFBDisplayLayer *layer = NULL;

static char *filename = NULL;

/******************************************************************************/

static bool parse_command_line( int argc, char *argv[] );
static void load_background();

/******************************************************************************/

int
main( int argc, char *argv[] )
{
     DFBResult ret;

     /* Initialize DirectFB including command line parsing. */
     ret = DirectFBInit( &argc, &argv );
     if (ret) {
          DirectFBError( "DirectFBInit() failed", ret );
          return -1;
     }

     /* Parse the command line. */
     if (!parse_command_line( argc, argv ))
          return -2;

     /* Create the super interface. */
     ret = DirectFBCreate( &dfb );
     if (ret) {
          DirectFBError( "DirectFBCreate() failed", ret );
          return -3;
     }

     /* Get the primary display layer. */
     ret = dfb->GetDisplayLayer( dfb, DLID_PRIMARY, &layer );
     if (ret) {
          DirectFBError( "IDirectFB::GetDisplayLayer() failed", ret );
          dfb->Release( dfb );
          return -4;
     }

     /* Load and display the background image. */
     load_background();

     /* Release the display layer. */
     layer->Release( layer );

     /* Release the super interface. */
     dfb->Release( dfb );
     
     return 0;
}

/******************************************************************************/

static const char *usage_string = 
"\n"
"DirectFB Background Configuration Tool\n"
"\n"
"Usage: dfbg <filename>\n"
"\n";

static bool
parse_command_line( int argc, char *argv[] )
{
     if (argc != 2) {
          printf( usage_string );
          return false;
     }

     filename = argv[1];

     return true;
}

static void
load_background()
{
     DFBResult               ret;
     DFBDisplayLayerConfig   config;
     DFBSurfaceDescription   desc;
     IDirectFBSurface       *surface;
     IDirectFBImageProvider *provider;

     ret = layer->GetConfiguration( layer, &config );
     if (ret) {
          DirectFBError( "IDirectFBDisplayLayer::GetConfiguration() failed", ret );
          return;
     }
     
     ret = dfb->CreateImageProvider( dfb, filename, &provider );
     if (ret) {
          DirectFBError( "IDirectFB::CreateImageProvider() failed", ret );
          return;
     }

     ret = provider->GetSurfaceDescription( provider, &desc );
     if (ret) {
          DirectFBError( "IDirectFBImageProvider::GetSurfaceDescription() failed", ret );
          provider->Release( provider );
          return;
     }

     desc.width  = config.width;
     desc.height = config.height;

     ret = dfb->CreateSurface( dfb, &desc, &surface );
     if (ret) {
          DirectFBError( "IDirectFB::CreateSurface() failed", ret );
          provider->Release( provider );
          return;
     }

     ret = provider->RenderTo( provider, surface, NULL );
     if (ret) {
          DirectFBError( "IDirectFBImageProvider::RenderTo() failed", ret );
          surface->Release( surface );
          provider->Release( provider );
          return;
     }

     ret = layer->SetBackgroundImage( layer, surface );
     if (ret) {
          DirectFBError( "IDirectFBDisplayLayer::SetBackgroundImage() failed", ret );
          surface->Release( surface );
          provider->Release( provider );
          return;
     }

     ret = layer->SetBackgroundMode( layer, DLBM_IMAGE );
     if (ret)
          DirectFBError( "IDirectFBDisplayLayer::SetBackgroundMode() failed", ret );

     surface->Release( surface );
     provider->Release( provider );
}

