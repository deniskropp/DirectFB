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

#include <config.h>

#include <pthread.h>

#include <directfb.h>

#include <direct/util.h>

#include <core/coretypes.h>

#include <core/state.h>
#include <core/gfxcard.h>

#include <gfx/util.h>

#include <misc/util.h>


static bool      copy_state_inited;
static CardState copy_state;

static bool      btf_state_inited;
static CardState btf_state;

#if FIXME_SC_3
static bool      cd_state_inited;
static CardState cd_state;
#endif

static pthread_mutex_t copy_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t btf_lock  = PTHREAD_MUTEX_INITIALIZER;
#if FIXME_SC_3
static pthread_mutex_t cd_lock   = PTHREAD_MUTEX_INITIALIZER;
#endif


void
dfb_gfx_copy( CoreSurface *source, CoreSurface *destination, const DFBRectangle *rect )
{
     dfb_gfx_copy_to( source, destination, rect, rect ? rect->x : 0, rect ? rect->y : 0, false );
}

void
dfb_gfx_copy_to( CoreSurface *source, CoreSurface *destination, const DFBRectangle *rect, int x, int y, bool from_back )
{
     DFBRectangle sourcerect = { 0, 0, source->config.size.w, source->config.size.h };

     pthread_mutex_lock( &copy_lock );

     if (!copy_state_inited) {
          dfb_state_init( &copy_state, NULL );
          copy_state_inited = true;
     }

     copy_state.modified   |= SMF_CLIP | SMF_SOURCE | SMF_DESTINATION;

     copy_state.clip.x2     = destination->config.size.w - 1;
     copy_state.clip.y2     = destination->config.size.h - 1;
     copy_state.source      = source;
     copy_state.destination = destination;
     copy_state.from        = from_back ? CSBR_BACK : CSBR_FRONT;
     copy_state.to          = CSBR_BACK;

     if (rect) {
          if (dfb_rectangle_intersect( &sourcerect, rect ))
               dfb_gfxcard_blit( &sourcerect,
                                 x + sourcerect.x - rect->x,
                                 y + sourcerect.y - rect->y, &copy_state );
     }
     else
          dfb_gfxcard_blit( &sourcerect, x, y, &copy_state );

     /* Signal end of sequence. */
     dfb_state_stop_drawing( &copy_state );

     pthread_mutex_unlock( &copy_lock );
}

void
dfb_gfx_stretch_to( CoreSurface *source, CoreSurface *destination,
                    const DFBRectangle *srect, const DFBRectangle *drect, bool from_back )
{
     DFBRectangle sourcerect = { 0, 0, source->config.size.w, source->config.size.h };
     DFBRectangle destrect =   { 0, 0, destination->config.size.w, destination->config.size.h };

     if (srect) {
          if (!dfb_rectangle_intersect( &sourcerect, srect ))
               return;
     }

     if (drect) {
          if (!dfb_rectangle_intersect( &destrect, drect ))
               return;
     }

     pthread_mutex_lock( &copy_lock );

     if (!copy_state_inited) {
          dfb_state_init( &copy_state, NULL );
          copy_state_inited = true;
     }

     copy_state.modified   |= SMF_CLIP | SMF_SOURCE | SMF_DESTINATION;

     copy_state.clip.x2     = destination->config.size.w - 1;
     copy_state.clip.y2     = destination->config.size.h - 1;
     copy_state.source      = source;
     copy_state.destination = destination;
     copy_state.from        = from_back ? CSBR_BACK : CSBR_FRONT;

     dfb_gfxcard_stretchblit( &sourcerect, &destrect, &copy_state );

     /* Signal end of sequence. */
     dfb_state_stop_drawing( &copy_state );

     pthread_mutex_unlock( &copy_lock );
}

void
dfb_gfx_copy_regions( CoreSurface           *source,
                      CoreSurfaceBufferRole  from,
                      CoreSurface           *destination,
                      CoreSurfaceBufferRole  to,
                      const DFBRegion       *regions,
                      unsigned int           num,
                      int                    x,
                      int                    y )
{
     unsigned int i, n = 0;
     DFBRectangle rect = { 0, 0, source->config.size.w, source->config.size.h };
     DFBRectangle rects[num];
     DFBPoint     points[num];

     for (i=0; i<num; i++) {
          DFB_REGION_ASSERT( &regions[i] );

          rects[n] = DFB_RECTANGLE_INIT_FROM_REGION( &regions[i] );

          if (dfb_rectangle_intersect( &rects[n], &rect )) {
               points[n].x = x + rects[n].x - rect.x;
               points[n].y = y + rects[n].y - rect.y;

               n++;
          }
     }

     if (n > 0) {
          pthread_mutex_lock( &copy_lock );

          if (!copy_state_inited) {
               dfb_state_init( &copy_state, NULL );
               copy_state_inited = true;
          }

          copy_state.modified   |= SMF_CLIP | SMF_SOURCE | SMF_DESTINATION;

          copy_state.clip.x2     = destination->config.size.w - 1;
          copy_state.clip.y2     = destination->config.size.h - 1;
          copy_state.source      = source;
          copy_state.destination = destination;
          copy_state.from        = from;
          copy_state.to          = to;

          dfb_gfxcard_batchblit( rects, points, n, &copy_state );
     
          /* Signal end of sequence. */
          dfb_state_stop_drawing( &copy_state );

          pthread_mutex_unlock( &copy_lock );
     }
}

static void
back_to_front_copy( CoreSurface *surface, const DFBRegion *region, DFBSurfaceBlittingFlags flags, int rotation)
{
     DFBRectangle rect;
     int          dx, dy;

     if (region) {
          rect.x = region->x1;
          rect.y = region->y1;
          rect.w = region->x2 - region->x1 + 1;
          rect.h = region->y2 - region->y1 + 1;
     }
     else {
          rect.x = 0;
          rect.y = 0;
          rect.w = surface->config.size.w;
          rect.h = surface->config.size.h;
     }

     dx = rect.x;
     dy = rect.y;

     pthread_mutex_lock( &btf_lock );

     if (!btf_state_inited) {
          dfb_state_init( &btf_state, NULL );

          btf_state.from = CSBR_BACK;
          btf_state.to   = CSBR_FRONT;

          btf_state_inited = true;
     }

     btf_state.modified     |= SMF_CLIP | SMF_SOURCE | SMF_DESTINATION;

     btf_state.clip.x2       = surface->config.size.w - 1;
     btf_state.clip.y2       = surface->config.size.h - 1;
     btf_state.source        = surface;
     btf_state.destination   = surface;


     if (rotation == 90) {
          dx = rect.y;
          dy = surface->config.size.w - rect.w - rect.x;

          flags |= DSBLIT_ROTATE90;
     }
     else if (rotation == 180) {
          dx = surface->config.size.w - rect.w - rect.x;
          dy = surface->config.size.h - rect.h - rect.y;

          flags |= DSBLIT_ROTATE180;
     }
     else if (rotation == 270) {
          dx = surface->config.size.h - rect.h - rect.y;
          dy = rect.x;

          flags |= DSBLIT_ROTATE270;
     }


     dfb_state_set_blitting_flags( &btf_state, flags );

     dfb_gfxcard_blit( &rect, dx, dy, &btf_state );

     /* Signal end of sequence. */
     dfb_state_stop_drawing( &btf_state );

     pthread_mutex_unlock( &btf_lock );
}

void
dfb_back_to_front_copy( CoreSurface *surface, const DFBRegion *region )
{
     back_to_front_copy( surface, region, DSBLIT_NOFX, 0);
}

void
dfb_back_to_front_copy_rotation( CoreSurface *surface, const DFBRegion *region, int rotation )
{
     back_to_front_copy( surface, region, DSBLIT_NOFX, rotation );
}

void
dfb_clear_depth( CoreSurface *surface, const DFBRegion *region )
{
#if FIXME_SC_3
     SurfaceBuffer *tmp;
     DFBRectangle   rect = { 0, 0, surface->config.size.w - 1, surface->config.size.h - 1 };

     if (region && !dfb_rectangle_intersect_by_region( &rect, region ))
          return;

     pthread_mutex_lock( &cd_lock );

     if (!cd_state_inited) {
          dfb_state_init( &cd_state, NULL );

          cd_state.color.r = 0xff;
          cd_state.color.g = 0xff;
          cd_state.color.b = 0xff;

          cd_state_inited = true;
     }

     cd_state.modified   |= SMF_CLIP | SMF_DESTINATION;

     cd_state.clip.x2     = surface->config.size.w - 1;
     cd_state.clip.y2     = surface->config.size.h - 1;
     cd_state.destination = surface;

     dfb_surfacemanager_lock( surface->manager );

     tmp = surface->back_buffer;
     surface->back_buffer = surface->depth_buffer;

     dfb_gfxcard_fillrectangles( &rect, 1, &cd_state );

     surface->back_buffer = tmp;

     dfb_surfacemanager_unlock( surface->manager );

     /* Signal end of sequence. */
     dfb_state_stop_drawing( &cd_state );

     pthread_mutex_unlock( &cd_lock );
#endif
}


void dfb_sort_triangle( DFBTriangle *tri )
{
     int temp;

     if (tri->y1 > tri->y2) {
          temp = tri->x1;
          tri->x1 = tri->x2;
          tri->x2 = temp;

          temp = tri->y1;
          tri->y1 = tri->y2;
          tri->y2 = temp;
     }

     if (tri->y2 > tri->y3) {
          temp = tri->x2;
          tri->x2 = tri->x3;
          tri->x3 = temp;

          temp = tri->y2;
          tri->y2 = tri->y3;
          tri->y3 = temp;
     }

     if (tri->y1 > tri->y2) {
          temp = tri->x1;
          tri->x1 = tri->x2;
          tri->x2 = temp;

          temp = tri->y1;
          tri->y1 = tri->y2;
          tri->y2 = temp;
     }
}

