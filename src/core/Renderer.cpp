/*
   (c) Copyright 2001-2012  The world wide DirectFB Open Source Community (directfb.org)
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

//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include "Renderer.h"

extern "C" {
#include <directfb.h>

#include <direct/debug.h>
#include <direct/messages.h>

#include <core/surface_allocation.h>
#include <core/surface_pool.h>

#include <gfx/convert.h>
#include <gfx/clip.h>
#include <gfx/util.h>
}

#include <Util.h>


D_DEBUG_DOMAIN( DirectFB_Renderer, "DirectFB/Renderer", "DirectFB Renderer" );

/*********************************************************************************************************************/


typedef struct {
     int                 magic;

     DirectFB::Renderer *last_renderer;
} RendererTLS;

static DirectTLS renderer_tls_key;

static void
renderer_tls_destroy( void *arg )
{
     RendererTLS *renderer_tls = (RendererTLS*) arg;

     D_MAGIC_ASSERT( renderer_tls, RendererTLS );

     D_MAGIC_CLEAR( renderer_tls );

     D_FREE( renderer_tls );
}


void
Renderer_TLS__init( void )
{
     direct_tls_register( &renderer_tls_key, renderer_tls_destroy );
}

void
Renderer_TLS__deinit( void )
{
     direct_tls_unregister( &renderer_tls_key );
}

static RendererTLS *
Renderer_GetTLS( void )
{
     RendererTLS *renderer_tls;

     renderer_tls = (RendererTLS*) direct_tls_get( renderer_tls_key );
     if (!renderer_tls) {
          renderer_tls = (RendererTLS*) D_CALLOC( 1, sizeof(RendererTLS) );
          if (!renderer_tls) {
               D_OOM();
               return NULL;
          }

          D_MAGIC_SET( renderer_tls, RendererTLS );

          direct_tls_set( renderer_tls_key, renderer_tls );
     }

     D_MAGIC_ASSERT( renderer_tls, RendererTLS );

     return renderer_tls;
}



namespace DirectFB {


Renderer::Renderer( CardState *state )
     :
     state( state ),
     engine( NULL ),
     setup( NULL ),
     operations( 0 )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

}

Renderer::~Renderer()
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     Flush();
}


void
Renderer::Flush()
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     if (engine) {
          unbindEngine();

          RendererTLS *tls = Renderer_GetTLS();

          if (tls->last_renderer == this)
               tls->last_renderer = NULL;
     }
}


DFBResult
Renderer::enterLock( CoreSurfaceBufferLock  *lock,
                     CoreSurfaceAllocation  *allocation,
                     CoreSurfaceAccessFlags  flags )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s( flags 0x%02x )\n", __FUNCTION__, flags );

     /*
        FIXME: move to engine / task
     */
     dfb_surface_buffer_lock_init( lock, setup->tasks[0]->accessor, flags );
     dfb_surface_pool_lock( allocation->pool, allocation, lock );

     D_DEBUG_AT( DirectFB_Renderer, "  -> %p (%d)\n", lock->addr, lock->pitch );

     return DFB_OK;
}

DFBResult
Renderer::leaveLock( CoreSurfaceBufferLock *lock )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     /*
        FIXME: move to engine / task
     */
     if (lock->buffer) {
          dfb_surface_pool_unlock( lock->allocation->pool, lock->allocation, lock );
          dfb_surface_buffer_lock_deinit( lock );
     }

     return DFB_OK;
}

DFBResult
Renderer::updateLock( CoreSurfaceBufferLock  *lock,
                      CoreSurface            *surface,
                      CoreSurfaceBufferRole   role,
                      DFBSurfaceStereoEye     eye,
                      u32                     flips,
                      CoreSurfaceAccessFlags  flags )
{
     DFBResult              ret;
     CoreSurfaceBuffer     *buffer;
     CoreSurfaceAllocation *allocation;
     SurfaceAllocationMap::iterator it;

     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );


     // FIXME: don't need to unlock/lock every time in most cases, move to engine

     leaveLock( lock );


     SurfaceAllocationKey key( surface->object.id, role, eye, flips );

     it = allocations.find( key );
     if (it != allocations.end()) {
          allocation = (*it).second;

     }
     else {
          dfb_surface_lock( surface );

          // FIXME: move to helper class
          //

          buffer = dfb_surface_get_buffer3( surface, role, eye, flips );

          allocation = dfb_surface_buffer_find_allocation( buffer, setup->tasks[0]->accessor, flags, true );
          if (!allocation) {
               /* If no allocation exists, create one. */
               ret = dfb_surface_pools_allocate( buffer, setup->tasks[0]->accessor, flags, &allocation );
               if (ret) {
                    D_DERROR( ret, "DirectFB/Renderer: Buffer allocation failed!\n" );
                    dfb_surface_unlock( surface );
                    return ret;
               }
          }

          dfb_surface_allocation_update( allocation, flags );

          setup->tasks[0]->AddAccess( allocation, flags );

          dfb_surface_unlock( surface );

//          long long t1 = direct_clock_get_abs_millis();
          // FIXME: this is a temporary solution, slaves will be blocked via kernel module later
//          printf("count %d\n", allocation->task_count);
          unsigned int timeout = 5000;
          while (allocation->task_count > 5) {
               if (!--timeout) {
                    D_ERROR( "DirectFB/Renderer: timeout!\n" );
                    TaskManager::dumpTasks();
                    break;
               }
               //printf("blocked\n");
               usleep( 1000 );
          }
//          long long t2 = direct_clock_get_abs_millis();
//          D_INFO("blocked %lld\n", t2 - t1);


          allocations.insert( SurfaceAllocationMapPair( key, allocation ) );
     }


     enterLock( lock, allocation, flags );

     return DFB_OK;
}

void
Renderer::update( DFBAccelerationMask accel )
{
     CoreSurfaceAccessFlags access = CSAF_WRITE;

     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s( accel 0x%08x )\n", __FUNCTION__, accel );
     D_DEBUG_AT( DirectFB_Renderer, "  -> modified 0x%08x\n", state->modified );
     D_DEBUG_AT( DirectFB_Renderer, "  -> mod_hw   0x%08x\n", state->mod_hw );

     /* find locking flags */
     if (DFB_BLITTING_FUNCTION( accel )) {
          if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL |
                                      DSBLIT_BLEND_COLORALPHA   |
                                      DSBLIT_DST_COLORKEY))
               access = (CoreSurfaceAccessFlags)(access | CSAF_READ);
     }
     else if (state->drawingflags & (DSDRAW_BLEND | DSDRAW_DST_COLORKEY))
          access = (CoreSurfaceAccessFlags)(access | CSAF_READ);


     if (state_mod & SMF_DESTINATION) {
          D_ASSERT( state->destination != NULL );

          updateLock( &state->dst, state->destination, state->to, state->to_eye, state->destination->flips,
                      (CoreSurfaceAccessFlags)( CSAF_WRITE | CSAF_READ ) );

          state_mod = (StateModificationFlags)(state_mod & ~SMF_DESTINATION);
     }

     if (DFB_BLITTING_FUNCTION( accel )) {
          D_ASSERT( state->source != NULL );

          if (state_mod & SMF_SOURCE) {
               updateLock( &state->src, state->source, state->from, state->from_eye, state->source->flips, CSAF_READ );

               state_mod = (StateModificationFlags)(state_mod & ~SMF_SOURCE);
          }

          /* if using a mask... */
          if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) {
               D_ASSERT( state->source_mask != NULL );

               if (state_mod & SMF_SOURCE_MASK) {
                    updateLock( &state->src_mask, state->source_mask, state->from, state->from_eye, state->source_mask->flips, CSAF_READ );

                    state_mod = (StateModificationFlags)(state_mod & ~SMF_SOURCE_MASK);
               }
          }

          /* if using source2... */
          if (accel == DFXL_BLIT2) {
               D_ASSERT( state->source2 != NULL );

               if (state_mod & SMF_SOURCE2) {
                    updateLock( &state->src2, state->source2, state->from, state->from_eye, state->source2->flips, CSAF_READ );

                    state_mod = (StateModificationFlags)(state_mod & ~SMF_SOURCE2);
               }
          }
     }

     if (state_mod & SMF_CLIP) {
          D_ASSERT( setup->tiles <= 32 );

          setup->task_mask = 0;

          for (unsigned int i=0; i<setup->tiles; i++) {
               setup->clips_clipped[i].x1 = MAX( state->clip.x1, setup->clips[i].x1 );
               setup->clips_clipped[i].y1 = MAX( state->clip.y1, setup->clips[i].y1 );
               setup->clips_clipped[i].x2 = MIN( state->clip.x2, setup->clips[i].x2 );
               setup->clips_clipped[i].y2 = MIN( state->clip.y2, setup->clips[i].y2 );

               if (setup->clips_clipped[i].x1 <= setup->clips_clipped[i].x2 &&
                   setup->clips_clipped[i].y1 <= setup->clips_clipped[i].y2)
                    setup->task_mask |= (1 << i);
          }

          state_mod = (StateModificationFlags)(state_mod & ~SMF_CLIP);
     }

     if (state->mod_hw || !(state->set & accel)) {
          DFBRegion              clip     = state->clip;
          StateModificationFlags modified = state->mod_hw;

          /// loop, clip switch, task mask (total clip)

          for (unsigned int i=0; i<setup->tiles; i++) {
               state->clip = setup->clips_clipped[i];

               engine->SetState( setup->tasks[i], state, modified, accel );
          }

          state->clip = clip;
     }
}

void
Renderer::DrawRectangles( const DFBRectangle *rects,
                          unsigned int        num_rects )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     if (checkEngine( DFXL_DRAWRECTANGLE, num_rects )) {
          update( DFXL_DRAWRECTANGLE );

          // FIXME: clipping, transform

          /// loop
          for (unsigned int i=0; i<setup->tiles; i++) {
               if (!(setup->task_mask & (1 << i)))
                    continue;

               engine->DrawRectangles( setup->tasks[i], rects, num_rects );
          }
     }
}

void
Renderer::DrawLines( const DFBRegion *lines,
                     unsigned int     num_lines )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     if (checkEngine( DFXL_DRAWLINE, num_lines )) {
          update( DFXL_DRAWLINE );

          // FIXME: clipping, transform

          /// loop
          for (unsigned int i=0; i<setup->tiles; i++) {
               if (!(setup->task_mask & (1 << i)))
                    continue;

               engine->DrawLines( setup->tasks[i], lines, num_lines );
          }
     }
}

void
Renderer::FillRectangles( const DFBRectangle *rects,
                          unsigned int        num_rects )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     if (checkEngine( DFXL_FILLRECTANGLE, num_rects )) {
          update( DFXL_FILLRECTANGLE );

          // FIXME: transform, max rectangle size

          /// loop
          for (unsigned int i=0; i<setup->tiles; i++) {
               if (!(setup->task_mask & (1 << i)))
                    continue;

               if (engine->caps.clipping & DFXL_FILLRECTANGLE) {
                    engine->FillRectangles( setup->tasks[i], rects, num_rects );
               }
               else {
                    Util::TempArray<DFBRectangle> copied_rects( num_rects );
                    unsigned int                  copied_num = 0;

                    for (unsigned int n=0; n<num_rects; n++) {
                         copied_rects.array[copied_num] = rects[n];

                         if (dfb_clip_rectangle( &setup->clips_clipped[i], &copied_rects.array[copied_num] ))
                              copied_num++;
                    }

                    if (copied_num)
                         engine->FillRectangles( setup->tasks[i], copied_rects.array, copied_num );
               }
          }
     }
}



typedef struct {
   int xi;
   int xf;
   int mi;
   int mf;
   int _2dy;
} DDA;

#define SETUP_DDA(xs,ys,xe,ye,dda)         \
     do {                                  \
          int dx = (xe) - (xs);            \
          int dy = (ye) - (ys);            \
          dda.xi = (xs);                   \
          if (dy != 0) {                   \
               dda.mi = dx / dy;           \
               dda.mf = 2*(dx % dy);       \
               dda.xf = -dy;               \
               dda._2dy = 2 * dy;          \
               if (dda.mf < 0) {           \
                    dda.mf += 2 * ABS(dy); \
                    dda.mi--;              \
               }                           \
          }                                \
          else {                           \
               dda.mi = 0;                 \
               dda.mf = 0;                 \
               dda.xf = 0;                 \
               dda._2dy = 0;               \
          }                                \
     } while (0)


#define INC_DDA(dda)                       \
     do {                                  \
          dda.xi += dda.mi;                \
          dda.xf += dda.mf;                \
          if (dda.xf > 0) {                \
               dda.xi++;                   \
               dda.xf -= dda._2dy;         \
          }                                \
     } while (0)



/**
 *  render a triangle using two parallel DDA's
 */
static void
fill_tri( DFBTriangle *tri, CardState *state, DFBRectangle *ret_rects, unsigned int *ret_num )
{
     int y, yend;
     DDA dda1, dda2;
     int clip_x1 = state->clip.x1;
     int clip_x2 = state->clip.x2;

     D_MAGIC_ASSERT( state, CardState );

     dda1.xi = 0;
     dda2.xi = 0;

     *ret_num = 0;

     y = tri->y1;
     yend = tri->y3;

     if (yend > state->clip.y2)
          yend = state->clip.y2;

     SETUP_DDA(tri->x1, tri->y1, tri->x3, tri->y3, dda1);
     SETUP_DDA(tri->x1, tri->y1, tri->x2, tri->y2, dda2);

     while (y <= yend) {
          DFBRectangle rect;

          if (y == tri->y2) {
               if (tri->y2 == tri->y3)
                    return;
               SETUP_DDA(tri->x2, tri->y2, tri->x3, tri->y3, dda2);
          }

          rect.w = ABS(dda1.xi - dda2.xi);
          rect.x = MIN(dda1.xi, dda2.xi);

          if (clip_x2 < rect.x + rect.w)
               rect.w = clip_x2 - rect.x + 1;

          if (rect.w > 0) {
               if (clip_x1 > rect.x) {
                    rect.w -= (clip_x1 - rect.x);
                    rect.x = clip_x1;
               }
               rect.y = y;
               rect.h = 1;

               if (rect.w > 0 && rect.y >= state->clip.y1) {
                    ret_rects[(*ret_num)++] = rect;
               }
          }

          INC_DDA(dda1);
          INC_DDA(dda2);

          y++;
     }
}



void
Renderer::FillTriangles( const DFBTriangle *tris,
                         unsigned int       num_tris )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     if (0) {
          if (checkEngine( DFXL_FILLTRIANGLE, num_tris )) {
               update( DFXL_FILLTRIANGLE );
     
               // FIXME: clipping, transform
     
               /// loop
               for (unsigned int i=0; i<setup->tiles; i++) {
                    if (!(setup->task_mask & (1 << i)))
                         continue;

                    engine->FillTriangles( setup->tasks[i], tris, num_tris );
               }
          }
     }
     else {
          if (checkEngine( DFXL_FILLRECTANGLE, num_tris )) {
               update( DFXL_FILLRECTANGLE );

               // FIXME: clipping, transform

               for (unsigned int n=0; n<num_tris; n++) {
                    DFBTriangle tri = tris[n];

                    dfb_sort_triangle( &tri );

                    if (tri.y3 - tri.y1 > 0) {
                         DFBRectangle rects[tri.y3 - tri.y1 + 1];
                         unsigned int num_rects;

                         fill_tri( &tri, state, rects, &num_rects );

                         /// loop
                         for (unsigned int i=0; i<setup->tiles; i++) {
                              if (!(setup->task_mask & (1 << i)))
                                   continue;

                              engine->FillRectangles( setup->tasks[i], rects, num_rects );
                         }
                    }
               }
          }
     }
}

void
Renderer::FillTrapezoids( const DFBTrapezoid *traps,
                          unsigned int        num_traps )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     if (checkEngine( DFXL_FILLTRAPEZOID, num_traps )) {
          update( DFXL_FILLTRAPEZOID );

          // FIXME: clipping, transform

          /// loop
          for (unsigned int i=0; i<setup->tiles; i++) {
               if (!(setup->task_mask & (1 << i)))
                    continue;

               engine->FillTrapezoids( setup->tasks[i], traps, num_traps );
          }
     }
}

void
Renderer::FillSpans( int            y,
                     const DFBSpan *spans,
                     unsigned int   num_spans )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     if (checkEngine( DFXL_FILLSPAN, num_spans )) {
          update( DFXL_FILLSPAN );

          // FIXME: clipping, transform

          /// loop
          for (unsigned int i=0; i<setup->tiles; i++) {
               if (!(setup->task_mask & (1 << i)))
                    continue;

               engine->FillSpans( setup->tasks[i], y, spans, num_spans );
          }
     }
}

void
Renderer::Blit( const DFBRectangle     *rects,
                const DFBPoint         *points,
                u32                     num )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     if (checkEngine( DFXL_BLIT, num )) {
          update( DFXL_BLIT );

          // FIXME: transform, max blit size

          /// loop
          for (unsigned int i=0; i<setup->tiles; i++) {
               if (!(setup->task_mask & (1 << i)))
                    continue;

               if (engine->caps.clipping & DFXL_BLIT) {
                    engine->Blit( setup->tasks[i], rects, points, num );
               }
               else {
                    Util::TempArray<DFBRectangle> copied_rects( num );
                    Util::TempArray<DFBPoint>     copied_points( num );
                    unsigned int                  copied_num = 0;

                    for (unsigned int n=0; n<num; n++) {
                         if (dfb_clip_blit_precheck( &setup->clips_clipped[i],
                                                     rects[n].w, rects[n].h,
                                                     points[n].x, points[n].y ))
                         {
                              copied_rects.array[copied_num]  = rects[n];
                              copied_points.array[copied_num] = points[n];

                              dfb_clip_blit( &setup->clips_clipped[i], &copied_rects.array[copied_num],
                                             &copied_points.array[copied_num].x, &copied_points.array[copied_num].y );

                              copied_num++;
                         }
                    }

                    if (copied_num)
                         engine->Blit( setup->tasks[i], copied_rects.array, copied_points.array, num );
               }
          }
     }
}

void
Renderer::Blit2( const DFBRectangle     *rects,
                 const DFBPoint         *points1,
                 const DFBPoint         *points2,
                 u32                     num )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     if (checkEngine( DFXL_BLIT2, num )) {
          update( DFXL_BLIT2 );

          // FIXME: clipping, transform

          /// loop
          for (unsigned int i=0; i<setup->tiles; i++) {
               if (!(setup->task_mask & (1 << i)))
                    continue;

               engine->Blit2( setup->tasks[i], rects, points1, points2, num );
          }
     }
}

void
Renderer::StretchBlit( const DFBRectangle     *srects,
                       const DFBRectangle     *drects,
                       u32                     num )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     if (checkEngine( DFXL_STRETCHBLIT, num )) {
          update( DFXL_STRETCHBLIT );

          // FIXME: clipping, transform

          /// loop
          for (unsigned int i=0; i<setup->tiles; i++) {
               if (!(setup->task_mask & (1 << i)))
                    continue;

               if (srects[i].w > drects[i].w * (int)engine->caps.max_scale_down_x || // FIXME: implement multi pass!
                   srects[i].h > drects[i].h * (int)engine->caps.max_scale_down_y)
                    continue;

               engine->StretchBlit( setup->tasks[i], srects, drects, num );
          }
     }
}

void
Renderer::TileBlit( const DFBRectangle     *rects,
                    const DFBPoint         *points1,
                    const DFBPoint         *points2,
                    u32                     num )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     if (checkEngine( DFXL_TILEBLIT, num )) {
          update( DFXL_TILEBLIT );

          // FIXME: clipping, transform

          /// loop
          for (unsigned int i=0; i<setup->tiles; i++) {
               if (!(setup->task_mask & (1 << i)))
                    continue;

               engine->TileBlit( setup->tasks[i], rects, points1, points2, num );
          }
     }
}

void
Renderer::TextureTriangles( const DFBVertex      *vertices,
                            int                   num,
                            DFBTriangleFormation  formation )
{
     unsigned int num_tris;

     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     switch (formation) {
          case DTTF_LIST:
               num_tris = num / 3;
               break;

          case DTTF_STRIP:
          case DTTF_FAN:
               num_tris = (num - 3) + 1;
               break;

          default:
               D_BUG( "invalid formation %d", formation );
               return;
     }

     if (checkEngine( DFXL_TEXTRIANGLES, num_tris )) {
          update( DFXL_TEXTRIANGLES );

          // FIXME: clipping, transform

          /// loop
          for (unsigned int i=0; i<setup->tiles; i++) {
               if (!(setup->task_mask & (1 << i)))
                    continue;

               engine->TextureTriangles( setup->tasks[i], vertices, num, formation );
          }
     }
}


bool
Renderer::checkEngine( DFBAccelerationMask accel,
                       unsigned int        num )
{
     DFBResult ret;

     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s( accel 0x%x ) <- modified 0x%08x\n", __FUNCTION__, accel, state->modified );

     RendererTLS *tls = Renderer_GetTLS();

     if (tls->last_renderer != this) {
          if (tls->last_renderer)
               tls->last_renderer->Flush();

          tls->last_renderer = this;
     }

     state->mod_hw = (StateModificationFlags)(state->mod_hw | state->modified);
     state_mod     = (StateModificationFlags)(state_mod     | state->modified);

     /* If destination or blend functions have been changed... */
     if (state->modified & (SMF_DESTINATION | SMF_SRC_BLEND | SMF_DST_BLEND | SMF_RENDER_OPTIONS)) {
          /* ...force rechecking for all functions. */
          state->checked = DFXL_NONE;
     }
     else {
          /* If source/mask or blitting flags have been changed... */
          if (state->modified & (SMF_SOURCE | SMF_BLITTING_FLAGS | SMF_SOURCE_MASK | SMF_SOURCE_MASK_VALS)) {
               /* ...force rechecking for all blitting functions. */
               state->checked = (DFBAccelerationMask)(state->checked & ~DFXL_ALL_BLIT);
          }
          else if (state->modified & SMF_SOURCE2) {
               /* Otherwise force rechecking for blit2 function if source2 has been changed. */
               state->checked = (DFBAccelerationMask)(state->checked & ~DFXL_BLIT2);
          }

          /* If drawing flags have been changed... */
          if (state->modified & SMF_DRAWING_FLAGS) {
               /* ...force rechecking for all drawing functions. */
               state->checked = (DFBAccelerationMask)(state->checked & ~DFXL_ALL_DRAW);
          }
     }

     state->modified = SMF_NONE;

     if (engine) {
//          printf("mod hw %d\n",state_mod & SMF_DESTINATION);
          if (operations + num <= engine->caps.max_operations && !(state_mod & SMF_DESTINATION)) {
               /* If the function needs to be checked... */
               if (!(state->checked & accel)) {
                    /* Unset unchecked functions. */
                    state->accel = (DFBAccelerationMask)(state->accel & state->checked);

                    /// use task[0]
                    if (engine->CheckState( state, accel ) == DFB_OK)
                         state->accel = (DFBAccelerationMask)(state->accel | accel);

                    /* Add the function to 'checked functions'. */
                    state->checked = (DFBAccelerationMask)(state->checked | state->accel);
               }

               if (state->accel & accel && engine->check( setup ) == DFB_OK) {
                    operations += num;

                    return true;
               }
          }

          unbindEngine();
     }

     for (std::list<Engine*>::const_iterator it = engines.begin(); it != engines.end(); ++it) {
          Engine *engine = *it;

          // TODO: add engine mask for selection by user

          if (dfb_config->software_only && !engine->caps.software) {
               D_DEBUG_AT( DirectFB_Renderer, "  -> skipping engine, software only!\n" );
               continue;
          }

          if (engine->CheckState( state, accel ) == DFB_OK) {
               state->accel   = accel;
               state->checked = accel;

               // TODO: eventually split up here, e.g. using intermediate surface for multi pass scaling


               ret = bindEngine( engine );
               if (ret == DFB_OK) {
                    operations += num;

                    return true;
               }
          }
     }

     return false;
}

DFBResult
Renderer::bindEngine( Engine *engine )
{
     DFBResult ret;

     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     D_ASSERT( this->engine == NULL );
     D_ASSERT( setup == NULL );

     /// loop
     setup = new Setup( state->destination->config.size.w, state->destination->config.size.h, engine->caps.cores );

     ret = engine->bind( setup );
     if (ret) {
          D_DERROR( ret, "DirectFB/Renderer: Failed to bind engine!\n" );
          return ret;
     }

#if D_DEBUG_ENABLED
     for (unsigned int i=0; i<setup->tiles; i++) {
          D_ASSERT( setup->tasks[i] != NULL );
          DFB_REGION_ASSERT( &setup->clips[i] );
     }
#endif

     /// prepare for par flush
     for (unsigned int i=1; i<setup->tiles; i++) {
          setup->tasks[0]->AddSlave( setup->tasks[i] );
     }

     state->modified = SMF_NONE;
     state->mod_hw   = SMF_ALL;
     state->set      = DFXL_NONE;

     state_mod       = SMF_ALL;

     this->engine = engine;

     return DFB_OK;
}

void
Renderer::unbindEngine()
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     D_ASSERT( engine != NULL );
     D_ASSERT( setup != NULL );

     leaveLock( &state->src2 );
     leaveLock( &state->src_mask );
     leaveLock( &state->src );
     leaveLock( &state->dst );

     state->accel   = DFXL_NONE;
     state->checked = DFXL_NONE;

     /// par flush
     setup->tasks[0]->Flush();

     delete setup;
     setup = NULL;

     engine     = NULL;
     operations = 0;

     allocations.clear();
}


/*********************************************************************************************************************/

std::list<Engine*>  Renderer::engines;


DFBResult
Renderer::RegisterEngine( Engine *engine )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     engines.push_back( engine );

     return DFB_OK;
}

void
Renderer::UnregisterEngine( Engine *engine )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     engines.remove( engine );
}

/*********************************************************************************************************************/

DFBResult
Engine::DrawRectangles( SurfaceTask        *task,
                        const DFBRectangle *rects,
                        unsigned int        num_rects )
{
     D_DEBUG_AT( DirectFB_Renderer, "Engine::%s()\n", __FUNCTION__ );

     return DFB_UNIMPLEMENTED;
}

DFBResult
Engine::DrawLines( SurfaceTask     *task,
                   const DFBRegion *lines,
                   unsigned int     num_lines )
{
     D_DEBUG_AT( DirectFB_Renderer, "Engine::%s()\n", __FUNCTION__ );

     return DFB_UNIMPLEMENTED;
}

DFBResult
Engine::FillRectangles( SurfaceTask        *task,
                        const DFBRectangle *rects,
                        unsigned int        num_rects )
{
     D_DEBUG_AT( DirectFB_Renderer, "Engine::%s()\n", __FUNCTION__ );

     return DFB_UNIMPLEMENTED;
}

DFBResult
Engine::FillTriangles( SurfaceTask       *task,
                       const DFBTriangle *tris,
                       unsigned int       num_tris )
{
     D_DEBUG_AT( DirectFB_Renderer, "Engine::%s()\n", __FUNCTION__ );

     return DFB_UNIMPLEMENTED;
}

DFBResult
Engine::FillTrapezoids( SurfaceTask        *task,
                        const DFBTrapezoid *traps,
                        unsigned int        num_traps )
{
     D_DEBUG_AT( DirectFB_Renderer, "Engine::%s()\n", __FUNCTION__ );

     return DFB_UNIMPLEMENTED;
}

DFBResult
Engine::FillSpans( SurfaceTask   *task,
                   int            y,
                   const DFBSpan *spans,
                   unsigned int   num_spans )
{
     D_DEBUG_AT( DirectFB_Renderer, "Engine::%s()\n", __FUNCTION__ );

     return DFB_UNIMPLEMENTED;
}

DFBResult
Engine::Blit( SurfaceTask        *task,
              const DFBRectangle *rects,
              const DFBPoint     *points,
              u32                 num )
{
     D_DEBUG_AT( DirectFB_Renderer, "Engine::%s()\n", __FUNCTION__ );

     return DFB_UNIMPLEMENTED;
}

DFBResult
Engine::Blit2( SurfaceTask        *task,
               const DFBRectangle *rects,
               const DFBPoint     *points1,
               const DFBPoint     *points2,
               u32                 num )
{
     D_DEBUG_AT( DirectFB_Renderer, "Engine::%s()\n", __FUNCTION__ );

     return DFB_UNIMPLEMENTED;
}

DFBResult
Engine::StretchBlit( SurfaceTask        *task,
                     const DFBRectangle *srects,
                     const DFBRectangle *drects,
                     u32                 num )
{
     D_DEBUG_AT( DirectFB_Renderer, "Engine::%s()\n", __FUNCTION__ );

     return DFB_UNIMPLEMENTED;
}

DFBResult
Engine::TileBlit( SurfaceTask        *task,
                  const DFBRectangle *rects,
                  const DFBPoint     *points1,
                  const DFBPoint     *points2,
                  u32                 num )
{
     D_DEBUG_AT( DirectFB_Renderer, "Engine::%s()\n", __FUNCTION__ );

     return DFB_UNIMPLEMENTED;
}

DFBResult
Engine::TextureTriangles( SurfaceTask          *task,
                          const DFBVertex      *vertices,
                          int                   num,
                          DFBTriangleFormation  formation )
{
     D_DEBUG_AT( DirectFB_Renderer, "Engine::%s()\n", __FUNCTION__ );

     return DFB_UNIMPLEMENTED;
}



















D_DEBUG_DOMAIN( Test_MyEngine, "Test/MyEngine", "Test MyEngine" );
D_DEBUG_DOMAIN( Test_MyTask,   "Test/MyTask",   "Test MyTask" );










class MyEngine;

class MyTask : public DirectFB::SurfaceTask
{
public:
     MyTask( MyEngine *engine )
          :
          SurfaceTask( CSAID_CPU ),
          engine( engine )
     {
     }

     virtual ~MyTask()
     {
     }

protected:
     virtual DFBResult Push();
     virtual DFBResult Run();

private:
     friend class MyEngine;

     MyEngine *engine;

     typedef enum {
          TYPE_SET_DESTINATION,
          TYPE_SET_SOURCE,
          TYPE_SET_COLOR,
          TYPE_FILL_RECTS,
          TYPE_BLIT
     } Type;

     std::vector<u32> commands;
};


class MyEngine : public DirectFB::Engine {
public:
     MyEngine()
     {
          D_DEBUG_AT( Test_MyEngine, "MyEngine::%s()\n", __FUNCTION__ );

          for (int i=0; i<1; i++) {
               char name[] = "MyEngineX";

               name[8] = '0' + i;

               threads[i] = direct_thread_create( DTT_DEFAULT, myEngineLoop, this, name );
          }
     }


     virtual DFBResult bind          ( Renderer::Setup        *setup )
     {
          D_DEBUG_AT( Test_MyEngine, "MyEngine::%s()\n", __FUNCTION__ );

          for (unsigned int i=0; i<setup->tiles; i++) {
               setup->tasks[i] = new MyTask( this );
          }

          return DFB_OK;
     }

     virtual DFBResult check         ( Renderer::Setup        *setup )
     {
          D_DEBUG_AT( Test_MyEngine, "MyEngine::%s()\n", __FUNCTION__ );

          return DFB_OK;
     }

     virtual DFBResult CheckState    ( CardState              *state,
                                       DFBAccelerationMask     accel )
     {
          D_DEBUG_AT( Test_MyEngine, "MyEngine::%s()\n", __FUNCTION__ );

          if (DFB_BLITTING_FUNCTION( accel )) {
               if (accel != DFXL_BLIT || //state->blittingflags ||
                   state->destination->config.format != DSPF_ARGB || state->source->config.format != DSPF_ARGB)
                    return DFB_UNSUPPORTED;
          }
          else {
               if (accel != DFXL_FILLRECTANGLE || state->drawingflags ||
                   state->destination->config.format != DSPF_ARGB)
                    return DFB_UNSUPPORTED;
          }

          return DFB_OK;
     }

     virtual DFBResult SetState      ( DirectFB::SurfaceTask  *task,
                                       CardState              *state,
                                       StateModificationFlags  modified,
                                       DFBAccelerationMask     accel )
     {
          MyTask *mytask = (MyTask *)task;

          D_DEBUG_AT( Test_MyEngine, "MyEngine::%s()\n", __FUNCTION__ );

          if (modified & SMF_DESTINATION) {
               mytask->commands.push_back( MyTask::TYPE_SET_DESTINATION );
               mytask->commands.push_back( (long long)(long)state->dst.addr >> 32 );
               mytask->commands.push_back( (u32)(long)state->dst.addr );
               mytask->commands.push_back( state->dst.pitch );

               state->mod_hw = (StateModificationFlags)(state->mod_hw & ~SMF_DESTINATION);
          }

          if (modified & SMF_COLOR) {
               mytask->commands.push_back( MyTask::TYPE_SET_COLOR );
               mytask->commands.push_back( PIXEL_ARGB( state->color.a, state->color.r, state->color.g, state->color.b ) );

               state->mod_hw = (StateModificationFlags)(state->mod_hw & ~SMF_COLOR);
          }

          if (DFB_BLITTING_FUNCTION( accel )) {
               if (modified & SMF_SOURCE) {
                    mytask->commands.push_back( MyTask::TYPE_SET_SOURCE );
                    mytask->commands.push_back( (long long)(long)state->src.addr >> 32 );
                    mytask->commands.push_back( (u32)(long)state->src.addr );
                    mytask->commands.push_back( state->src.pitch );

                    state->mod_hw = (StateModificationFlags)(state->mod_hw & ~SMF_SOURCE);
               }
          }

          return DFB_OK;
     }


     virtual DFBResult FillRectangles( DirectFB::SurfaceTask  *task,
                                       const DFBRectangle     *rects,
                                       unsigned int            num_rects )
     {
          MyTask *mytask = (MyTask *)task;

          D_DEBUG_AT( Test_MyEngine, "MyEngine::%s( %d )\n", __FUNCTION__, num_rects );

          mytask->commands.push_back( MyTask::TYPE_FILL_RECTS );
          mytask->commands.push_back( num_rects );

          for (unsigned int i=0; i<num_rects; i++) {
               mytask->commands.push_back( rects[i].x );
               mytask->commands.push_back( rects[i].y );
               mytask->commands.push_back( rects[i].w );
               mytask->commands.push_back( rects[i].h );
          }

          return DFB_OK;
     }


     virtual DFBResult Blit( DirectFB::SurfaceTask  *task,
                             const DFBRectangle     *rects,
                             const DFBPoint         *points,
                             u32                     num )
     {
          MyTask *mytask = (MyTask *)task;

          D_DEBUG_AT( Test_MyEngine, "MyEngine::%s( %d )\n", __FUNCTION__, num );

          mytask->commands.push_back( MyTask::TYPE_BLIT );
          mytask->commands.push_back( num );

          for (unsigned int i=0; i<num; i++) {
               mytask->commands.push_back( rects[i].x );
               mytask->commands.push_back( rects[i].y );
               mytask->commands.push_back( rects[i].w );
               mytask->commands.push_back( rects[i].h );
               mytask->commands.push_back( points[i].x );
               mytask->commands.push_back( points[i].y );
          }

          return DFB_OK;
     }


private:
     friend class MyTask;

     DirectFB::FIFO<MyTask*>  fifo;
     DirectThread            *threads[1];

     static void *
     myEngineLoop( DirectThread *thread,
                   void         *arg )
     {
          MyEngine *engine = (MyEngine *)arg;
          MyTask   *task;

          D_DEBUG_AT( Test_MyEngine, "MyEngine::%s()\n", __FUNCTION__ );

          while (true) {
               task = engine->fifo.pull();

               task->Run();
          }

          return NULL;
     }
};


DFBResult
MyTask::Push()
{
     D_DEBUG_AT( Test_MyTask, "MyTask::%s()\n", __FUNCTION__ );

     engine->fifo.push( this );

     return DFB_OK;
}

DFBResult
MyTask::Run()
{
     u32     ptr1      = 0;
     u32     ptr2      = 0;
     void   *ptr       = 0;
     u32     pitch     = 0;
     void   *src_ptr   = 0;
     u32     src_pitch = 0;
     u32     color     = 0;
     u32     num;
     size_t  size;

     D_DEBUG_AT( Test_MyTask, "MyTask::%s()\n", __FUNCTION__ );

     size = commands.size();
     if (size > 0) {
          u32 *buffer = &commands[0];

          for (unsigned int i=0; i<size; i++) {
               D_DEBUG_AT( Test_MyTask, "  -> next command at [%d]\n", i );

               switch (buffer[i]) {
                    case MyTask::TYPE_SET_DESTINATION:
                         D_DEBUG_AT( Test_MyTask, "  -> SET_DESTINATION\n" );

                         ptr1  = buffer[++i];
                         ptr2  = buffer[++i];
                         ptr   = (void*)(long)(((long long)ptr1 << 32) | ptr2);
                         D_DEBUG_AT( Test_MyTask, "  -> 0x%08x 0x%08x = %p\n", ptr1, ptr2, ptr );

                         pitch = buffer[++i];
                         D_DEBUG_AT( Test_MyTask, "  -> pitch %d\n", pitch );
                         break;

                    case MyTask::TYPE_SET_SOURCE:
                         D_DEBUG_AT( Test_MyTask, "  -> SET_SOURCE\n" );

                         ptr1    = buffer[++i];
                         ptr2    = buffer[++i];
                         src_ptr = (void*)(long)(((long long)ptr1 << 32) | ptr2);
                         D_DEBUG_AT( Test_MyTask, "  -> 0x%08x 0x%08x = %p\n", ptr1, ptr2, src_ptr );

                         src_pitch = buffer[++i];
                         D_DEBUG_AT( Test_MyTask, "  -> pitch %d\n", src_pitch );
                         break;

                    case MyTask::TYPE_SET_COLOR:
                         D_DEBUG_AT( Test_MyTask, "  -> SET_COLOR\n" );

                         color = buffer[++i];
                         D_DEBUG_AT( Test_MyTask, "  -> 0x%08x\n", color );
                         break;

                    case MyTask::TYPE_FILL_RECTS:
                         D_DEBUG_AT( Test_MyTask, "  -> FILL_RECTS\n" );

                         num = buffer[++i];
                         D_DEBUG_AT( Test_MyTask, "  -> num %d\n", num );

                         for (u32 n=0; n<num; n++) {
                              int x = buffer[++i];
                              int y = buffer[++i];
                              int w = buffer[++i];
                              int h = buffer[++i];

                              D_DEBUG_AT( Test_MyTask, "  -> %4d,%4d-%4dx%4d\n", x, y, w, h );

                              u32 *d = (u32 *)((u8*)ptr + pitch * y + x * 4);

                              while (h--) {
                                   for (int X=0; X<w; X++) {
                                        d[X] = color;
                                   }

                                   d += pitch/4;
                              }
                         }
                         break;

                    case MyTask::TYPE_BLIT:
                         D_DEBUG_AT( Test_MyTask, "  -> BLIT\n" );

                         num = buffer[++i];
                         D_DEBUG_AT( Test_MyTask, "  -> num %d\n", num );

                         for (u32 n=0; n<num; n++) {
                              int x  = buffer[++i];
                              int y  = buffer[++i];
                              int w  = buffer[++i];
                              int h  = buffer[++i];
                              int dx = buffer[++i];
                              int dy = buffer[++i];

                              D_DEBUG_AT( Test_MyTask, "  -> %4d,%4d-%4dx%4d -> %4d,%4d\n", x, y, w, h, dx, dy );

                              u32 *d = (u32 *)((u8*)ptr + pitch * dy + dx * 4);
                              u32 *s = (u32 *)((u8*)src_ptr + src_pitch * y + x * 4);

                              while (h--) {
                                   for (int X=0; X<w; X++) {
                                        d[X] = s[X];
                                   }

                                   d += pitch/4;
                                   s += src_pitch/4;
                              }
                         }
                         break;

                    default:
                         D_BUG( "unknown type %d", buffer[i] );
               }
          }
     }

     Done();

     return DFB_OK;
}


extern "C" {
     void
     register_myengine()
     {
          Renderer::RegisterEngine( new MyEngine() );
     }
}



}

