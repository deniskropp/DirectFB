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

#include "misc/mem.h"

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

               case DIKC_F1 ... DIKC_F12:
                    if (dfb_config->vt_switching) {
                         int num = evt->keycode - DIKC_F1 + 1;

                         DEBUGMSG( "DirectFB/directfb/keyboard_handler: "
                                   "Locking/unlocking hardware to "
                                   "increase the chance of idle hardware...\n" );

                         pthread_mutex_lock( &card->lock );
                         pthread_mutex_unlock( &card->lock );

                         DEBUGMSG( "DirectFB/directfb/keyboard_handler: "
                                   "Switching to VT %d...\n", num );

                         if (ioctl( core_vt->fd, VT_ACTIVATE, num ))
                              PERRORMSG( "DirectFB/directfb/keyboard_handler: "
                                         "VT_ACTIVATE for VT %d failed!\n", num );

                         //ioctl( core_vt->fd, VT_WAITACTIVE, num );

                         DEBUGMSG( "DirectFB/directfb/keyboard_handler: "
                                   "...hopefully switched to VT %d.\n", num );

                         return RS_DROP;
                    }
                    break;

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
          ERRORMSG( "DirectFBSetOption: DirectFBInit has to be "
                    "called before DirectFBSetOption!\n" );
          return DFB_INIT;
     }

     if (idirectfb_singleton) {
          ERRORMSG( "DirectFBSetOption: DirectFBSetOption has to be "
                    "called before DirectFBCreate!\n" );
          return DFB_INIT;
     }

     if (!name)
          return DFB_INVARG;

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
          /*  don't use ERRORMSG() here, it uses dfb_config  */
          fprintf( stderr,
                   "(!) DirectFBCreate: DirectFBInit has to be "
                   "called before DirectFBCreate!\n" );
          return DFB_INIT;
     }

     if (!interface)
          return DFB_INVARG;

     if (idirectfb_singleton) {
          idirectfb_singleton->AddRef( idirectfb_singleton );
          *interface = idirectfb_singleton;
          return DFB_OK;
     }

     if (!dfb_config->quiet && !dfb_config->no_banner) {
          printf( "\n" );
          printf( "       ----------------------- DirectFB v%d.%d.%d ---------------------\n",
                  DIRECTFB_MAJOR_VERSION, DIRECTFB_MINOR_VERSION, DIRECTFB_MICRO_VERSION );
          printf( "              (c)2000-2001  convergence integrated media GmbH  \n" );
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
          DFBFREE( idirectfb_singleton );
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

               return DFB_INIT;
          }

          ret = provider->RenderTo( provider, image );
          if (ret) {
               DirectFBError( "Failed loading background image", ret );

               image->Release( image );
               DFBFREE( image );
               provider->Release( provider );

               return DFB_INIT;
          }

          provider->Release( provider );

          image_data = (IDirectFBSurface_data*) image->priv;

          layers->bg.image = image_data->surface;
     }

     windowstack_repaint_all( layers->windowstack );

     return DFB_OK;
}

void DirectFBError( const char *msg, DFBResult error )
{
     if (msg)
          fprintf( stderr, "(#) DirectFBError [%s]: %s\n", msg,
                   DirectFBErrorString( error ) );
     else
          fprintf( stderr, "(#) DirectFBError: %s\n",
                   DirectFBErrorString( error ) );
}

const char *DirectFBErrorString( DFBResult error )
{
     switch (error) {
          case DFB_OK:
               return "Everything OK!";
          case DFB_FAILURE:
               return "General failure!";
          case DFB_INIT:
               return "General initialization failure!";
          case DFB_BUG:
               return "Internal bug!";
          case DFB_DEAD:
               return "Interface is dead!";
          case DFB_UNSUPPORTED:
               return "Not supported!";
          case DFB_UNIMPLEMENTED:
               return "Unimplemented!";
          case DFB_ACCESSDENIED:
               return "Access denied!";
          case DFB_INVARG:
               return "Invalid argument(s)!";
          case DFB_NOSYSTEMMEMORY:
               return "Out of system memory!";
          case DFB_NOVIDEOMEMORY:
               return "Out of video memory!";
          case DFB_LOCKED:
               return "Resource (already) locked!";
          case DFB_BUFFEREMPTY:
               return "Buffer is empty!";
          case DFB_FILENOTFOUND:
               return "File not found!";
          case DFB_IO:
               return "General I/O failure!";
          case DFB_NOIMPL:
               return "Interface implementation not available!";
          case DFB_MISSINGFONT:
               return "No font has been set!";
          case DFB_TIMEOUT:
               return "Operation timed out!";
          case DFB_MISSINGIMAGE:
               return "No image has been set!";
          case DFB_BUSY:
               return "Resource in use (busy)!";
          case DFB_THIZNULL:
               return "'thiz' pointer is NULL!";
          case DFB_IDNOTFOUND:
               return "ID not found!";
          case DFB_INVAREA:
               return "Invalid area specified or detected!";
          case DFB_DESTROYED:
               return "Object has been destroyed!";
     }

     return "<UNKNOWN ERROR CODE>!";
}

void DirectFBErrorFatal( const char *msg, DFBResult error )
{
     DirectFBError( msg, error );

     /* Deinit all stuff here. */
     core_deinit();     /* for now, this dirty thing should work */

     exit( error );
}

