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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <directfb.h>
#include <directfb_strings.h>

static const DirectFBInputDeviceTypeFlagsNames(input_types);
static const DirectFBInputDeviceCapabilitiesNames(input_caps);

static const DirectFBDisplayLayerTypeFlagsNames(layer_types);
static const DirectFBDisplayLayerCapabilitiesNames(layer_caps);

static const DirectFBScreenCapabilitiesNames(screen_caps);

/*****************************************************************************/

static IDirectFB *dfb = NULL;

/*****************************************************************************/

static DFBBoolean parse_command_line ( int argc, char *argv[] );
static void       enum_input_devices ();
static void       enum_screens ();

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

     printf( "\n" );

     enum_screens();
     enum_input_devices();

     /* Release the super interface. */
     dfb->Release( dfb );

     return EXIT_SUCCESS;
}

/*****************************************************************************/

static DFBBoolean
parse_command_line( int argc, char *argv[] )
{
     return DFB_TRUE;
}

/*****************************************************************************/

static DFBEnumerationResult
input_device_callback( DFBInputDeviceID           id,
                       DFBInputDeviceDescription  desc,
                       void                      *arg )
{
     int i;

     /* Name */
     printf( "Input (%02x) %-30s", id, desc.name );

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
     printf( "   Type: " );

     for (i=0; input_types[i].type; i++) {
          if (desc.type & input_types[i].type)
               printf( "%s ", input_types[i].name );
     }

     printf( "\n" );

     /* Caps */
     printf( "   Caps: " );

     for (i=0; input_caps[i].capability; i++) {
          if (desc.caps & input_caps[i].capability)
               printf( "%s ", input_caps[i].name );
     }

     printf( "\n" );

     printf( "\n" );

     return DFB_OK;
}

static void
enum_input_devices()
{
     DFBResult ret;

     printf( "\n" );

     ret = dfb->EnumInputDevices( dfb, input_device_callback, NULL );
     if (ret)
          DirectFBError( "IDirectFB::EnumInputDevices", ret );
}

/*****************************************************************************/

static DFBEnumerationResult
display_layer_callback( DFBDisplayLayerID           id,
                        DFBDisplayLayerDescription  desc,
                        void                       *arg )
{
     int i;

     /* Name */
     printf( "     Layer (%02x) %-30s", id, desc.name );

     switch (id) {
          case DLID_PRIMARY:
               printf( "  (primary layer)" );
               break;
          default:
               break;
     }

     printf( "\n" );

     /* Type */
     printf( "        Type: " );

     for (i=0; layer_types[i].type; i++) {
          if (desc.type & layer_types[i].type)
               printf( "%s ", layer_types[i].name );
     }

     printf( "\n" );

     /* Caps */
     printf( "        Caps: " );

     for (i=0; layer_caps[i].capability; i++) {
          if (desc.caps & layer_caps[i].capability)
               printf( "%s ", layer_caps[i].name );
     }

     printf( "\n" );

     printf( "\n" );

     return DFB_OK;
}

static void
enum_display_layers( IDirectFBScreen *screen )
{
     DFBResult ret;

     ret = screen->EnumDisplayLayers( screen, display_layer_callback, NULL );
     if (ret)
          DirectFBError( "IDirectFBScreen::EnumDisplayLayers", ret );
}

/*****************************************************************************/

static DFBEnumerationResult
screen_callback( DFBScreenID           id,
                 DFBScreenDescription  desc,
                 void                 *arg )
{
     int              i;
     DFBResult        ret;
     IDirectFBScreen *screen;

     /* Name */
     printf( "Screen (%02x) %-30s", id, desc.name );

     switch (id) {
          case DSCID_PRIMARY:
               printf( "  (primary screen)" );
               break;
          default:
               break;
     }

     printf( "\n" );

     /* Caps */
     printf( "   Caps: " );

     for (i=0; screen_caps[i].capability; i++) {
          if (desc.caps & screen_caps[i].capability)
               printf( "%s ", screen_caps[i].name );
     }

     printf( "\n" );

     printf( "\n" );


     ret = dfb->GetScreen( dfb, id, &screen );
     if (ret)
          DirectFBErrorFatal( "IDirectFB::GetScreen", ret );

     enum_display_layers( screen );

     screen->Release( screen );

     return DFB_OK;
}

static void
enum_screens()
{
     DFBResult ret;

     printf( "\n" );

     ret = dfb->EnumScreens( dfb, screen_callback, NULL );
     if (ret)
          DirectFBError( "IDirectFB::EnumScreens", ret );
}

