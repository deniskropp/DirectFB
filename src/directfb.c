/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <malloc.h>
#include <string.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <pthread.h>

#include "directfb.h"
#include "directfb_internals.h"
#include "directfb_version.h"
 
#include "misc/conf.h"

#include "core/core.h"
#include "core/coredefs.h"
#include "core/coretypes.h"

#include "core/input.h"
#include "core/layers.h"
#include "core/state.h"
#include "core/gfxcard.h"
#include "core/surfaces.h"
#include "core/windows.h"
#include "core/vt.h"

#include "display/idirectfbsurface.h"

#include "idirectfb.h"


IDirectFB *idirectfb_singleton = NULL;

/*
 * Version checking
 */
const unsigned int directfb_major_version = DIRECTFB_MAJOR_VERSION;
const unsigned int directfb_minor_version = DIRECTFB_MINOR_VERSION;
const unsigned int directfb_micro_version = DIRECTFB_MICRO_VERSION;
const unsigned int directfb_binary_age    = DIRECTFB_BINARY_AGE;
const unsigned int directfb_interface_age = DIRECTFB_INTERFACE_AGE;

char * DirectFBCheckVersion (unsigned int required_major,
                             unsigned int required_minor,
                             unsigned int required_micro)
{
     if (required_major > DIRECTFB_MAJOR_VERSION)
          return "DirectFB version too old (major mismatch)";
     if (required_major < DIRECTFB_MAJOR_VERSION)
          return "DirectFB version too new (major mismatch)";
     if (required_minor > DIRECTFB_MINOR_VERSION)
          return "DirectFB version too old (minor mismatch)";
     if (required_minor < DIRECTFB_MINOR_VERSION)
          return "DirectFB version too new (minor mismatch)";
     if (required_micro < DIRECTFB_MICRO_VERSION - DIRECTFB_BINARY_AGE)
          return "DirectFB version too new (micro mismatch)";
     if (required_micro > DIRECTFB_MICRO_VERSION)
          return "DirectFB version too old (micro mismatch)";

     return NULL;
}

static ReactionResult keyboard_handler( const void *msg_data, void *ctx )
{
     const DFBInputEvent *evt = (DFBInputEvent*)msg_data;

     if (evt->type == DIET_KEYPRESS &&
         (evt->modifiers & (DIMK_CTRL|DIMK_ALT)) == (DIMK_CTRL|DIMK_ALT))
     {
          switch (evt->keycode) {
               case DIKC_BACKSPACE:
                    kill( getpid(), SIGINT );
                    return RS_DROP;

               case DIKC_F1 ... DIKC_F12: {
                    int num = evt->keycode - DIKC_F1 + 1;

                    DEBUGMSG( "DirectFB/directfb/keyboard_handler: "
                              "Locking/unlocking hardware to "
                              "increase the chance of idle hardware...\n" );

                    pthread_mutex_lock( &card->lock );
                    pthread_mutex_unlock( &card->lock );

                    DEBUGMSG( "DirectFB/directfb/keyboard_handler: "
                              "Switching to VT %d...\n", num );

                    if (ioctl( vt->fd, VT_ACTIVATE, num ))
                         PERRORMSG( "DirectFB/directfb/keyboard_handler: "
                                    "VT_ACTIVATE for VT %d failed!\n", num );

                    //ioctl( vt->fd, VT_WAITACTIVE, num );

                    DEBUGMSG( "DirectFB/directfb/keyboard_handler: "
                              "...hopefully switched to VT %d.\n", num );

                    return RS_DROP;
               }

               default:
                    return RS_OK;
          }
     }

     return RS_OK; /* continue dispatching this event */
}

DFBResult DirectFBInit( int *argc, char **argv[] )
{
     DFBResult ret;

     ret = config_init( argc, argv );
     if (ret)
          return ret;

     return DFB_OK;
}

DFBResult DirectFBSetOption( char *name, char *value)
{
     DFBResult ret;

     if (dfb_config == NULL) {
          ERRORMSG( "DirectFB/DirectFBSetOption: DirectFBInit has to be "
          "called before DirectFBSetOption!\n" );
          return DFB_INIT;
     }

     if (idirectfb_singleton) {
          ERRORMSG( "DirectFB/DirectFBSetOption: DirectFBSetOption has to be "
          "called before DirectFBCreate!\n" );
          return DFB_INIT;
     }

     ret = config_set(name, value);
     if (ret)
          return ret;

     return DFB_OK;
}


/*
 * Programs have to call this to get the super interface
 * which is needed to access other functions
 */
DFBResult DirectFBCreate( IDirectFB **interface )
{
     DFBResult ret;
     DFBDisplayLayerConfig layer_config;

     if (dfb_config == NULL) {
          ERRORMSG( "DirectFB/DirectFBCreate: DirectFBInit has to be called "
                    "before DirectFBCreate!\n" );
          return DFB_INIT;
     }

     if (idirectfb_singleton) {
          idirectfb_singleton->AddRef( idirectfb_singleton );
          *interface = idirectfb_singleton;
          return DFB_OK;
     }

     if (!dfb_config->quiet && !dfb_config->no_banner) {
          printf( "\n" );
          printf( "       ----------------------- DirectFB v%d.%d.%d ---------------------\n",
                  DIRECTFB_MAJOR_VERSION, DIRECTFB_MINOR_VERSION, DIRECTFB_MICRO_VERSION );
          printf( "                (c)2000  convergence integrated media GmbH  \n" );
          printf( "        -----------------------------------------------------------\n" );
          printf( "\n" );
     }

     ret = core_init();
     if (ret)
          return ret;

     {
          InputDevice *d = inputdevices;

          while (d) {
               if (d->id == DIDID_KEYBOARD)
                    reactor_attach( d->reactor, keyboard_handler, NULL );

               d = d->next;
          }
     }

     DFB_ALLOCATE_INTERFACE( idirectfb_singleton, IDirectFB );

     ret = IDirectFB_Construct( idirectfb_singleton );
     if (ret) {
          free( idirectfb_singleton );
          idirectfb_singleton = NULL;
          return ret;
     }

     *interface = idirectfb_singleton;

     /* set buffer mode for desktop */
     layer_config.flags = DLCONF_BUFFERMODE;

     if (dfb_config->buffer_mode == -1) {
          if (card->caps.accel & DFXL_BLIT)
               layer_config.buffermode = DLBM_BACKVIDEO;
          else
               layer_config.buffermode = DLBM_BACKSYSTEM;
     }
     else
          layer_config.buffermode = dfb_config->buffer_mode;

     if (layers->SetConfiguration( layers, &layer_config )) {
          ERRORMSG( "DirectFB/DirectFBCreate: "
                    "Setting primary layer buffer mode failed! "
                    "-> No support for virtual resolutions (panning)?\n" );
     }

     /* set desktop background */
     layers->bg.mode  = dfb_config->layer_bg_mode;
     layers->bg.color = dfb_config->layer_bg_color;

     if (dfb_config->layer_bg_mode == DLBM_IMAGE) {
          DFBSurfaceDescription   desc;
          IDirectFBImageProvider *provider;
          IDirectFBSurface       *image;
          IDirectFBSurface_data  *image_data;

          ret = (*interface)->CreateImageProvider( *interface, dfb_config->layer_bg_filename, &provider );
          if (ret) {
               DirectFBError( "Failed loading background image", ret );
               return DFB_INIT;
          }

          desc.flags = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
          desc.width = layers->width;
          desc.height = layers->height;
          desc.pixelformat = layers->surface->format;


          ret = (*interface)->CreateSurface( *interface, &desc, &image );
          if (ret) {
               DirectFBError( "Failed creating surface for background image", ret );

               provider->Release( provider );
               free( provider );

               return DFB_INIT;
          }

          ret = provider->RenderTo( provider, image );
          if (ret) {
               DirectFBError( "Failed loading background image", ret );

               image->Release( image );
               free( image );
               provider->Release( provider );
               free( provider );

               return DFB_INIT;
          }

          provider->Release( provider );
          free( provider );


          image_data = (IDirectFBSurface_data*) image->priv;

          layers->bg.image = image_data->surface;
     }

     windowstack_repaint_all( layers->windowstack );

     return DFB_OK;
}

void DirectFBError( const char *msg, DFBResult error )
{
     if (msg)
          fprintf( stderr, "(#) DirectFBError [%s]: ", msg );
     else
          fprintf( stderr, "(#) DirectFBError: " );

     switch (error) {
          case DFB_OK:
               fprintf( stderr, "Everything OK!\n" );
               break;
          case DFB_FAILURE:
               fprintf( stderr, "General failure!\n" );
               break;
          case DFB_INIT:
               fprintf( stderr, "General initialization failure!\n" );
               break;
          case DFB_BUG:
               fprintf( stderr, "Internal bug!\n" );
               break;
          case DFB_DEAD:
               fprintf( stderr, "Interface is dead!\n" );
               break;
          case DFB_UNSUPPORTED:
               fprintf( stderr, "Not supported!\n" );
               break;
          case DFB_UNIMPLEMENTED:
               fprintf( stderr, "Unimplemented!\n" );
               break;
          case DFB_ACCESSDENIED:
               fprintf( stderr, "Access denied!\n" );
               break;
          case DFB_INVARG:
               fprintf( stderr, "Invalid argument(s)!\n" );
               break;
          case DFB_NOSYSTEMMEMORY:
               fprintf( stderr, "Out of system memory!\n" );
               break;
          case DFB_NOVIDEOMEMORY:
               fprintf( stderr, "Out of video memory!\n" );
               break;
          case DFB_LOCKED:
               fprintf( stderr, "Resource (already) locked!\n" );
               break;
          case DFB_BUFFEREMPTY:
               fprintf( stderr, "Buffer is empty!\n" );
               break;
          case DFB_FILENOTFOUND:
               fprintf( stderr, "File not found!\n" );
               break;
          case DFB_IO:
               fprintf( stderr, "General I/O failure!\n" );
               break;
          case DFB_NOIMPL:
               fprintf( stderr, "Interface implementation not available!\n" );
               break;
          case DFB_MISSINGFONT:
               fprintf( stderr, "No font has been set!\n" );
               break;
          case DFB_TIMEOUT:
               fprintf( stderr, "Operation timed out!\n" );
               break;
          case DFB_MISSINGIMAGE:
               fprintf( stderr, "No image has been set!\n" );
               break;
          default:
               fprintf( stderr, "UNKNOWN ERROR CODE!\n" );
               break;
     }
}

void DirectFBErrorFatal( const char *msg, DFBResult error )
{
     DirectFBError( msg, error );

     /* Deinit all stuff here. */
     core_deinit();     /* for now, this dirty thing should work */

     exit( error );
}

