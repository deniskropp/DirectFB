/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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
#include <unistd.h>

#include <directfb.h>
#include <directfb_strings.h>

static DirectFBPixelFormatNames( format_names );

/*****************************************************************************/

static IDirectFB                  *dfb;
static IDirectFBDisplayLayer      *layer;
static DFBDisplayLayerDescription  desc;

/*****************************************************************************/

static DFBDisplayLayerID         id         = DLID_PRIMARY;
static int                       width      = 0;
static int                       height     = 0;
static DFBSurfacePixelFormat     format     = DSPF_UNKNOWN;
static DFBDisplayLayerBufferMode buffermode = -1;
static int                       opacity    = -1;
static int                       level      = 0;
static DFBBoolean                set_level  = DFB_FALSE;

/*****************************************************************************/

static DFBBoolean parse_command_line( int argc, char *argv[] );
static void       set_configuration ( void );

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

     /* Create the super interface. */
     ret = DirectFBCreate( &dfb );
     if (ret) {
          DirectFBError( "DirectFBCreate() failed", ret );
          return -3;
     }

     /* Get the primary display layer. */
     ret = dfb->GetDisplayLayer( dfb, id, &layer );
     if (ret) {
          if (ret == DFB_IDNOTFOUND)
               fprintf (stderr, "\nUnknown layer id, check 'dfbinfo' for valid values.\n\n");
          else
               DirectFBError( "IDirectFB::GetDisplayLayer() failed", ret );
          dfb->Release( dfb );
          return -4;
     }

     /* Get a description of the layer. */
     ret = layer->GetDescription( layer, &desc );
     if (ret) {
          DirectFBError( "IDirectFBDisplayLayer::GetDescription() failed", ret );
          layer->Release( layer );
          dfb->Release( dfb );
          return -5;
     }

     /* Acquire administrative cooperative level. */
     ret = layer->SetCooperativeLevel( layer, DLSCL_ADMINISTRATIVE );
     if (ret) {
          DirectFBError( "IDirectFBDisplayLayer::SetCooperativeLevel() failed", ret );
          layer->Release( layer );
          dfb->Release( dfb );
          return -6;
     }

     /* Show/change the configuration. */
     set_configuration();

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
     int i = 0;

     fprintf (stderr, "\nDirectFB Layer Configuration (version %s)\n\n", DIRECTFB_VERSION);
     fprintf (stderr, "Usage: %s [options]\n\n", prg_name);
     fprintf (stderr, "Options:\n");
     fprintf (stderr, "   -l, --layer   <id>              Use the specified layer, default is primary\n");
     fprintf (stderr, "   -m, --mode    <width>x<height>  Change the resolution (pixels)\n");
     fprintf (stderr, "   -f, --format  <pixelformat>     Change the pixel format\n");
     fprintf (stderr, "   -b, --buffer  <buffermode>      Change the buffer mode (single/video/system)\n");
     fprintf (stderr, "   -o, --opacity <opacity>         Change the layer's opacity (0-255)\n");
     fprintf (stderr, "   -L, --level   <level>           Change the layer's level\n");
     fprintf (stderr, "   -h, --help                      Show this help message\n");
     fprintf (stderr, "   -v, --version                   Print version information\n");
     fprintf (stderr, "\n");

     fprintf (stderr, "Known pixel formats:\n");

     while (format_names[i].format != DSPF_UNKNOWN) {
          DFBSurfacePixelFormat format = format_names[i].format;

          fprintf (stderr, "   %-10s %2d bits, %d bytes",
                   format_names[i].name, DFB_BITS_PER_PIXEL(format),
                   DFB_BYTES_PER_PIXEL(format));

          if (DFB_PIXELFORMAT_HAS_ALPHA(format))
               fprintf (stderr, "   ALPHA");

          if (DFB_PIXELFORMAT_IS_INDEXED(format))
               fprintf (stderr, "   INDEXED");

          if (DFB_PLANAR_PIXELFORMAT(format)) {
               int planes = DFB_PLANE_MULTIPLY(format, 1000);

               fprintf (stderr, "   PLANAR (x%d.%03d)",
                        planes / 1000, planes % 1000);
          }

          fprintf (stderr, "\n");

          ++i;
     }
     fprintf (stderr, "\n");

     fprintf (stderr, "Valid buffer modes:\n");
     fprintf (stderr, "   FRONTONLY     or 'single'\n");
     fprintf (stderr, "   BACKVIDEO     or 'video'\n");
     fprintf (stderr, "   BACKSYSTEM    or 'system'\n");
     fprintf (stderr, "   TRIPLE\n");
     fprintf (stderr, "   WINDOWS\n");

     fprintf (stderr, "\n");
     fprintf (stderr, "Specifying neither mode nor format just displays the current configuration.\n");
     fprintf (stderr, "\n");
}

static DFBBoolean
parse_layer( const char *arg )
{
     if (sscanf( arg, "%d", &id ) != 1 || id < 0) {
          fprintf (stderr, "\n"
                   "Invalid layer id specified!\n"
                   "Check 'dfbinfo' for valid values.\n\n");

          return DFB_FALSE;
     }

     return DFB_TRUE;
}

static DFBBoolean
parse_mode( const char *arg )
{
     if (sscanf( arg, "%dx%d", &width, &height ) != 2 ||
         width < 1 || height < 1)
     {
          fprintf (stderr, "\nInvalid mode specified!\n\n" );
          return DFB_FALSE;
     }

     return DFB_TRUE;
}

static DFBBoolean
parse_format( const char *arg )
{
     int i = 0;

     while (format_names[i].format != DSPF_UNKNOWN) {
          if (!strcasecmp( arg, format_names[i].name )) {
               format = format_names[i].format;
               return DFB_TRUE;
          }

          ++i;
     }

     fprintf (stderr, "\nInvalid format specified!\n\n" );

     return DFB_FALSE;
}

static DFBBoolean
parse_buffermode( const char *arg )
{
     if (!strcasecmp( arg, "single" ) || !strcasecmp( arg, "frontonly" ))
          buffermode = DLBM_FRONTONLY;
     else if (!strcasecmp( arg, "system" ) || !strcasecmp( arg, "backsystem" ))
          buffermode = DLBM_BACKSYSTEM;
     else if (!strcasecmp( arg, "video" ) || !strcasecmp( arg, "backvideo" ))
          buffermode = DLBM_BACKVIDEO;
     else if (!strcasecmp( arg, "triple" ))
          buffermode = DLBM_TRIPLE;
     else if (!strcasecmp( arg, "windows" ))
          buffermode = DLBM_WINDOWS;
     else {
          fprintf (stderr, "\nInvalid buffer mode specified!\n\n" );
          return DFB_FALSE;
     }

     return DFB_TRUE;
}

static DFBBoolean
parse_opacity( const char *arg )
{
     if (sscanf( arg, "%d", &opacity ) != 1 || opacity < 0 || opacity > 255) {
          fprintf (stderr, "\nInvalid opacity value specified!\n\n");

          return DFB_FALSE;
     }

     return DFB_TRUE;
}

static DFBBoolean
parse_level( const char *arg )
{
     if (sscanf( arg, "%d", &level ) != 1) {
          fprintf (stderr, "\nInvalid level specified!\n\n");

          return DFB_FALSE;
     }

     set_level = DFB_TRUE;

     return DFB_TRUE;
}

static DFBBoolean
parse_command_line( int argc, char *argv[] )
{
     int n;

     for (n = 1; n < argc; n++) {
          const char *arg = argv[n];

          if (strcmp (arg, "-h") == 0 || strcmp (arg, "--help") == 0) {
               print_usage (argv[0]);
               return DFB_FALSE;
          }

          if (strcmp (arg, "-v") == 0 || strcmp (arg, "--version") == 0) {
               fprintf (stderr, "dfbg version %s\n", DIRECTFB_VERSION);
               return DFB_FALSE;
          }

          if (strcmp (arg, "-l") == 0 || strcmp (arg, "--layer") == 0) {
               if (++n == argc) {
                    print_usage (argv[0]);
                    return DFB_FALSE;
               }

               if (!parse_layer( argv[n] ))
                    return DFB_FALSE;

               continue;
          }

          if (strcmp (arg, "-m") == 0 || strcmp (arg, "--mode") == 0) {
               if (++n == argc) {
                    print_usage (argv[0]);
                    return DFB_FALSE;
               }

               if (!parse_mode( argv[n] ))
                    return DFB_FALSE;

               continue;
          }

          if (strcmp (arg, "-f") == 0 || strcmp (arg, "--format") == 0) {
               if (++n == argc) {
                    print_usage (argv[0]);
                    return DFB_FALSE;
               }

               if (!parse_format( argv[n] ))
                    return DFB_FALSE;

               continue;
          }

          if (strcmp (arg, "-b") == 0 || strcmp (arg, "--buffer") == 0) {
               if (++n == argc) {
                    print_usage (argv[0]);
                    return DFB_FALSE;
               }

               if (!parse_buffermode( argv[n] ))
                    return DFB_FALSE;

               continue;
          }

          if (strcmp (arg, "-o") == 0 || strcmp (arg, "--opacity") == 0) {
               if (++n == argc) {
                    print_usage (argv[0]);
                    return DFB_FALSE;
               }

               if (!parse_opacity( argv[n] ))
                    return DFB_FALSE;

               continue;
          }

          if (strcmp (arg, "-L") == 0 || strcmp (arg, "--level") == 0) {
               if (++n == argc) {
                    print_usage (argv[0]);
                    return DFB_FALSE;
               }

               if (!parse_level( argv[n] ))
                    return DFB_FALSE;

               continue;
          }

          print_usage (argv[0]);

          return DFB_FALSE;
     }

     return DFB_TRUE;
}

static void
set_configuration()
{
     DFBResult             ret;
     DFBDisplayLayerConfig config;

     printf( "\n" );
     printf( "%s\n", desc.name );
     printf( "\n" );

     config.flags = DLCONF_NONE;

     if (width) {
          config.flags |= DLCONF_WIDTH;
          config.width  = width;
     }

     if (height) {
          config.flags  |= DLCONF_HEIGHT;
          config.height  = height;
     }

     if (format != DSPF_UNKNOWN) {
          config.flags       |= DLCONF_PIXELFORMAT;
          config.pixelformat  = format;
     }

     if (buffermode != -1) {
          config.flags      |= DLCONF_BUFFERMODE;
          config.buffermode  = buffermode;
     }

     /* Set the configuration if anything changed. */
     if (config.flags) {
          ret = layer->SetConfiguration( layer, &config );
          if (ret) {
               DirectFBError( "IDirectFBDisplayLayer::SetConfiguration() failed", ret );
               return;
          }
     }

     /* Get and show the current (new) configuration. */
     ret = layer->GetConfiguration( layer, &config );
     if (ret) {
          DirectFBError( "IDirectFBDisplayLayer::GetConfiguration() failed", ret );
          return;
     }

     if (config.flags & DLCONF_WIDTH)
          printf( "Width       %d\n", config.width );

     if (config.flags & DLCONF_HEIGHT)
          printf( "Height      %d\n", config.height );

     if (config.flags & DLCONF_PIXELFORMAT)
          printf( "Format      %s\n",
                  format_names[DFB_PIXELFORMAT_INDEX(config.pixelformat)].name );

     if (config.flags & DLCONF_BUFFERMODE) {
          printf( "Buffermode  " );

          switch (config.buffermode) {
               case DLBM_FRONTONLY:
                    printf( "FRONTONLY\n" );
                    break;
               case DLBM_BACKVIDEO:
                    printf( "BACKVIDEO\n" );
                    break;
               case DLBM_BACKSYSTEM:
                    printf( "BACKSYSTEM\n" );
                    break;
               case DLBM_TRIPLE:
                    printf( "TRIPLE\n" );
                    break;
               case DLBM_WINDOWS:
                    printf( "WINDOWS\n" );
                    break;
               default:
                    printf( "unknown!\n" );
                    break;
          }
     }

     if (config.flags & DLCONF_OPTIONS) {
          printf( "Options     " );

          if (config.options == DLOP_NONE) {
               printf( "none\n" );
          }
          else {
               if (config.options & DLOP_ALPHACHANNEL)
                    printf( "ALPHA CHANNEL       " );

               if (config.options & DLOP_DEINTERLACING)
                    printf( "DEINTERLACING       " );

               if (config.options & DLOP_DST_COLORKEY)
                    printf( "DST COLOR KEY       " );

               if (config.options & DLOP_FIELD_PARITY)
                    printf( "FIELD PARITY        " );

               if (config.options & DLOP_FLICKER_FILTERING)
                    printf( "FLICKER FILTERING   " );

               if (config.options & DLOP_OPACITY)
                    printf( "OPACITY             " );

               if (config.options & DLOP_SRC_COLORKEY)
                    printf( "SRC COLOR KEY       " );

               printf( "\n" );
          }
     }

     printf( "\n" );

     /* Set the opacity if requested. */
     if (opacity != -1) {
          ret = layer->SetOpacity( layer, opacity );
          if (ret == DFB_UNSUPPORTED)
               fprintf( stderr, "Opacity value (%d) not supported!\n\n", opacity );
          else if (ret)
               DirectFBError( "IDirectFBDisplayLayer::SetOpacity() failed", ret );
     }

     /* Set the level if requested. */
     if (set_level) {
          ret = layer->SetLevel( layer, level );
          if (ret == DFB_UNSUPPORTED)
               fprintf( stderr, "Level (%d) not supported!\n\n", level );
          else if (ret)
               DirectFBError( "IDirectFBDisplayLayer::SetLevel() failed", ret );
     }
}

