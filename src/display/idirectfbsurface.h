/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
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

#ifndef __IDIRECTFBSURFACE_H__
#define __IDIRECTFBSURFACE_H__

#include <directfb.h>

#include <direct/types.h>

#include <fusion/reactor.h>

#include <core/core.h>
#include <core/state.h>

#include <core/CoreGraphicsStateClient.h>


/*
 * private data struct of IDirectFBSurface
 */
typedef struct {
     DirectLink             link;

     int                    ref;             /* reference counter */

     DFBSurfaceCapabilities caps;            /* capabilities */

     struct {
         /* 'wanted' is passed to GetSubSurface(), it doesn't matter if it's
            too large or has negative starting coordinates as long as it
            intersects with the 'granted' rectangle of the parent.
            'wanted' should be seen as the origin for operations on that
            surface. Non sub surfaces have a 'wanted' rectangle
            of '{ 0, 0, width, height }'. 'wanted' is calculated just once
            during surface creation. */
         DFBRectangle       wanted;
         /* 'granted' is the intersection of the 'wanted' rectangle and the
            'granted' one of the parent. If they do not intersect DFB_INVAREA
            is returned. For non sub surfaces it's the same as the 'wanted'
            rectangle, because it's the rectangle describing the whole
            surface. 'granted' is calculated just once during surface
            creation */
         DFBRectangle       granted;
         /* 'current' is the intersection of the 'granted' rectangle and the
            surface extents. SetClip() and many other functions are limited
            by this.
            This way sub surface area information is preserved during surface
            resizing, e.g. when resizing a window. Calling SetClip() with NULL
            causes the clipping region to exactly cover the 'current'
            rectangle, also the flag 'clip_set' is cleared causing the
            clipping region to be set to the new 'current' after resizing. If
            SetClip() is called with a clipping region specified, an
            intersection is done with the 'wanted' rectangle that is then
            stored in 'clip_wanted' and 'clip_set' is set. However, if there
            is no intersection 'DFB_INVARG' is returned, otherwise another
            intersection is made with the 'current' rectangle and gets applied
            to the surface's state.
            Each resize, after the 'current' rectangle is updated, the
            clipping region is set to NULL or 'clip_wanted' depending on
            'clip_set'. This way even clipping regions are restored or
            extended automatically. It's now possible to create a fullscreen
            primary and call SetVideoMode() with different resolutions or
            pixelformats several times without the need for updating the
            primary surface by recreating it */
         DFBRectangle       current;         /* currently available area */
         DFBInsets          insets;          /* actually set by the window manager */
     } area;
     
     bool                   limit_set;       /* greanted rectangle set?
                                                (GetSubSurface called with rect != NULL) */

     bool                   clip_set;        /* fixed clip set? (SetClip called
                                                with clip != NULL) */
     DFBRegion              clip_wanted;     /* last region passed to SetClip
                                                intersected by wanted area,
                                                only valid if clip_set != 0 */

     CoreSurface           *surface;         /* buffer to show */
     bool                   locked;          /* which buffer is locked? */
     CoreSurfaceBufferLock  lock;

     IDirectFBFont         *font;            /* font to use */
     CardState              state;           /* render state to use */
     DFBTextEncodingID      encoding;        /* text encoding */

     struct {
          u8                r;               /* red component */
          u8                g;               /* green component */
          u8                b;               /* blue component */
          u32               value;           /* r/g/b in surface's format */
     } src_key;                              /* src key for blitting from
                                                this surface */

     struct {
          u8                r;               /* red component */
          u8                g;               /* green component */
          u8                b;               /* blue component */
          u32               value;           /* r/g/b in surface's format */
     } dst_key;                              /* dst key for blitting to
                                                this surface */

     Reaction               reaction;

     CoreDFB               *core;
     IDirectFB             *idirectfb;

     IDirectFBSurface      *parent;
     DirectLink            *children_data;
     pthread_mutex_t        children_lock;

     CoreGraphicsStateClient  state_client;

     CoreMemoryPermission    *memory_permissions[3];
     unsigned int             memory_permissions_count;

     DirectWaitQueue          back_buffer_wq;
     DirectMutex              back_buffer_lock;

     unsigned int             frame_ack;

     CoreSurfaceClient       *surface_client;
     unsigned int             surface_client_flip_count;
     DirectMutex              surface_client_lock;

     DFBSurfaceStereoEye      src_eye;
} IDirectFBSurface_data;

/*
 * initializes interface struct and private data
 */
DFBResult IDirectFBSurface_Construct( IDirectFBSurface *thiz,
                                      IDirectFBSurface *parent,
                                      DFBRectangle *req_rect,
                                      DFBRectangle *clip_rect,
                                      DFBInsets    *insets,
                                      CoreSurface  *surface,
                                      DFBSurfaceCapabilities  caps,
                                      CoreDFB                *core,
                                      IDirectFB              *idirectfb );

/*
 * destroys surface(s) and frees private data
 */
void IDirectFBSurface_Destruct( IDirectFBSurface *thiz );

/*
 * internal
 */
void IDirectFBSurface_StopAll( IDirectFBSurface_data *data );

void IDirectFBSurface_WaitForBackBuffer( IDirectFBSurface_data *data );

#endif
