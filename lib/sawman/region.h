/*
   (c) Copyright 2001-2008  The world wide DirectFB Open Source Community (directfb.org)
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
/***********************************************************

Copyright 1987, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/
/*
 * Copyright © 1998, 2004 Keith Packard
 * Copyright   2007 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __DFB__MISC__REGION_H__
#define __DFB__MISC__REGION_H__

#include <directfb_util.h>

#include <fusion/types.h>

D_DEBUG_DOMAIN( Misc_Region, "Misc/Region", "Misc Region" );

typedef enum {
     MISC_REGION_OUT,
     MISC_REGION_IN,
     MISC_REGION_PART
} misc_region_overlap_t;

/*
 * regions
 */

typedef DFBBox                   misc_box_t;
typedef DFBRectangle             misc_rectangle_t;

typedef struct misc_region       misc_region_t;
typedef struct misc_region_data  misc_region_data_t;

struct misc_region_data {
     long                size;
     long                numRects;
/*  misc_box_t        rects[size];   in memory but not explicitly declared */
};

struct misc_region {
     int                  magic;

     misc_box_t           extents;

     FusionSHMPoolShared *shmpool;
     misc_region_data_t  *data;
};

/**********************************************************************************************************************/

#if D_DEBUG_ENABLED


#define MISC_REGION_ASSERT( reg )                                                                                  \
     do {                                                                                                          \
          D_MAGIC_ASSERT( reg, misc_region_t );                                                                    \
        /*  DFB_BOX_ASSERT( &(reg)->extents );    */                                                                   \
          if ((reg)->data != NULL) {                                                                               \
            /*   long i;     */                                                                                        \
               D_ASSERT( (reg)->data->size >= (reg)->data->numRects );                                             \
               D_ASSERT( (reg)->data->numRects >= 0 );                                                             \
      /*         for (i=0; i<(reg)->data->numRects; i++)                                                             \
                    DFB_BOX_ASSERT( &((DFBBox*)((reg)->data + 1))[i] );         */                                   \
          }                                                                                                        \
     } while (0)

#define MISC_REGION_LOG( Domain, _LEVEL, reg, info )                                                               \
     do {                                                                                                          \
          D_DEBUG_AT( Domain, "  -> %-10s| %4d,%4d-%4dx%4d (%2ld/%2ld) empty: %d\n", info,                         \
      /*    D_LOG( Domain, _LEVEL, "  -> %-10s| %4d,%4d-%4dx%4d (%2ld/%2ld) empty: %d\n", info,      */                \
                      DFB_RECTANGLE_VALS_FROM_BOX(&(reg)->extents),                                                \
                      (reg)->data ? (reg)->data->numRects : 0,                                                     \
                      (reg)->data ? (reg)->data->size     : 0,                                                     \
                      misc_region_is_empty(reg) );                                                                 \
                                                                                                                   \
          if ((reg)->data != NULL) {                                                                               \
               long i;                                                                                             \
                                                                                                                   \
               for (i=0; i<(reg)->data->numRects; i++)                                                             \
      /*              D_LOG( Domain, _LEVEL, "  -> [%2ld] %4d,%4d-%4dx%4d\n", i,       */                              \
                    D_DEBUG_AT( Domain, "  -> [%2ld] %4d,%4d-%4dx%4d\n", i,                                     \
                                DFB_RECTANGLE_VALS_FROM_BOX( &((DFBBox*)((reg)->data + 1))[i] ) );                 \
          }                                                                                                        \
     } while (0)

#define MISC_REGION_DEBUG_AT( Domain, reg, info )                                                                  \
     do {                                                                                                          \
          MISC_REGION_LOG( Domain, DEBUG, reg, info );                                                             \
     } while (0)


#else

#define MISC_REGION_ASSERT( reg )                                                                                  \
     do {                                                                                                          \
     } while (0)

#define MISC_REGION_LOG( Domain, _LEVEL, reg, info )                                                               \
     do {                                                                                                          \
     } while (0)

#define MISC_REGION_DEBUG_AT( Domain, reg, info )                                                                  \
     do {                                                                                                          \
     } while (0)
#endif

/**********************************************************************************************************************/

/* creation/destruction */
void                  misc_region_init              ( misc_region_t       *region,
                                                      FusionSHMPoolShared *shmpool );

void                  misc_region_init_rect         ( misc_region_t       *region,
                                                      FusionSHMPoolShared *shmpool,
                                                      int                  x,
                                                      int                  y,
                                                      unsigned int         width,
                                                      unsigned int         height );

bool                  misc_region_init_boxes        ( misc_region_t       *region,
                                                      FusionSHMPoolShared *shmpool,
                                                      const DFBBox        *boxes,
                                                      int                  count );

void                  misc_region_init_with_extents ( misc_region_t       *region,
                                                      FusionSHMPoolShared *shmpool,
                                                      misc_box_t          *extents );

void                  misc_region_deinit            ( misc_region_t       *region );

/**********************************************************************************************************************/

/* manipulation */
void                  misc_region_translate         ( misc_region_t       *region,
                                                      int                  x,
                                                      int                  y );

bool                  misc_region_copy              ( misc_region_t       *dest,
                                                      misc_region_t       *source );

bool                  misc_region_intersect         ( misc_region_t       *newReg,
                                                      misc_region_t       *reg1,
                                                      misc_region_t       *reg2 );

bool                  misc_region_union             ( misc_region_t       *newReg,
                                                      misc_region_t       *reg1,
                                                      misc_region_t       *reg2 );

bool                  misc_region_union_rect        ( misc_region_t       *dest,
                                                      misc_region_t       *source,
                                                      int                  x,
                                                      int                  y,
                                                      unsigned int         width,
                                                      unsigned int         height );

bool                  misc_region_subtract          ( misc_region_t       *regD,
                                                      misc_region_t       *regM,
                                                      const misc_region_t *regS );

bool                  misc_region_inverse           ( misc_region_t       *newReg,
                                                      misc_region_t       *reg1,
                                                      misc_box_t          *invRect );

bool                  misc_region_contains_point    ( misc_region_t       *region,
                                                      int                  x,
                                                      int                  y,
                                                      misc_box_t          *box );

misc_region_overlap_t misc_region_contains_rectangle( misc_region_t       *region,
                                                      misc_box_t          *prect );

bool                  misc_region_is_empty          ( misc_region_t       *region );

bool                  misc_region_not_empty         ( misc_region_t       *region );

misc_box_t *          misc_region_extents           ( misc_region_t       *region );

int                   misc_region_n_rects           ( misc_region_t       *region );

int                   misc_region_n_pixels          ( misc_region_t       *region );

misc_box_t *          misc_region_boxes             ( misc_region_t       *region,
                                                      int                 *n_rects );

bool                  misc_region_equal             ( misc_region_t       *region1,
                                                      misc_region_t       *region2 );

bool                  misc_region_selfcheck         ( misc_region_t       *region );

void                  misc_region_reset             ( misc_region_t       *region,
                                                      misc_box_t          *box );

void                  misc_region_collapse          ( misc_region_t       *region );

/**********************************************************************************************************************/

static inline void
misc_region_boxes_to_rects( DFBRectangle     *rects,
                            const misc_box_t *boxes,
                            int               num )
{
     int i;

     for (i=0; i<num; i++) {
          const misc_box_t *box = &boxes[i];

          rects[i].x = box->x1;
          rects[i].y = box->y1;
          rects[i].w = box->x2 - box->x1;
          rects[i].h = box->y2 - box->y1;
     }
}

static inline void
misc_region_rects_to_boxes( misc_box_t         *boxes,
                            const DFBRectangle *rects,
                            int                 num )
{
     int i;

     for (i=0; i<num; i++) {
          const DFBRectangle *rect = &rects[i];

          boxes[i].x1 = rect->x;
          boxes[i].y1 = rect->y;
          boxes[i].x2 = rect->x + rect->w;
          boxes[i].y2 = rect->y + rect->h;
     }
}

static inline void
misc_region_regions_to_boxes( misc_box_t      *boxes,
                              const DFBRegion *regions,
                              int              num )
{
     int i;

     for (i=0; i<num; i++) {
          const DFBRegion *region = &regions[i];

          boxes[i].x1 = region->x1;
          boxes[i].y1 = region->y1;
          boxes[i].x2 = region->x2 + 1;
          boxes[i].y2 = region->y2 + 1;
     }
}

static inline bool
misc_region_init_rects( misc_region_t       *region,
                        FusionSHMPoolShared *shmpool,
                        const DFBRectangle  *rects,
                        int                  num )
{
     misc_box_t boxes[num];

     D_DEBUG_AT( Misc_Region, "%s( %p, %p, %p [%d] )\n", __FUNCTION__, region, shmpool, rects, num );
     DFB_RECTANGLES_DEBUG_AT( Misc_Region, rects, num );

     misc_region_rects_to_boxes( boxes, rects, num );

     return misc_region_init_boxes( region, shmpool, boxes, num );
}

static inline bool
misc_region_init_regions( misc_region_t       *region,
                          FusionSHMPoolShared *shmpool,
                          const DFBRegion     *regions,
                          int                  num )
{
     misc_box_t boxes[num];

     D_DEBUG_AT( Misc_Region, "%s( %p, %p, %p [%d] )\n", __FUNCTION__, region, shmpool, regions, num );
//     DFB_REGIONS_DEBUG_AT( Misc_Region, regions, num );

     misc_region_regions_to_boxes( boxes, regions, num );

     return misc_region_init_boxes( region, shmpool, boxes, num );
}

static inline bool
misc_region_init_updates( misc_region_t       *region,
                          FusionSHMPoolShared *shmpool,
                          const DFBUpdates    *updates )
{
     misc_box_t boxes[updates->num_regions];

     misc_region_regions_to_boxes( boxes, updates->regions, updates->num_regions );

     return misc_region_init_boxes( region, shmpool, boxes, updates->num_regions );
}

static inline bool
misc_region_union_rects( misc_region_t      *dest,
                         misc_region_t      *region,
                         const DFBRectangle *rects,
                         int                 num )
{
     bool          ret;
     misc_region_t region2;

     MISC_REGION_ASSERT( dest );
     MISC_REGION_ASSERT( region );

     if (!misc_region_init_rects( &region2, NULL, rects, num ))
          return false;

     ret = misc_region_union( dest, region, &region2 );

     misc_region_deinit( &region2 );

     return ret;
}

static inline bool
misc_region_add_updates( misc_region_t    *region,
                         const DFBUpdates *updates )
{
     int          num;
     DFBRectangle rects[updates->num_regions];

     D_DEBUG_AT( Misc_Region, "%s( %p, %p )\n", __FUNCTION__, region, updates );
//     DFB_UPDATES_DEBUG_AT( Misc_Region, updates );
     MISC_REGION_DEBUG_AT( Misc_Region, region, "updates" );
     MISC_REGION_ASSERT( region );

     dfb_updates_get_rectangles( (void*) updates, rects, &num );

     return misc_region_union_rects( region, region, rects, num );
}

#endif

