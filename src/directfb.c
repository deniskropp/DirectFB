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
#include <signal.h>

#include <directfb.h>

#include <misc/conf.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/input.h>
#include <core/layers.h>
#include <core/gfxcard.h>

#include <directfb_internals.h>
#include <directfb_version.h>

#include <display/idirectfbsurface.h>

#include "idirectfb.h"

static IDirectFB *idirectfb_singleton = NULL;

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

void keyboard_handler( DFBInputEvent *evt, void *ctx )
{
     if (evt->type == DIET_KEYPRESS &&
         evt->keycode == DIKC_BACKSPACE &&
         (evt->modifiers & (DIMK_CTRL|DIMK_ALT)) == (DIMK_CTRL|DIMK_ALT))
          kill( getpid(), SIGINT );
}

DFBResult DirectFBInit( int *argc, char **argv[] )
{
     DFBResult ret;

     ret = config_init( argc, argv );
     if (ret)
          return ret;
     
     if (!config->quiet && !config->no_banner) {
          printf( "\n" );
          printf( "       ----------------------- DirectFB v%d.%d.%d ---------------------\n",
                  DIRECTFB_MAJOR_VERSION, DIRECTFB_MINOR_VERSION, DIRECTFB_MICRO_VERSION );
          printf( "                (c)2000  convergence integrated media GmbH  \n" );
          printf( "        -----------------------------------------------------------\n" );
          printf( "\n" );
     }

     return DFB_OK;
}

/*
 * Programs have to call this to get the super interface
 * which is needed to access other functions
 */
DFBResult DirectFBCreate( IDirectFB **interface )
{
     DFBResult ret;

     if (config == NULL) {
          ERRORMSG( "DirectFB/DirectFBCreate: DirectFBInit has to be called "
                    "before DirectFBCreate!\n" );
          return DFB_INIT;
     }

     if (idirectfb_singleton) {
          idirectfb_singleton->AddRef( idirectfb_singleton );
          *interface = idirectfb_singleton;
          return DFB_OK;
     }

     ret = core_init();
     if (ret)
          return ret;

     {
          InputDevice *d = inputdevices;

          while (d) {
               if (d->id == DIDID_KEYBOARD)
                    input_add_listener( d, keyboard_handler, NULL );

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
     if (config->buffer_mode == -1) {
          if (card->caps.accel & DFXL_BLIT) {
               ret = layers->SetBufferMode( layers, DLBM_BACKVIDEO );
          }      
          else {
               ret = layers->SetBufferMode( layers, DLBM_BACKSYSTEM );
          }
     }
     else {
          ret = layers->SetBufferMode( layers, config->buffer_mode );
     }

     if (ret) {
          ERRORMSG( "DirectFB/DirectFBCreate: "
                    "Setting primary layer buffer mode failed! "
                    "-> No support for virtual resolutions?\n" );
     }
     
     /* set desktop background */
     layers->bg.mode = config->layer_bg_mode;
     layers->bg.color = config->layer_bg_color;

     if (config->layer_bg_mode == DLBM_IMAGE) {
          DFBSurfaceDescription   desc;
          IDirectFBImageProvider *provider;
          IDirectFBSurface       *image;
          IDirectFBSurface_data  *image_data;

          ret = (*interface)->CreateImageProvider( *interface, config->layer_bg_filename, &provider );
          if (ret) {
               DirectFBError( "Failed loading background image", ret );
               return DFB_INIT;
          }
          
          desc.flags = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_BPP;
          desc.width = layers->width;
          desc.height = layers->height;
          desc.bpp = BYTES_PER_PIXEL(layers->surface->format)*8;
          
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
          default:
               fprintf( stderr, "UNKNOWN ERROR CODE!\n" );
               break;
     }
}

void DirectFBErrorFatal( const char *msg, DFBResult error )
{
     DirectFBError( msg, error );

     /* Call applications AtExit here. */

     /* Deinit all stuff here. */
     core_deinit();     /* for now, this dirty thing should work */

     exit( error );
}

