/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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

#include <core/coretypes.h>
#include <core/state.h>


/*
 * private data struct of IDirectFBSurface
 */
typedef struct {
     DirectLink             link;

     int                    ref;             /* reference counter */

     DFBSurfaceCapabilities caps;            /* capabilities */

     struct {
         DFBRectangle       wanted;          /* passed to GetSubSurface */
         DFBRectangle       granted;         /* clipped by parent on creation */
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

     int                    locked;          /* which buffer is locked? */
     CoreSurface            *surface;        /* buffer to show */
     IDirectFBFont          *font;           /* font to use */
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

     IDirectFBSurface      *parent;
     DirectLink            *children_data;
     pthread_mutex_t        children_lock;
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
                                      CoreDFB                *core );

/*
 * destroys surface(s) and frees private data
 */
void IDirectFBSurface_Destruct( IDirectFBSurface *thiz );

/*
 * internal
 */
void IDirectFBSurface_StopAll( IDirectFBSurface_data *data );

#endif
