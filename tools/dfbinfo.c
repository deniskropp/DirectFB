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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "directfb.h"


/*****************************************************************************/

static IDirectFB *dfb = NULL;

/*****************************************************************************/

static DFBBoolean parse_command_line ( int argc, char *argv[] );
static void       enum_input_devices ();

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

     enum_input_devices();

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
     return DFB_TRUE;
}

static DFBEnumerationResult
input_device_callback( DFBInputDeviceID           id,
                       DFBInputDeviceDescription  desc,
                       void                      *arg )
{
     /* Name */
     printf( "(%02x) %-30s", id, desc.name );

     switch (id) {
          case DIDID_JOYSTICK:
               printf( "  (primary joystick)" );
               break;
          case DIDID_KEYBOARD:
               printf( "  (primary keyboard)" );
               break;
          case DIDID_MOUSE:
               printf( "  (primary mouse)" );
               break;
          case DIDID_REMOTE:
               printf( "  (primary remote control)" );
               break;
          default:
               break;
     }

     printf( "\n" );

     /* Type */
     printf( "        Type: " );

     if (desc.type & DIDTF_KEYBOARD)
          printf( "keyboard " );

     if (desc.type & DIDTF_MOUSE)
          printf( "mouse " );

     if (desc.type & DIDTF_JOYSTICK)
          printf( "joystick " );

     if (desc.type & DIDTF_REMOTE)
          printf( "remote " );

     if (desc.type & DIDTF_VIRTUAL)
          printf( "virtual " );

     printf( "\n" );

     /* Caps */
     printf( "        Caps: " );

     if (desc.caps & DICAPS_AXES)
          printf( "axes " );

     if (desc.caps & DICAPS_BUTTONS)
          printf( "buttons " );

     if (desc.caps & DICAPS_KEYS)
          printf( "keys " );

     printf( "\n" );

     printf( "\n" );

     return DFB_OK;
}

static void
enum_input_devices()
{
     DFBResult ret;

     printf( "\nInput Devices\n\n" );

     ret = dfb->EnumInputDevices( dfb, input_device_callback, NULL );
     if (ret)
          DirectFBError( "IDirectFB::EnumInputDevices", ret );
}

