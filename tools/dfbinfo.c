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
static const DirectFBScreenEncoderCapabilitiesNames(encoder_caps);
static const DirectFBScreenEncoderTypeNames(encoder_type);
static const DirectFBScreenEncoderTVStandardsNames(tv_standards);
static const DirectFBScreenOutputCapabilitiesNames(output_caps);
static const DirectFBScreenOutputConnectorsNames(connectors);
static const DirectFBScreenOutputSignalsNames(signals);
static const DirectFBScreenMixerCapabilitiesNames(mixer_caps);

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
     printf( "        Type:    " );

     for (i=0; layer_types[i].type; i++) {
          if (desc.type & layer_types[i].type)
               printf( "%s ", layer_types[i].name );
     }

     printf( "\n" );


     /* Caps */
     printf( "        Caps:    " );

     for (i=0; layer_caps[i].capability; i++) {
          if (desc.caps & layer_caps[i].capability)
               printf( "%s ", layer_caps[i].name );
     }

     printf( "\n" );


     /* Sources */
     if (desc.caps & DLCAPS_SOURCES) {
          DFBResult                         ret;
          IDirectFBDisplayLayer            *layer;
          DFBDisplayLayerSourceDescription  descs[desc.sources];

          ret = dfb->GetDisplayLayer( dfb, id, &layer );
          if (ret) {
               DirectFBError( "DirectFB::GetDisplayLayer() failed", ret );
          }
          else {
               ret = layer->GetSourceDescriptions( layer, descs );
               if (ret) {
                    DirectFBError( "DirectFBDisplayLayer::GetSourceDescriptions() failed", ret );
               }
               else {
                    printf( "        Sources: " );

                    for (i=0; i<desc.sources; i++) {
                         if (i > 0)
                              printf( ", %s", descs[i].name );
                         else
                              printf( "%s", descs[i].name );
                    }

                    printf( "\n" );
               }

               layer->Release( layer );
          }
     }


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

static void
dump_mixers( IDirectFBScreen *screen,
             int              num )
{
     int                       i, n;
     DFBResult                 ret;
     DFBScreenMixerDescription descs[num];

     ret = screen->GetMixerDescriptions( screen, descs );
     if (ret) {
          DirectFBError( "IDirectFBScreen::GetMixerDescriptions", ret );
          return;
     }

     for (i=0; i<num; i++) {
          printf( "   Mixer (%d) %s\n", i, descs[i].name );

          /* Caps */
          printf( "     Caps:                    " );

          for (n=0; mixer_caps[n].capability; n++) {
               if (descs[i].caps & mixer_caps[n].capability)
                    printf( "%s ", mixer_caps[n].name );
          }

          printf( "\n" );


          /* Full mode layers */
          if (descs[i].caps & DSMCAPS_FULL) {
               printf( "     Layers (full mode):      " );

               for (n=0; n<DFB_DISPLAYLAYER_IDS_MAX; n++) {
                    if (DFB_DISPLAYLAYER_IDS_HAVE( descs[i].layers, n ))
                         printf( "(%02x) ", n );
               }

               printf( "\n" );
          }


          /* Sub mode layers */
          if (descs[i].caps & DSMCAPS_SUB_LAYERS) {
               printf( "     Layers (sub mode): %2d of ", descs[i].sub_num );

               for (n=0; n<DFB_DISPLAYLAYER_IDS_MAX; n++) {
                    if (DFB_DISPLAYLAYER_IDS_HAVE( descs[i].sub_layers, n ))
                         printf( "(%02x) ", n );
               }

               printf( "\n" );
          }

          printf( "\n" );
     }

     printf( "\n" );
}

static void
dump_encoders( IDirectFBScreen *screen,
               int              num )
{
     int                         i, n;
     DFBResult                   ret;
     DFBScreenEncoderDescription descs[num];

     ret = screen->GetEncoderDescriptions( screen, descs );
     if (ret) {
          DirectFBError( "IDirectFBScreen::GetEncoderDescriptions", ret );
          return;
     }

     for (i=0; i<num; i++) {
          printf( "   Encoder (%d) %s\n", i, descs[i].name );

          /* Type */
          printf( "     Type:           " );

          for (n=0; encoder_type[n].type; n++) {
               if (descs[i].type == encoder_type[n].type)
                    printf( "%s ", encoder_type[n].name );
          }

          printf( "\n" );


          /* Caps */
          printf( "     Caps:           " );

          for (n=0; encoder_caps[n].capability; n++) {
               if (descs[i].caps & encoder_caps[n].capability)
                    printf( "%s ", encoder_caps[n].name );
          }

          printf( "\n" );


          /* TV Norms */
          if (descs[i].caps & DSECAPS_TV_STANDARDS) {
               printf( "     TV Standards:   " );

               for (n=0; tv_standards[n].standard; n++) {
                    if (descs[i].tv_standards & tv_standards[n].standard)
                         printf( "%s ", tv_standards[n].name );
               }

               printf( "\n" );
          }


          /* Output signals */
          if (descs[i].caps & DSECAPS_OUT_SIGNALS) {
               printf( "     Output Signals: " );

               for (n=0; signals[n].signal; n++) {
                    if (descs[i].out_signals & signals[n].signal)
                         printf( "%s ", signals[n].name );
               }

               printf( "\n" );
          }

          printf( "\n" );
     }

     printf( "\n" );
}

static void
dump_outputs( IDirectFBScreen *screen,
              int              num )
{
     int                        i, n;
     DFBResult                  ret;
     DFBScreenOutputDescription descs[num];

     ret = screen->GetOutputDescriptions( screen, descs );
     if (ret) {
          DirectFBError( "IDirectFBScreen::GetOutputDescriptions", ret );
          return;
     }

     for (i=0; i<num; i++) {
          printf( "   Output (%d) %s\n", i, descs[i].name );


          /* Caps */
          printf( "     Caps:       " );

          for (n=0; output_caps[n].capability; n++) {
               if (descs[i].caps & output_caps[n].capability)
                    printf( "%s ", output_caps[n].name );
          }

          printf( "\n" );


          /* Connectors */
          printf( "     Connectors: " );

          for (n=0; connectors[n].connector; n++) {
               if (descs[i].all_connectors & connectors[n].connector)
                    printf( "%s ", connectors[n].name );
          }

          printf( "\n" );


          /* Signals */
          printf( "     Signals:    " );

          for (n=0; signals[n].signal; n++) {
               if (descs[i].all_signals & signals[n].signal)
                    printf( "%s ", signals[n].name );
          }

          printf( "\n" );

          printf( "\n" );
     }

     printf( "\n" );
}

static DFBEnumerationResult
screen_callback( DFBScreenID           id,
                 DFBScreenDescription  desc,
                 void                 *arg )
{
     int              i;
     DFBResult        ret;
     IDirectFBScreen *screen;

     ret = dfb->GetScreen( dfb, id, &screen );
     if (ret)
          DirectFBErrorFatal( "IDirectFB::GetScreen", ret );

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

     printf( "\n\n" );


     /* Mixers */
     if (desc.mixers)
          dump_mixers( screen, desc.mixers );

     /* Encoders */
     if (desc.encoders)
          dump_encoders( screen, desc.encoders );

     /* Outputs */
     if (desc.outputs)
          dump_outputs( screen, desc.outputs );

     /* Display layers */
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

