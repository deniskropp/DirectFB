/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#include "config.h"

#include <string.h>
#include <malloc.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <pthread.h>

#include <core/fusion/shmalloc.h>
#include <core/fusion/arena.h>
#include <core/fusion/list.h>

#include <directfb.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/gfxcard.h>
#include <core/fonts.h>
#include <core/state.h>
#include <core/palette.h>
#include <core/surfaces.h>
#include <core/surfacemanager.h>

#include <core/fbdev/fbdev.h>

#include <gfx/generic/generic.h>
#include <gfx/clip.h>
#include <gfx/util.h>

#include <misc/gfx_util.h>
#include <misc/utf8.h>
#include <misc/mem.h>
#include <misc/util.h>

typedef struct {
     FusionLink            link;

     GraphicsDriverFuncs  *funcs;

     int                   abi_version;
} GraphicsDriver;

/*
 * struct for graphics cards
 */
typedef struct {
     /* amount of usable video memory */
     unsigned int          videoram_length;

     GraphicsDriverInfo    driver_info;
     GraphicsDeviceInfo    device_info;
     void                 *device_data;

     FusionSkirmish        lock;

     SurfaceManager       *surface_manager;

     FusionObjectPool     *surface_pool;
     FusionObjectPool     *palette_pool;

     /*
      * Points to the current state of the graphics card.
      */
     CardState            *state;
} GraphicsDeviceShared;

struct _GraphicsDevice {
     GraphicsDeviceShared *shared;

     GraphicsDriver       *driver;
     void                 *driver_data;
     void                 *device_data; /* copy of shared->device_data */

     GraphicsDeviceFuncs   funcs;
};


static FusionLink     *graphics_drivers = NULL;

static GraphicsDevice *card = NULL;
#define Scard (card->shared)


static GraphicsDriver *dfb_gfxcard_find_driver();


/** public **/

DFBResult dfb_gfxcard_initialize()
{
     DFBResult       ret;
     GraphicsDriver *driver;

     card = (GraphicsDevice*) DFBCALLOC( 1, sizeof(GraphicsDevice) );

     Scard = (GraphicsDeviceShared*) shcalloc( 1, sizeof(GraphicsDeviceShared) );

     /* fill generic driver info */
     gGetDriverInfo( &Scard->driver_info );

     /* fill generic device info */
     gGetDeviceInfo( &Scard->device_info );


     /* Limit video ram length */
     if (dfb_config->videoram_limit > 0 &&
         dfb_config->videoram_limit < Sfbdev->fix.smem_len)
          Scard->videoram_length = dfb_config->videoram_limit;
     else
          Scard->videoram_length = Sfbdev->fix.smem_len;

     /* Load driver */
     driver = dfb_gfxcard_find_driver();
     if (driver) {
          card->driver_data = DFBCALLOC( 1,
                                         Scard->driver_info.driver_data_size );

          ret = driver->funcs->InitDriver( card,
                                           &card->funcs, card->driver_data );
          if (ret) {
               DFBFREE( card->driver_data );
               DFBFREE( driver );
               DFBFREE( card );
               card = NULL;
               return ret;
          }

          Scard->device_data = shcalloc( 1,
                                         Scard->driver_info.device_data_size );

          ret = driver->funcs->InitDevice( card, &Scard->device_info,
                                           card->driver_data, Scard->device_data );
          if (ret) {
               driver->funcs->CloseDriver( card, card->driver_data );
               shfree( Scard->device_data );
               DFBFREE( card->driver_data );
               DFBFREE( driver );
               DFBFREE( card );
               card = NULL;
               return ret;
          }

          card->driver      = driver;
          card->device_data = Scard->device_data;
     }

     INITMSG( "DirectFB/GraphicsDevice: %s %s %d.%d (%s)\n",
              Scard->device_info.vendor, Scard->device_info.name,
              Scard->driver_info.version.major,
              Scard->driver_info.version.minor, Scard->driver_info.vendor );

     Scard->surface_manager = dfb_surfacemanager_create( Scard->videoram_length,
                card->shared->device_info.limits.surface_byteoffset_alignment,
                card->shared->device_info.limits.surface_pixelpitch_alignment );

     Scard->palette_pool = dfb_palette_pool_create();
     Scard->surface_pool = dfb_surface_pool_create();

     skirmish_init( &Scard->lock );

#ifndef FUSION_FAKE
     arena_add_shared_field( dfb_core->arena, Scard, "Scard" );
#endif

     return DFB_OK;
}

#ifndef FUSION_FAKE
DFBResult dfb_gfxcard_join()
{
     DFBResult       ret;
     GraphicsDriver *driver;

     card = (GraphicsDevice*)DFBCALLOC( 1, sizeof(GraphicsDevice) );

     arena_get_shared_field( dfb_core->arena, (void**) &Scard, "Scard" );

     /* load driver, FIXME: do not probe */
     driver = dfb_gfxcard_find_driver();
     if (driver) {
          card->driver_data = DFBCALLOC( 1,
                                         Scard->driver_info.driver_data_size );

          ret = driver->funcs->InitDriver( card,
                                           &card->funcs, card->driver_data );
          if (ret) {
               DFBFREE( card->driver_data );
               DFBFREE( driver );
               DFBFREE( card );
               card = NULL;
               return ret;
          }

          card->driver      = driver;
          card->device_data = Scard->device_data;
     }

     return DFB_OK;
}
#endif

DFBResult dfb_gfxcard_shutdown( bool emergency )
{
     int i;

     if (!card)
          return DFB_OK;

     if (emergency) {
          /* try to prohibit graphics hardware access,
             this may fail if the current thread locked it */
          for (i=0; i<100; i++) {
               dfb_gfxcard_sync();

               if (skirmish_swoop( &Scard->lock ) != EBUSY)
                    break;

               sched_yield();
          }
     }
     else
          skirmish_prevail( &Scard->lock );

     if (card->driver) {
          card->driver->funcs->CloseDevice( card,
                                            card->driver_data, card->device_data );
          card->driver->funcs->CloseDriver( card, card->driver_data );

          shfree( card->device_data );
          DFBFREE( card->driver_data );
     }

     dfb_surface_pool_destroy( Scard->surface_pool );
     dfb_palette_pool_destroy( Scard->palette_pool );

     dfb_surfacemanager_destroy( Scard->surface_manager );

     skirmish_destroy( &Scard->lock );

     shfree( Scard );

     DFBFREE( card );
     card = NULL;

     return DFB_OK;
}

#ifndef FUSION_FAKE
DFBResult dfb_gfxcard_leave( bool emergency )
{
     if (!card)
          return DFB_OK;

     dfb_gfxcard_sync();

     if (card->driver) {
          card->driver->funcs->CloseDriver( card, card->driver_data );

          DFBFREE( card->driver_data );
     }

     DFBFREE( card );
     card = NULL;

     return DFB_OK;
}
#endif

#ifdef FUSION_FAKE
DFBResult dfb_gfxcard_suspend()
{
     dfb_gfxcard_sync();

     return dfb_surfacemanager_suspend( card->shared->surface_manager );
}

DFBResult dfb_gfxcard_resume()
{
     return dfb_surfacemanager_resume( card->shared->surface_manager );
}
#endif

void dfb_graphics_register_module( GraphicsDriverFuncs *funcs )
{
     GraphicsDriver *driver;

     driver = DFBCALLOC( 1, sizeof(GraphicsDriver) );

     driver->funcs       = funcs;
     driver->abi_version = funcs->GetAbiVersion();

     fusion_list_prepend( &graphics_drivers, &driver->link );
}

/*
 * This function returns non zero if acceleration is available
 * for the specific function using the given state.
 */
int dfb_gfxcard_state_check( CardState *state, DFBAccelerationMask accel )
{
     /*
      * If there's no CheckState function there's no acceleration at all.
      */
     if (!card->funcs.CheckState)
          return 0;

     /* Debug checks */
     if (!state->destination) {
          BUG("state check: no destination");
          return 0;
     }
     if (!state->source  &&  DFB_BLITTING_FUNCTION( accel )) {
          BUG("state check: no source");
          return 0;
     }

     /*
      * If back_buffer policy is 'system only' there's no acceleration
      * available.
      */
     if (state->destination->back_buffer->policy == CSP_SYSTEMONLY) {

          /* clear 'accelerated functions' */
          state->accel = 0;

          /* return immediately */
          return 0;
     }

     /*
      * If front_buffer policy is 'system only' there's no accelerated
      * blitting available.
      */
     if (state->source &&
         state->source->front_buffer->policy == CSP_SYSTEMONLY) {
       
          /* clear 'accelerated blitting functions' */
          state->accel &= 0xFFFF;

          /* return if blitting function was requested */
          if (DFB_BLITTING_FUNCTION( accel ))
               return 0;
     }

     /* If destination has been changed... */
     if (state->modified & SMF_DESTINATION) {
          /* ...force rechecking for all functions. */
          state->checked = 0;
     }

     /* If source has been changed... */
     if (state->modified & SMF_SOURCE) {
          /* ...force rechecking for all blitting functions. */
          state->checked &= 0xFFFF;
     }

     /* If blend functions have been changed force recheck. */
     if (state->modified & (SMF_SRC_BLEND | SMF_DST_BLEND)) {
          state->checked = 0;
     }
     else {
          /* If drawing flags have been changed recheck drawing functions. */
          if (state->modified & SMF_DRAWING_FLAGS)
               state->checked &= 0xFFFF0000;

          /* If blitting flags have been changed recheck blitting functions. */
          if (state->modified & SMF_BLITTING_FLAGS)
               state->checked &= 0xFFFF;
     }

     /* if function needs to be checked... */
     if (!(state->checked & accel)) {
          /* unset function */
          state->accel &= ~accel;

          /* call driver function that sets the bit if supported */
          card->funcs.CheckState( card->driver_data,
                                  card->device_data, state, accel );

          /* add function to 'checked functions' */
          state->checked |= accel;
     }

     return (state->accel & accel);
}

/*
 * This function returns non zero after successful locking the surface(s)
 * for access by hardware. Propagate state changes to driver.
 */
int dfb_gfxcard_state_acquire( CardState *state, DFBAccelerationMask accel )
{
     DFBSurfaceLockFlags lock_flags;

     /* Debug checks */
     if (!state->destination) {
          BUG("state check: no destination");
          return 0;
     }
     if (!state->source  &&  DFB_BLITTING_FUNCTION( accel )) {
          BUG("state check: no source");
          return 0;
     }

     /* find locking flags */
     if (DFB_BLITTING_FUNCTION( accel ))
          lock_flags = (state->blittingflags & ( DSBLIT_BLEND_ALPHACHANNEL |
                                                 DSBLIT_BLEND_COLORALPHA   |
                                                 DSBLIT_DST_COLORKEY ) ?
                        DSLF_READ | DSLF_WRITE : DSLF_WRITE) | CSLF_FORCE;
     else
          lock_flags = state->drawingflags & ( DSDRAW_BLEND |
                                               DSDRAW_DST_COLORKEY ) ?
                       DSLF_READ | DSLF_WRITE : DSLF_WRITE;

     /* lock surface manager */
     dfb_surfacemanager_lock( card->shared->surface_manager );

     /* if blitting... */
     if (DFB_BLITTING_FUNCTION( accel )) {
          /* ...lock source for reading */
          if (dfb_surface_hardware_lock( state->source, DSLF_READ, 1 )) {
               dfb_surfacemanager_unlock( card->shared->surface_manager );
               return 0;
          }

          state->source_locked = 1;
     }
     else
          state->source_locked = 0;

     /* lock destination */
     if (dfb_surface_hardware_lock( state->destination, lock_flags, 0 )) {
          if (state->source_locked)
               dfb_surface_unlock( state->source, 1 );

          dfb_surfacemanager_unlock( card->shared->surface_manager );
          return 0;
     }

     /* unlock surface manager */
     dfb_surfacemanager_unlock( card->shared->surface_manager );

     /*
      * Make sure that state setting with subsequent command execution
      * isn't done by two processes simultaneously.
      */
     if (skirmish_prevail( &Scard->lock ))
          return 0;

     /* if we are switching to another state... */
     if (state != Scard->state) {
          /* ...set all modification bits and clear 'set functions' */
          state->modified |= SMF_ALL;
          state->set       = 0;

          Scard->state = state;
     }

     /*
      * If function hasn't been set or state is modified
      * call the driver function to propagate the state changes.
      */
     if (!(state->set & accel) || state->modified)
          card->funcs.SetState( card->driver_data, card->device_data,
                                &card->funcs, state, accel );

     return 1;
}

/*
 * Unlock destination and possibly the source.
 */
void dfb_gfxcard_state_release( CardState *state )
{
     /* destination always gets locked during acquisition */
     dfb_surface_unlock( state->destination, 0 );

     /* if source got locked this value is true */
     if (state->source_locked)
          dfb_surface_unlock( state->source, 1 );

     /* allow others to use the hardware */
     skirmish_dismiss( &Scard->lock );
}

/** DRAWING FUNCTIONS **/

void dfb_gfxcard_fillrectangle( DFBRectangle *rect, CardState *state )
{
     /* The state is locked during graphics operations. */
     dfb_state_lock( state );

     /* Check for acceleration and setup execution. */
     if (dfb_gfxcard_state_check( state, DFXL_FILLRECTANGLE ) &&
         dfb_gfxcard_state_acquire( state, DFXL_FILLRECTANGLE ))
     {
          /*
           * Either hardware has clipping support or the software clipping
           * routine returned that there's something to do.
           */
          if ((Scard->device_info.caps.flags & CCF_CLIPPING) ||
              dfb_clip_rectangle( &state->clip, rect ))
          {
               /*
                * Now everything is prepared for execution of the
                * FillRectangle driver function.
                */
               card->funcs.FillRectangle( card->driver_data,
                                          card->device_data, rect );
          }

          /* Release after state acquisition. */
          dfb_gfxcard_state_release( state );
     }
     else {
          /*
           * Otherwise use the software clipping routine and execute the
           * software fallback if the rectangle isn't completely clipped.
           */
          if (dfb_clip_rectangle( &state->clip, rect ) &&
              gAquire( state, DFXL_FILLRECTANGLE ))
          {
               gFillRectangle( rect );
               gRelease( state );
          }
     }

     /* Unlock after execution. */
     dfb_state_unlock( state );
}

void dfb_gfxcard_drawrectangle( DFBRectangle *rect, CardState *state )
{
     dfb_state_lock( state );

     if (dfb_gfxcard_state_check( state, DFXL_DRAWRECTANGLE ) &&
         dfb_gfxcard_state_acquire( state, DFXL_DRAWRECTANGLE )) {
          if (Scard->device_info.caps.flags & CCF_CLIPPING  ||
              dfb_clip_rectangle( &state->clip, rect )) {
               /* FIXME: correct clipping like below */
               card->funcs.DrawRectangle( card->driver_data,
                                          card->device_data, rect );
          }
          dfb_gfxcard_state_release( state );
     }
     else {
          unsigned int edges = dfb_clip_rectangle (&state->clip, rect);

          if (edges & 0xF) {
               if (gAquire( state, DFXL_DRAWLINE)) {
                    DFBRegion line;

                    if (edges & 1) {
                         line.x1 = line.x2 = rect->x;
                         line.y1 = rect->y + (edges & 2 ? 1 : 0);
                         line.y2 = rect->y + rect->h - 1;
                         gDrawLine( &line );
                    }
                    if (edges & 2) {
                         line.x1 = rect->x;
                         line.x2 = rect->x + rect->w - (edges & 4 ? 2 : 1);
                         line.y1 = line.y2 = rect->y;
                         gDrawLine( &line );
                    }
                    if (edges & 4) {
                         line.x1 = line.x2 = rect->x + rect->w - 1;
                         line.y1 = rect->y;
                         line.y2 = rect->y + rect->h - (edges & 8 ? 2 : 1);
                         gDrawLine( &line );
                    }
                    if (edges & 8) {
                         line.x1 = rect->x + (edges & 1 ? 1 : 0);
                         line.x2 = rect->x + rect->w - 1;
                         line.y1 = line.y2 = rect->y + rect->h - 1;
                         gDrawLine( &line );
                    }

                    gRelease (state);
               }
          }
     }

     dfb_state_unlock( state );
}

void dfb_gfxcard_drawlines( DFBRegion *lines, int num_lines, CardState *state )
{
     int i;

     dfb_state_lock( state );

     if (dfb_gfxcard_state_check( state, DFXL_DRAWLINE ) &&
         dfb_gfxcard_state_acquire( state, DFXL_DRAWLINE )) {
          if (Scard->device_info.caps.flags & CCF_CLIPPING)
               for (i=0; i<num_lines; i++)
                    card->funcs.DrawLine( card->driver_data,
                                          card->device_data, &lines[i] );
          else
               for (i=0; i<num_lines; i++) {
                    if (dfb_clip_line( &state->clip, &lines[i] ))
                         card->funcs.DrawLine( card->driver_data,
                                               card->device_data, &lines[i] );
               }

          dfb_gfxcard_state_release( state );
     }
     else {
          if (gAquire( state, DFXL_DRAWLINE )) {
               for (i=0; i<num_lines; i++) {
                    if (dfb_clip_line( &state->clip, &lines[i] ))
                         gDrawLine( &lines[i] );
               }

               gRelease( state );
          }
     }

     dfb_state_unlock( state );
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
          int dx = xe - xs;                \
          int dy = ye - ys;                \
          dda.xi = xs;                     \
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
static
void fill_tri( DFBTriangle *tri, CardState *state, int accelerated )
{
     int y, yend;
     DDA dda1, dda2;
     int clip_x1 = state->clip.x1;
     int clip_x2 = state->clip.x2;

     y = tri->y1;
     yend = tri->y3;

     if (y < state->clip.y1)
          y = state->clip.y1;

     if (yend > state->clip.y2)
          yend = state->clip.y2;

     SETUP_DDA(tri->x1, tri->y1, tri->x3, tri->y3, dda1);
     SETUP_DDA(tri->x1, tri->y1, tri->x2, tri->y2, dda2);

     while (y < yend) {
          DFBRectangle rect;

          if (y == tri->y2) {
               if (tri->y2 == tri->y3)
                    return;
               SETUP_DDA(tri->x2, tri->y2, tri->x3, tri->y3, dda2);
          }

          rect.w = ABS(dda1.xi - dda2.xi);
          rect.x = MIN(dda1.xi, dda2.xi);

          if (clip_x2 < rect.x + rect.w)
               rect.w = clip_x2 - rect.x;

          if (rect.w > 0) {
               if (clip_x1 > rect.x)
                    rect.x = clip_x1;
               rect.y = y;
               rect.h = 1;

               if (accelerated)
                    card->funcs.FillRectangle( card->driver_data,
                                               card->device_data, &rect );
               else
                    gFillRectangle (&rect);
          }

          INC_DDA(dda1);
          INC_DDA(dda2);

          y++;
     }
}


void dfb_gfxcard_filltriangle( DFBTriangle *tri, CardState *state )
{
     dfb_state_lock( state );

     /* if hardware has clipping try directly accelerated triangle filling */
     if ((Scard->device_info.caps.flags & CCF_CLIPPING) &&
          dfb_gfxcard_state_check( state, DFXL_FILLTRIANGLE ) &&
          dfb_gfxcard_state_acquire( state, DFXL_FILLTRIANGLE ))
     {
          card->funcs.FillTriangle( card->driver_data,
                                    card->device_data, tri );
          dfb_gfxcard_state_release( state );
     }
     else {
          /* otherwise use the spanline rasterizer (fill_tri)
             and fill the triangle using a rectangle for each spanline */

          dfb_sort_triangle( tri );

          if (tri->y3 - tri->y1 > 0) {
               /* try hardware accelerated rectangle filling */
               if (dfb_gfxcard_state_check( state, DFXL_FILLRECTANGLE ) &&
                   dfb_gfxcard_state_acquire( state, DFXL_FILLRECTANGLE ))
               {
                    fill_tri( tri, state, 1 );

                    dfb_gfxcard_state_release( state );
               }
               else if (gAquire( state, DFXL_FILLTRIANGLE )) {
                    fill_tri( tri, state, 0 );

                    gRelease( state );
               }
          }
     }

     dfb_state_unlock( state );
}


void dfb_gfxcard_blit( DFBRectangle *rect, int dx, int dy, CardState *state )
{
     dfb_state_lock( state );

     if (!dfb_clip_blit_precheck( &state->clip, rect->w, rect->h, dx, dy )) {
          /* no work at all */
          dfb_state_unlock( state );
          return;
     }

     if (dfb_gfxcard_state_check( state, DFXL_BLIT ) &&
         dfb_gfxcard_state_acquire( state, DFXL_BLIT )) {
          if (!(Scard->device_info.caps.flags & CCF_CLIPPING))
               dfb_clip_blit( &state->clip, rect, &dx, &dy );

          card->funcs.Blit( card->driver_data, card->device_data,
                            rect, dx, dy );
          dfb_gfxcard_state_release( state );
     }
     else {
          if (gAquire( state, DFXL_BLIT )) {
               dfb_clip_blit( &state->clip, rect, &dx, &dy );
               gBlit( rect, dx, dy );
               gRelease( state );
          }
     }

     dfb_state_unlock( state );
}

void dfb_gfxcard_tileblit( DFBRectangle *rect, int dx, int dy, int w, int h,
                           CardState *state )
{
     int x, y;
     int odx;
     DFBRectangle srect;

     /* If called with an invalid rectangle, the algorithm goes into an
        infinite loop. This should never happen but it's safer to check. */
     if (rect->w < 1 || rect->h < 1) {
          BUG( "invalid rectangle" );
          return;
     }

     odx = dx;

     dfb_state_lock( state );

     if (dfb_gfxcard_state_check( state, DFXL_BLIT ) &&
         dfb_gfxcard_state_acquire( state, DFXL_BLIT )) {

          for (; dy < h; dy += rect->h) {
               for (dx = odx; dx < w; dx += rect->w) {

                    if (!dfb_clip_blit_precheck( &state->clip,
                                                 rect->w, rect->h, dx, dy ))
                         continue;

                    x = dx;
                    y = dy;
                    srect = *rect;

                    if (!(Scard->device_info.caps.flags & CCF_CLIPPING))
                         dfb_clip_blit( &state->clip, &srect, &x, &y );

                    card->funcs.Blit( card->driver_data, card->device_data,
                                      &srect, x, y );
               }
          }
          dfb_gfxcard_state_release( state );
     }
     else {
          if (gAquire( state, DFXL_BLIT )) {

               for (; dy < h; dy += rect->h) {
                    for (dx = odx; dx < w; dx += rect->w) {

                         if (!dfb_clip_blit_precheck( &state->clip,
                                                      rect->w, rect->h,
                                                      dx, dy ))
                              continue;

                         x = dx;
                         y = dy;
                         srect = *rect;

                         dfb_clip_blit( &state->clip, &srect, &x, &y );

                         gBlit( &srect, x, y );
                    }
               }
               gRelease( state );
          }
     }

     dfb_state_unlock( state );
}

void dfb_gfxcard_stretchblit( DFBRectangle *srect, DFBRectangle *drect,
                              CardState *state )
{
     dfb_state_lock( state );

     if (!dfb_clip_blit_precheck( &state->clip, drect->w, drect->h,
                                  drect->x, drect->y ))
     {
          dfb_state_unlock( state );
          return;
     }

     if (dfb_gfxcard_state_check( state, DFXL_STRETCHBLIT ) &&
         dfb_gfxcard_state_acquire( state, DFXL_STRETCHBLIT )) {
          if (!(Scard->device_info.caps.flags & CCF_CLIPPING))
               dfb_clip_stretchblit( &state->clip, srect, drect );

          card->funcs.StretchBlit( card->driver_data,
                                   card->device_data, srect, drect );
          dfb_gfxcard_state_release( state );
     }
     else {
          if (gAquire( state, DFXL_STRETCHBLIT )) {
               dfb_clip_stretchblit( &state->clip, srect, drect );
               gStretchBlit( srect, drect );
               gRelease( state );
          }
     }

     dfb_state_unlock( state );
}

void dfb_gfxcard_drawstring( const __u8 *text, int bytes,
                             int x, int y,
                             CoreFont *font, CardState *state )
{
     CoreGlyphData *data;
     DFBRectangle   rect;

     unichar prev = 0;
     unichar current;

     int hw_clipping = (Scard->device_info.caps.flags & CCF_CLIPPING);
     int kern_x;
     int kern_y;
     int offset;
     int blit = 0;

     dfb_font_lock( font );

     /* simple prechecks */
     if (y > state->clip.y2 ||
         y + font->ascender - font->descender <= state->clip.y1) {
          dfb_font_unlock( font );
          return;
     }

     dfb_state_set_destination( &font->state, state->destination );

     /* set clip and color */
     font->state.clip        = state->clip;
     font->state.color       = state->color;
     font->state.color_index = state->color_index;
     font->state.modified |= SMF_CLIP | SMF_COLOR;

     for (offset = 0; offset < bytes; offset += dfb_utf8_skip[text[offset]]) {

          current = dfb_utf8_get_char (&text[offset]);

          if (dfb_font_get_glyph_data (font, current, &data) == DFB_OK) {
               if (prev && font->GetKerning &&
                   (* font->GetKerning) (font, 
                                         prev, current, 
                                         &kern_x, &kern_y) == DFB_OK) {
                    x += kern_x;
                    y += kern_y;
               }

               rect.x = data->start;
               rect.y = 0;
               rect.w = data->width;
               rect.h = data->height;

               if (rect.w > 0) {
                    int xx = x + data->left;
                    int yy = y + data->top;

                    if (font->state.source != data->surface || !blit) {
                         switch (blit) {
                              case 1:
                                   dfb_gfxcard_state_release( &font->state );
                                   break;
                              case 2:
                                   gRelease( &font->state );
                                   break;
                              default:
                                   break;
                         }
                         dfb_state_set_source( &font->state, data->surface );

                         if (dfb_gfxcard_state_check( &font->state, DFXL_BLIT ) &&
                              dfb_gfxcard_state_acquire( &font->state, DFXL_BLIT ))
                              blit = 1;
                         else if (gAquire( &font->state, DFXL_BLIT ))
                              blit = 2;
                         else
                              blit = 0;
                    }

                    if (dfb_clip_blit_precheck( &font->state.clip,
                                                rect.w, rect.h, xx, yy )) {
                         switch (blit) {
                              case 1:
                                   if (!hw_clipping)
                                        dfb_clip_blit( &font->state.clip,
                                                       &rect, &xx, &yy );
                                   card->funcs.Blit( card->driver_data,
                                                     card->device_data,
                                                     &rect, xx, yy );
                                   break;
                              case 2:
                                   dfb_clip_blit( &font->state.clip,
                                                  &rect, &xx, &yy );
                                   gBlit( &rect, xx, yy );
                                   break;
                              default:
                                   break;
                         }
                    }
               }
               x += data->advance;
               prev = current;
          }
     }

     switch (blit) {
          case 1:
               dfb_gfxcard_state_release( &font->state );
               break;
          case 2:
               gRelease( &font->state );
               break;
          default:
               break;
     }

     dfb_font_unlock( font );
}

void dfb_gfxcard_drawglyph( unichar index, int x, int y,
                            CoreFont *font, CardState *state )
{
     CoreGlyphData *data;
     DFBRectangle   rect;

     dfb_font_lock( font );

     if (dfb_font_get_glyph_data (font, index, &data) != DFB_OK ||
         data->width <= 0 || data->height <= 0) {

          dfb_font_unlock( font );
          return;
     }

     x += data->left;
     y += data->top;

     if (! dfb_clip_blit_precheck( &state->clip,
                                   data->width, data->height, x, y )) {
          dfb_font_unlock( font );
          return;
     }

     dfb_state_set_destination( &font->state, state->destination );

     /* set clip and color */
     font->state.clip        = state->clip;
     font->state.color       = state->color;
     font->state.color_index = state->color_index;
     font->state.modified |= SMF_CLIP | SMF_COLOR;

     dfb_state_set_source( &font->state, data->surface );

     rect.x = data->start;
     rect.y = 0;
     rect.w = data->width;
     rect.h = data->height;

     if (dfb_gfxcard_state_check( &font->state, DFXL_BLIT ) &&
         dfb_gfxcard_state_acquire( &font->state, DFXL_BLIT )) {

          if (!(Scard->device_info.caps.flags & CCF_CLIPPING))
               dfb_clip_blit( &font->state.clip, &rect, &x, &y );

          card->funcs.Blit( card->driver_data, card->device_data, &rect, x, y);
          dfb_gfxcard_state_release( &font->state );
     }
     else if (gAquire( &font->state, DFXL_BLIT )) {

          dfb_clip_blit( &font->state.clip, &rect, &x, &y );
          gBlit( &rect, x, y );
          gRelease( &font->state );
     }

     dfb_font_unlock( font );
}

volatile void *dfb_gfxcard_map_mmio( GraphicsDevice *device,
                                     unsigned int    offset,
                                     int             length )
{
     void *addr;

     if (length < 0)
          length = Sfbdev->fix.mmio_len;

     addr = mmap( NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED,
                  dfb_fbdev->fd, Sfbdev->fix.smem_len + offset );
     if ((int)(addr) == -1) {
          PERRORMSG( "DirectFB/core/gfxcard: Could not mmap MMIO region "
                     "(offset %d, length %d)!\n", offset, length );
          return NULL;
     }

     return (volatile void*) addr;
}

void dfb_gfxcard_unmap_mmio( GraphicsDevice *device,
                             volatile void  *addr,
                             int             length )
{
     if (length < 0)
          length = Sfbdev->fix.mmio_len;

     if (munmap( (void*) addr, length ) < 0)
          PERRORMSG( "DirectFB/core/gfxcard: Could not unmap MMIO region "
                     "at %p (length %d)!\n", addr, length );
}

int dfb_gfxcard_get_accelerator( GraphicsDevice *device )
{
#ifdef FB_ACCEL_MATROX_MGAG400
     if (!strcmp( Sfbdev->fix.id, "MATROX DH" ))
          return FB_ACCEL_MATROX_MGAG400;
#endif
     return Sfbdev->fix.accel;
}

void dfb_gfxcard_sync()
{
     if (card->funcs.EngineSync)
          card->funcs.EngineSync( card->driver_data, card->device_data );
}

void dfb_gfxcard_flush_texture_cache()
{
     if (card->funcs.FlushTextureCache)
          card->funcs.FlushTextureCache( card->driver_data, card->device_data );
}

void dfb_gfxcard_after_set_var()
{
     if (card->funcs.AfterSetVar)
          card->funcs.AfterSetVar( card->driver_data, card->device_data );
}

DFBResult
dfb_gfxcard_adjust_heap_offset( unsigned int offset )
{
     return dfb_surfacemanager_adjust_heap_offset( Scard->surface_manager, offset );
}

SurfaceManager *
dfb_gfxcard_surface_manager()
{
     return Scard->surface_manager;
}

FusionObjectPool *
dfb_gfxcard_surface_pool()
{
     return Scard->surface_pool;
}

FusionObjectPool *
dfb_gfxcard_palette_pool()
{
     return Scard->palette_pool;
}

CardCapabilities
dfb_gfxcard_capabilities()
{
     return Scard->device_info.caps;
}

int
dfb_gfxcard_reserve_memory( GraphicsDevice *device, unsigned int size )
{
     GraphicsDeviceShared *shared = device->shared;

     DFB_ASSERT( shared != NULL );

     if (shared->surface_manager)
          return -1;

     if (shared->videoram_length < size)
          return -1;

     shared->videoram_length -= size;

     return shared->videoram_length;
}

unsigned long
dfb_gfxcard_memory_physical( unsigned int offset )
{
     return Sfbdev->fix.smem_start + offset;
}

void *
dfb_gfxcard_memory_virtual( unsigned int offset )
{
     return (void*)((__u8*)(dfb_fbdev->framebuffer_base) + offset);
}

unsigned int
dfb_gfxcard_memory_length()
{
     return Scard->videoram_length;
}

/** internal **/

#ifdef DFB_DYNAMIC_LINKING
static CoreModuleLoadResult graphics_driver_handle_func( void *handle,
                                                         char *name,
                                                         void *ctx )
{
     GraphicsDriver *driver = (GraphicsDriver*) graphics_drivers;

     if (!driver)
          return MODULE_REJECTED;

     if (driver->abi_version != DFB_GRAPHICS_DRIVER_ABI_VERSION) {
          ERRORMSG( "DirectFB/core/gfxcard: '%s' "
                    "was built for ABI version %d, but %d is required!\n", name,
                    driver->abi_version, DFB_GRAPHICS_DRIVER_ABI_VERSION );

          fusion_list_remove( &graphics_drivers, graphics_drivers );

          DFBFREE( driver );

          return MODULE_REJECTED;
     }

     return MODULE_LOADED_CONTINUE;
}
#endif

/*
 * loads/probes/unloads one driver module after another until a suitable
 * driver is found and returns its symlinked functions
 */
static GraphicsDriver* dfb_gfxcard_find_driver()
{
     FusionLink *link;

     if (dfb_config->software_only)
          return NULL;

#ifdef DFB_DYNAMIC_LINKING
     dfb_core_load_modules( MODULEDIR"/gfxdrivers",
                            graphics_driver_handle_func, NULL );
#endif

     fusion_list_foreach( link, graphics_drivers ) {
          GraphicsDriver *driver = (GraphicsDriver*) link;

          if (driver->funcs->Probe( card )) {
               driver->funcs->GetDriverInfo( card, &Scard->driver_info );

               return driver;
          }
     }

     return NULL;
}

