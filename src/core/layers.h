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

#ifndef __LAYERS_H__
#define __LAYERS_H__

#include <asm/types.h>
#include <pthread.h>

#include <core/fusion/lock.h>

#include <directfb.h>
#include <core/coretypes.h>

#define DFB_DISPLAY_LAYER_INFO_NAME_LENGTH   30

struct _DisplayLayerShared
{
     unsigned int   id;                 /* unique id, functions as an index,
                                           primary layer has a fixed id */
     unsigned int   caps;               /* capabilities such as pixelbased
                                           alphablending */

     char           description[DFB_DISPLAY_LAYER_INFO_NAME_LENGTH];
                                        /* description set by driver */

     int            enabled;            /* layers can be turned on and off */

     unsigned int   width;              /* width in pixels */
     unsigned int   height;             /* height in pixels */
     DFBDisplayLayerBufferMode buffermode;
                                        /* buffermode: single or double
                                           (with video or system backbuffer) */
     DFBDisplayLayerOptions    options; /* enable/disable certain features
                                           like pixelbased alphablending */
     __u8           opacity;            /* if enabled this value controls
                                           blending of the whole layer */

     /* these are normalized values for stretching layers in hardware */
     struct {
          float     x, y;               /* 0,0 for the primary layer */
          float     w, h;               /* 1,1 for the primary layer */
     } screen;

     /* stores information on handling the background on exposure */
     struct {
          DFBDisplayLayerBackgroundMode mode;
                                         /* background handling mode:
                                            don't care, solid color or image */

          DFBColor       color;          /* color for solid background mode */


          CoreSurface    *image;         /* surface for background image mode */
     } bg;

     DFBColorAdjustment adjustment;

     CoreWindowStack *windowstack;       /* every layer has its own
                                            windowstack as every layer has
                                            its own pixel buffer */

     CoreSurface     *surface;           /* surface of the layer */

     int              exclusive;         /* indicates exclusive access to
                                            this layer by an application */
     FusionSkirmish      lock;              /* locked when in exclusive access */

};

struct _DisplayLayer
{
     DisplayLayerShared *shared;

     void (*deinit)( DisplayLayer *layer );

     /*
      * internal layer driver API (alpha version)
      */

     DFBResult (*Enable)            ( DisplayLayer               *thiz );
     DFBResult (*Disable)           ( DisplayLayer               *thiz );
     DFBResult (*TestConfiguration) ( DisplayLayer               *thiz,
                                      DFBDisplayLayerConfig      *config,
                                      DFBDisplayLayerConfigFlags *failed );
     DFBResult (*SetConfiguration)  ( DisplayLayer               *thiz,
                                      DFBDisplayLayerConfig      *config );
     DFBResult (*SetOpacity)        ( DisplayLayer               *thiz,
                                      __u8                        opacity );
     DFBResult (*SetScreenLocation) ( DisplayLayer               *thiz,
                                      float                       x,
                                      float                       y,
                                      float                       width,
                                      float                       height );
     DFBResult (*SetColorKey)       ( DisplayLayer               *thiz,
                                      __u32                       key );
     DFBResult (*FlipBuffers)       ( DisplayLayer *thiz );
     DFBResult (*SetColorAdjustment)( DisplayLayer               *thiz,
                                      DFBColorAdjustment         *adj );

     DisplayLayer *next;
};

extern DisplayLayer *dfb_layers;

/*
 * initializes the lock skirmish, registers input event listeners for
 * keyboard and mouse devices (for cursor and windowing),
 * adds the layer to the layer list, if it's the first a cleanup function
 * is added to the cleanup stack that removes all layers
 */
void dfb_layers_add( DisplayLayer *layer );

DFBResult dfb_layers_initialize();
DFBResult dfb_layers_join();

DFBResult dfb_layers_shutdown();
DFBResult dfb_layers_leave();

#ifdef FUSION_FAKE
DFBResult dfb_layers_suspend();
DFBResult dfb_layers_resume();
#endif

/*
 * lock/unlock layer for exclusive access
 */
DFBResult dfb_layer_lock( DisplayLayer *layer );
DFBResult dfb_layer_unlock( DisplayLayer *layer );

DFBResult dfb_layer_enable( DisplayLayer *layer );

CoreSurface *dfb_layer_surface( DisplayLayer *layer );

/*
 * utility functions
 */
DFBResult dfb_layer_cursor_enable( DisplayLayer *layer, int enable );
DFBResult dfb_layer_cursor_set_shape( DisplayLayer *layer, CoreSurface *shape,
                                      int hot_x, int hot_y );
DFBResult dfb_layer_cursor_set_opacity( DisplayLayer *layer, __u8 opacity );
DFBResult dfb_layer_cursor_warp( DisplayLayer *layer, int x, int y );


#endif
