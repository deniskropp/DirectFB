/*
   (c) Copyright 2001-2010  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

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

#ifndef __IDIRECTFB_H__
#define __IDIRECTFB_H__

#include <directfb.h>

#include <fusion/reactor.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

/*
 * private data struct of IDirectFB
 */
typedef struct {
     int                         ref;      /* reference counter */
     CoreDFB                    *core;

     DFBCooperativeLevel         level;    /* current cooperative level */

     CoreLayer                  *layer;    /* primary display layer */
     CoreLayerContext           *context;  /* shared context of primary layer */
     CoreWindowStack            *stack;    /* window stack of primary layer */

     struct {
          int                    width;    /* IDirectFB stores window width    */
          int                    height;   /* and height and the pixel depth   */
          DFBSurfacePixelFormat  format;   /* from SetVideoMode() parameters.  */
          DFBSurfaceColorSpace   colorspace; /* from SetVideoMode() parameters.  */

          CoreWindow            *window;   /* implicitly created window */
          Reaction               reaction; /* for the focus listener */
          bool                   focused;  /* primary's window has the focus */

          CoreLayerContext      *context;  /* context for fullscreen primary */
          DFBWindowOptions       window_options;
     } primary;                            /* Used for DFSCL_NORMAL's primary. */

     bool                        app_focus;

     struct {
          CoreLayer             *layer;
          CoreLayerContext      *context;
          CoreLayerRegion       *region;
          CoreSurface           *surface;
          CorePalette           *palette;
     } layers[MAX_LAYERS];

     bool                        init_done;
     DirectMutex                 init_lock;
     DirectWaitQueue             init_wq;
} IDirectFB_data;

/*
 * IDirectFB constructor/destructor
 */
DFBResult IDirectFB_Construct  ( IDirectFB  *thiz,
                                 CoreDFB    *core );

void      IDirectFB_Destruct   ( IDirectFB  *thiz );

DFBResult IDirectFB_SetAppFocus( IDirectFB  *thiz,
                                 DFBBoolean  focused );

/*
 * Remove the event buffer element from the internally managed linked list of
 * connections between event buffers created by
 * IDirectFB::CreateInputEventBuffer and input devices that are hot-plugged in.
 */
void      containers_remove_input_eventbuffer( IDirectFBEventBuffer *thiz );

DFBResult IDirectFB_InitLayers( IDirectFB *thiz );


DFBResult IDirectFB_WaitInitialised( IDirectFB *thiz );

extern IDirectFB *idirectfb_singleton;

#endif
