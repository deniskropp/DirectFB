/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <directfb.h>


/*****************************************************************************/

static IDirectFB             *dfb   = NULL;
static IDirectFBDisplayLayer *layer = NULL;

static const char *filename = NULL;
static DFBBoolean  color    = DFB_FALSE;
static DFBBoolean  tiled    = DFB_FALSE;


/*****************************************************************************/

static DFBBoolean parse_command_line   ( int argc, char *argv[] );
static void       set_background_color ( void );
static void       set_background_image ( void );

/*****************************************************************************/

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

     DirectFBSetOption( "bg-none", NULL );
     DirectFBSetOption( "no-cursor", NULL );

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

     /* Acquire administrative cooperative level. */
     ret = layer->SetCooperativeLevel( layer, DLSCL_ADMINISTRATIVE );
     if (ret) {
          DirectFBError( "IDirectFBDisplayLayer::SetCooperativeLevel() failed", ret );
          layer->Release( layer );
          dfb->Release( dfb );
          return -5;
     }

     /* Set the background according to the users wishes. */
     if (color)
          set_background_color();
     else
          set_background_image();

     /* Release the display layer. */
     layer->Release( layer );

     /* Release the super interface. */
     dfb->Release( dfb );
     
     return EXIT_SUCCESS;
}

/*****************************************************************************/

static void
print_usage (const char *prg_name)
{
     fprintf (stderr, "dfbg version %s\n", DIRECTFB_VERSION);
     fprintf (stderr, "DirectFB Background Configuration Tool\n\n");
     fprintf (stderr, "Usage: %s [options] <imagefile>\n", prg_name);
     fprintf (stderr, "   -c, --color     interpret the filename as a color (AARRGGBB)\n");
     fprintf (stderr, "   -t, --tile      tile background with the image\n");
     fprintf (stderr, "   -h, --help      show this help message\n");
     fprintf (stderr, "   -v, --version   print version information\n");
     fprintf (stderr, "\n");
}

static DFBBoolean
parse_command_line( int argc, char *argv[] )
{
     int n;

     for (n = 1; n < argc; n++) {
          const char *a = argv[n];

          if (*a != '-') {
               if (!filename) {
                    filename = a;
                    continue;
               }
               else {
                    print_usage (argv[0]);
                    return DFB_FALSE;
               }
          }
          if (strcmp (a, "-h") == 0 || strcmp (a, "--help") == 0) {
               print_usage (argv[0]);
               return DFB_FALSE;
          }
          if (strcmp (a, "-v") == 0 || strcmp (a, "--version") == 0) {
               fprintf (stderr, "dfbg version %s\n", DIRECTFB_VERSION);
               return DFB_FALSE;
          }
          if (strcmp (a, "-c") == 0 || strcmp (a, "--color") == 0) {
               color = DFB_TRUE;
               continue;
          }
          if (strcmp (a, "-t") == 0 || strcmp (a, "--tile") == 0) {
               tiled = DFB_TRUE;
               continue;
          }
     }

     if (!filename) {
          print_usage (argv[0]);
          return DFB_FALSE;
     }

     return DFB_TRUE;
}

static void
set_background_color()
{
     DFBResult  ret;
     char      *error;
     __u32      argb;

     if (*filename == '#')
          filename++;

     argb = strtoul( filename, &error, 16 );

     if (*error) {
          fprintf( stderr,
                   "Invalid characters in color string: '%s'\n", error );
          return;
     }

     ret = layer->SetBackgroundColor( layer,
                                      (argb & 0xFF0000)   >> 16,
                                      (argb & 0xFF00)     >> 8,
                                      (argb & 0xFF)       >> 0,
                                      (argb & 0xFF000000) >> 24 );
     if (ret) {
          DirectFBError( "IDirectFBDisplayLayer::SetBackgroundColor() failed", ret );
          return;
     }
     
     ret = layer->SetBackgroundMode( layer, DLBM_COLOR );
     if (ret)
          DirectFBError( "IDirectFBDisplayLayer::SetBackgroundMode() failed", ret );
}

static void
set_background_image()
{
     DFBResult               ret;
     DFBSurfaceDescription   desc;
     IDirectFBSurface       *surface;
     IDirectFBImageProvider *provider;

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

     if (!tiled) {
          DFBDisplayLayerConfig   config;

          ret = layer->GetConfiguration( layer, &config );
          if (ret) {
               DirectFBError( "IDirectFBDisplayLayer::GetConfiguration() failed", ret );
               provider->Release( provider );
               return;
          }

          desc.width  = config.width;
          desc.height = config.height;
     }

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

     ret = layer->SetBackgroundMode( layer, tiled ? DLBM_TILE : DLBM_IMAGE );
     if (ret)
          DirectFBError( "IDirectFBDisplayLayer::SetBackgroundMode() failed", ret );

     surface->Release( surface );
     provider->Release( provider );
}
