/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de>,
              Andreas Hundt <andi@convergence.de> and
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

#include "directfb.h"

#include "core.h"
#include "coredefs.h"
#include "coretypes.h"

#include "gfxcard.h"
#include "fbdev.h"
#include "fonts.h"
#include "state.h"
#include "surfaces.h"
#include "surfacemanager.h"

#include "gfx/generic/generic.h"
#include "gfx/util.h"

#include "misc/gfx_util.h"
#include "misc/utf8.h"
#include "misc/mem.h"
#include "misc/util.h"


typedef struct {
     int       (*GetAbiVersion)  ();
     int       (*Probe)          (GraphicsDevice      *device);
     void      (*GetDriverInfo)  (GraphicsDevice      *device,
                                  GraphicsDriverInfo  *driver_info);

     DFBResult (*InitDriver)     (GraphicsDevice      *device,
                                  GraphicsDeviceFuncs *funcs,
                                  void                *driver_data);

     DFBResult (*InitDevice)     (GraphicsDevice      *device,
                                  GraphicsDeviceInfo  *device_info,
                                  void                *driver_data,
                                  void                *device_data);

     /* temporary function to have hardware layers in single app core,
        this will change after restructuring layer driver data handling */
     DFBResult (*InitLayers)     (void                *driver_data,
                                  void                *device_data);


     void      (*CloseDevice)    (GraphicsDevice      *device,
                                  void                *driver_data,
                                  void                *device_data);
     void      (*CloseDriver)    (GraphicsDevice      *device,
                                  void                *driver_data);
} GraphicsDriverModule;

/*
 * struct for graphics cards
 */
typedef struct {
     /* fbdev fixed screeninfo, contains infos about memory and type of card */
     struct fb_fix_screeninfo fix;

     char               *driver_module;

     GraphicsDriverInfo  driver_info;
     GraphicsDeviceInfo  device_info;
     void               *device_data;

     FusionSkirmish      lock;

     SurfaceManager     *surface_manager;

     /*
      * Points to the current state of the graphics card.
      */
     CardState          *state;
} GraphicsDeviceShared;

struct _GraphicsDevice {
     GraphicsDeviceShared *shared;

     GraphicsDriverModule *driver;
     void                 *driver_data;
     void                 *device_data; /* copy of shared->device_data */

     GraphicsDeviceFuncs   funcs;

     /* framebuffer address and size */
     struct {
          unsigned int     length;
          void            *base;
     } framebuffer;
};



static GraphicsDevice *card = NULL;
#define Scard (card->shared)


static CoreModuleLoadResult  gfxcard_driver_handle_func( void *handle,
                                                         char *name,
                                                         void *ctx );
static GraphicsDriverModule* gfxcard_find_driver();


/** public **/

DFBResult gfxcard_initialize()
{
     DFBResult             ret;
     GraphicsDriverModule *driver;

     card = (GraphicsDevice*) DFBCALLOC( 1, sizeof(GraphicsDevice) );

     Scard = (GraphicsDeviceShared*) shcalloc( 1, sizeof(GraphicsDeviceShared) );

     /* fill generic driver info */
     gGetDriverInfo( &Scard->driver_info );

     /* fill generic device info */
     gGetDeviceInfo( &Scard->device_info );



     if (ioctl( fbdev->fd, FBIOGET_FSCREENINFO, &Scard->fix ) < 0) {
          PERRORMSG( "DirectFB/core/gfxcard: "
                     "Could not get fixed screen information!\n" );
          DFBFREE( card );
          card = NULL;
          return DFB_INIT;
     }

     card->framebuffer.length = Scard->fix.smem_len;
     card->framebuffer.base = mmap( NULL, Scard->fix.smem_len,
                                    PROT_READ | PROT_WRITE, MAP_SHARED,
                                    fbdev->fd, 0 );
     if ((int)(card->framebuffer.base) == -1) {
          PERRORMSG( "DirectFB/core/gfxcard: "
                     "Could not mmap the framebuffer!\n");
          DFBFREE( card );
          card = NULL;
          return DFB_INIT;
     }

     memset( card->framebuffer.base, 0, Scard->fix.smem_len );

     /* load driver */
     driver = gfxcard_find_driver();
     if (driver) {
          card->driver_data = DFBCALLOC( 1,
                                         Scard->driver_info.driver_data_size );

          ret = driver->InitDriver( card, &card->funcs, card->driver_data );
          if (ret) {
               munmap( card->framebuffer.base, card->framebuffer.length );
               DFBFREE( card->driver_data );
               DFBFREE( driver );
               DFBFREE( card );
               card = NULL;
               return ret;
          }

          Scard->device_data = shcalloc( 1,
                                         Scard->driver_info.device_data_size );

          ret = driver->InitDevice( card, &Scard->device_info,
                                    card->driver_data, Scard->device_data );
          if (ret) {
               driver->CloseDriver( card, card->driver_data );
               munmap( card->framebuffer.base, card->framebuffer.length );
               shmfree( Scard->device_data );
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

     Scard->surface_manager = surfacemanager_create( card->framebuffer.length,
                card->shared->device_info.limits.surface_byteoffset_alignment,
                card->shared->device_info.limits.surface_pixelpitch_alignment );

     skirmish_init( &Scard->lock );

#ifndef FUSION_FAKE
     arena_add_shared_field( dfb_core->arena, Scard, "Scard" );
#endif

     return DFB_OK;
}

#ifndef FUSION_FAKE
DFBResult gfxcard_join()
{
     DFBResult             ret;
     GraphicsDriverModule *driver;

     card = (GraphicsDevice*)DFBCALLOC( 1, sizeof(GraphicsDevice) );

     arena_get_shared_field( dfb_core->arena, (void**) &Scard, "Scard" );

     card->framebuffer.length = Scard->fix.smem_len;
     card->framebuffer.base = mmap( NULL, Scard->fix.smem_len,
                                    PROT_READ | PROT_WRITE, MAP_SHARED,
                                    fbdev->fd, 0 );
     if ((int)(card->framebuffer.base) == -1) {
          PERRORMSG( "DirectFB/core/gfxcard: "
                     "Could not mmap the framebuffer!\n");
         DFBFREE( card );
         card = NULL;
         return DFB_INIT;
     }

     /* load driver, FIXME: do not probe */
     driver = gfxcard_find_driver();
     if (driver) {
          card->driver_data = DFBCALLOC( 1,
                                         Scard->driver_info.driver_data_size );

          ret = driver->InitDriver( card, &card->funcs, card->driver_data );
          if (ret) {
               munmap( card->framebuffer.base, card->framebuffer.length );
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

DFBResult gfxcard_shutdown()
{
     gfxcard_sync();

     if (card->driver) {
          card->driver->CloseDevice( card,
                                     card->driver_data, card->device_data );
          card->driver->CloseDriver( card, card->driver_data );

          shmfree( card->device_data );
          DFBFREE( card->driver_data );
          DFBFREE( card->driver );
     }

     munmap( (char*)card->framebuffer.base, card->framebuffer.length );

     skirmish_destroy( &Scard->lock );

     shmfree( Scard );

     DFBFREE( card );
     card = NULL;

     return DFB_OK;
}

#ifndef FUSION_FAKE
DFBResult gfxcard_leave()
{
     gfxcard_sync();

     if (card->driver) {
          card->driver->CloseDriver( card, card->driver_data );

          DFBFREE( card->driver_data );
          DFBFREE( card->driver );
     }

     munmap( (char*)card->framebuffer.base, card->framebuffer.length );

     DFBFREE( card );
     card = NULL;

     return DFB_OK;
}
#endif


DFBResult gfxcard_init_layers()
{
     if (card->driver && card->driver->InitLayers)
          card->driver->InitLayers( card->driver_data, card->device_data );

     return DFB_OK;
}

/*
 * This function returns non zero if acceleration is available
 * for the specific function using the given state.
 */
int gfxcard_state_check( CardState *state, DFBAccelerationMask accel )
{
     /*
      * If there's no CheckState function there's no acceleration at all.
      */
     if (!card->funcs.CheckState)
          return 0;

     /* If destination has been changed... */
     if (state->modified & SMF_DESTINATION) {
          /* ...force rechecking for all functions. */
          state->checked = 0;
          
          /* Debug check */
          if (!state->destination) {
               BUG("state check: no destination");
               return 0;
          }

          /*
           * If policy is 'system only' there's no acceleration available.
           */
          if (state->destination->back_buffer->policy == CSP_SYSTEMONLY) {
               /* unset 'destination modified' bit */
               state->modified &= ~SMF_DESTINATION;

               /* clear 'accelerated functions' */
               state->accel = 0;

               /* return immediately */
               return 0;
          }
     }

     /* If source has been changed... */
     if (state->modified & SMF_SOURCE) {
          /* ...force rechecking for all blitting functions. */
          state->checked &= 0xFFFF;
          
          /* Debug check */
          if (!state->source  &&  DFB_BLITTING_FUNCTION( accel )) {
               BUG("state check: no source");
               return 0;
          }

          /*
           * If policy is 'system only' there's no accelerated blitting
           * available.
           */
          if (state->source->front_buffer->policy == CSP_SYSTEMONLY) {
               /* unset 'destination modified' bit */
               state->modified &= ~SMF_SOURCE;
              
               /* clear 'accelerated blitting functions' */
               state->accel &= 0xFFFF;

               /* return if blitting function was requested */
               if (DFB_BLITTING_FUNCTION( accel ))
                    return 0;
          }
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
int gfxcard_state_acquire( CardState *state, DFBAccelerationMask accel )
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
     surfacemanager_lock( card->shared->surface_manager );

     /* if blitting... */
     if (DFB_BLITTING_FUNCTION( accel )) {
          /* ...lock source for reading */
          if (surface_hardware_lock( state->source, DSLF_READ, 1 )) {
               surfacemanager_unlock( card->shared->surface_manager );
               return 0;
          }

          state->source_locked = 1;
     }
     else
          state->source_locked = 0;

     /* lock destination */
     if (surface_hardware_lock( state->destination, lock_flags, 0 )) {
          if (state->source_locked)
               surface_unlock( state->source, 1 );

          surfacemanager_unlock( card->shared->surface_manager );
          return 0;
     }

     /* unlock surface manager */
     surfacemanager_unlock( card->shared->surface_manager );

     /* synchronize card access */
     skirmish_prevail( &Scard->lock );

     /* if we are switching to another state... */
     if (state != Scard->state) {
          /* ...set all modification bits and clear 'set functions' */
          state->modified |= SMF_ALL;
          state->set       = 0;

          Scard->state = state;
     }

     /*
      * If function hasn't been set or state is modified call the driver
      * function to propagate the state changes.
      */
     if (!(state->set & accel) || state->modified)
          card->funcs.SetState( card->driver_data, card->device_data,
                                &card->funcs, state, accel );

     return 1;
}

/*
 * Unlock destination and possibly the source.
 */
void gfxcard_state_release( CardState *state )
{
     surface_unlock( state->destination, 0 );

     if (state->source_locked)
          surface_unlock( state->source, 1 );

     skirmish_dismiss( &Scard->lock );
}

/** DRAWING FUNCTIONS **/

void gfxcard_fillrectangle( DFBRectangle *rect, CardState *state )
{
     state_lock( state );

     if (gfxcard_state_check( state, DFXL_FILLRECTANGLE ) &&
         gfxcard_state_acquire( state, DFXL_FILLRECTANGLE )) {
          if ((Scard->device_info.caps.flags & CCF_CLIPPING) ||
              clip_rectangle( &state->clip, rect )) {
               card->funcs.FillRectangle( card->driver_data,
                                          card->device_data, rect );
          }
          gfxcard_state_release( state );
     }
     else {
          if (clip_rectangle( &state->clip, rect ) &&
              gAquire( state, DFXL_FILLRECTANGLE )) {
               gFillRectangle( rect );
               gRelease( state );
          }
     }

     state_unlock( state );
}

void gfxcard_drawrectangle( DFBRectangle *rect, CardState *state )
{
     state_lock( state );

     if (gfxcard_state_check( state, DFXL_DRAWRECTANGLE ) &&
         gfxcard_state_acquire( state, DFXL_DRAWRECTANGLE )) {
          if (Scard->device_info.caps.flags & CCF_CLIPPING  ||
              clip_rectangle( &state->clip, rect )) {
               /* FIXME: correct clipping like below */
               card->funcs.DrawRectangle( card->driver_data,
                                          card->device_data, rect );
          }
          gfxcard_state_release( state );
     }
     else {
          unsigned int edges = clip_rectangle (&state->clip, rect);

          if (edges) {
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

     state_unlock( state );
}

void gfxcard_drawlines( DFBRegion *lines, int num_lines, CardState *state )
{
     int i;

     state_lock( state );

     if (gfxcard_state_check( state, DFXL_DRAWLINE ) &&
         gfxcard_state_acquire( state, DFXL_DRAWLINE )) {
          if (Scard->device_info.caps.flags & CCF_CLIPPING)
               for (i=0; i<num_lines; i++)
                    card->funcs.DrawLine( card->driver_data,
                                          card->device_data, &lines[i] );
          else
               for (i=0; i<num_lines; i++) {
                    if (clip_line( &state->clip, &lines[i] ))
                         card->funcs.DrawLine( card->driver_data,
                                               card->device_data, &lines[i] );
               }

          gfxcard_state_release( state );
     }
     else {
          if (gAquire( state, DFXL_DRAWLINE )) {
               for (i=0; i<num_lines; i++) {
                    if (clip_line( &state->clip, &lines[i] ))
                         gDrawLine( &lines[i] );
               }

               gRelease( state );
          }
     }

     state_unlock( state );
}


typedef struct {
   int xi;
   int xf;
   int mi;
   int mf;
   int _2dy;
} DDA;

#define ABS(x) ((x) < 0 ? -(x) : (x))

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


void gfxcard_filltriangle( DFBTriangle *tri, CardState *state )
{
     state_lock( state );

     /* if hardware has clipping try directly accelerated triangle filling */
     if ((Scard->device_info.caps.flags & CCF_CLIPPING) &&
          gfxcard_state_check( state, DFXL_FILLTRIANGLE ) &&
          gfxcard_state_acquire( state, DFXL_FILLTRIANGLE ))
     {
          card->funcs.FillTriangle( card->driver_data,
                                    card->device_data, tri );
          gfxcard_state_release( state );
     }
     else {
          /* otherwise use the spanline rasterizer (fill_tri)
             and fill the triangle using a rectangle for each spanline */

          sort_triangle( tri );

          if (tri->y3 - tri->y1 > 0) {
               /* try hardware accelerated rectangle filling */
               if (gfxcard_state_check( state, DFXL_FILLRECTANGLE ) &&
                   gfxcard_state_acquire( state, DFXL_FILLRECTANGLE ))
               {
                    fill_tri( tri, state, 1 );

                    gfxcard_state_release( state );
               }
               else if (gAquire( state, DFXL_FILLTRIANGLE )) {
                    fill_tri( tri, state, 0 );

                    gRelease( state );
               }
          }
     }

     state_unlock( state );
}


void gfxcard_blit( DFBRectangle *rect, int dx, int dy, CardState *state )
{
     state_lock( state );

     if (!clip_blit_precheck( &state->clip, rect->w, rect->h, dx, dy )) {
          /* no work at all */
          state_unlock( state );
          return;
     }

     if (gfxcard_state_check( state, DFXL_BLIT ) &&
         gfxcard_state_acquire( state, DFXL_BLIT )) {
          if (!(Scard->device_info.caps.flags & CCF_CLIPPING))
               clip_blit( &state->clip, rect, &dx, &dy );

          card->funcs.Blit( card->driver_data, card->device_data,
                            rect, dx, dy );
          gfxcard_state_release( state );
     }
     else {
          if (gAquire( state, DFXL_BLIT )) {
               clip_blit( &state->clip, rect, &dx, &dy );
               gBlit( rect, dx, dy );
               gRelease( state );
          }
     }

     state_unlock( state );
}

void gfxcard_stretchblit( DFBRectangle *srect, DFBRectangle *drect,
                          CardState *state )
{
     state_lock( state );

     if (!clip_blit_precheck( &state->clip, drect->w, drect->h,
                              drect->x, drect->y ))
     {
          state_unlock( state );
          return;
     }

     if (gfxcard_state_check( state, DFXL_STRETCHBLIT ) &&
         gfxcard_state_acquire( state, DFXL_STRETCHBLIT )) {
          if (!(Scard->device_info.caps.flags & CCF_CLIPPING))
               clip_stretchblit( &state->clip, srect, drect );

          card->funcs.StretchBlit( card->driver_data,
                                   card->device_data, srect, drect );
          gfxcard_state_release( state );
     }
     else {
          if (gAquire( state, DFXL_STRETCHBLIT )) {
               clip_stretchblit( &state->clip, srect, drect );
               gStretchBlit( srect, drect );
               gRelease( state );
          }
     }

     state_unlock( state );
}

#define FONT_BLITTINGFLAGS   (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_COLORIZE)

void gfxcard_drawstring( const __u8 *text, int bytes,
                         int x, int y,
                         CoreFont *font, CardState *state )
{
     CoreGlyphData *data;
     DFBRectangle   rect;

     unichar prev = 0;
     unichar current;

     int hw_clipping = (Scard->device_info.caps.flags & CCF_CLIPPING);
     int kerning;
     int offset;
     int blit = 0;

     int restore_blittingflags = 0;
     DFBSurfaceBlittingFlags original_blittingflags = 0;

     state_lock( state );
     font_lock( font );

     /* simple prechecks */
     if (y + font->height <= state->clip.y1) {
          font_unlock( font );
          state_unlock( state );
          return;
     }
     if (y > state->clip.y2) {
          font_unlock( font );
          state_unlock( state );
          return;
     }

     state_set_source( state, NULL );

     if (state->blittingflags != FONT_BLITTINGFLAGS) {
          restore_blittingflags = 1;
          original_blittingflags = state->blittingflags;
          state->blittingflags = FONT_BLITTINGFLAGS;
          state->modified |= SMF_BLITTING_FLAGS;
     }

     for (offset = 0; offset < bytes; offset += utf8_skip[text[offset]]) {

          current = utf8_get_char (&text[offset]);

          if (font_get_glyph_data (font, current, &data) == DFB_OK) {
               if (prev && font->GetKerning &&
                   (* font->GetKerning) (font, prev, current, &kerning) == DFB_OK) {
                    x += kerning;
               }

               rect.x = data->start;
               rect.y = 0;
               rect.w = data->width;
               rect.h = data->height;

               if (rect.w > 0) {
                    int xx = x + data->left;
                    int yy = y + data->top;

                    if (state->source != data->surface) {
                         switch (blit) {
                              case 1:
                                   gfxcard_state_release( state );
                                   break;
                              case 2:
                                   gRelease( state );
                                   break;
                              default:
                                   break;
                         }
                         state->source = data->surface;
                         state->modified |= SMF_SOURCE;

                         if (gfxcard_state_check( state, DFXL_BLIT ) &&
                             gfxcard_state_acquire( state, DFXL_BLIT ))
                              blit = 1;
                         else if (gAquire( state, DFXL_BLIT ))
                              blit = 2;
                         else
                              blit = 0;
                    }

                    if (clip_blit_precheck( &state->clip,
                                            rect.w, rect.h, xx, yy )) {
                         switch (blit) {
                              case 1:
                                   if (!hw_clipping)
                                        clip_blit( &state->clip, &rect, &xx, &yy );
                                   card->funcs.Blit( card->driver_data,
                                                     card->device_data,
                                                     &rect, xx, yy );
                                   break;
                              case 2:
                                   clip_blit( &state->clip, &rect, &xx, &yy );
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
               gfxcard_state_release( state );
               break;
          case 2:
               gRelease( state );
               break;
          default:
               break;
     }

     state->source = NULL;
     state->modified |= SMF_SOURCE;

     if (restore_blittingflags) {
          state->blittingflags = original_blittingflags;
          state->modified |= SMF_BLITTING_FLAGS;
     }

     font_unlock( font );
     state_unlock( state );
}

volatile void *gfxcard_map_mmio( GraphicsDevice *device,
                                 unsigned int    offset,
                                 int             length )
{
     void *addr;

     if (length < 0)
          length = device->shared->fix.mmio_len;

     addr = mmap( NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED,
                  fbdev->fd, device->shared->fix.smem_len + offset );
     if ((int)(addr) == -1) {
          PERRORMSG( "DirectFB/core/gfxcard: Could not mmap MMIO region "
                     "(offset %d, length %d)!\n", offset, length );
          return NULL;
     }

     return (volatile void*) addr;
}

void gfxcard_unmap_mmio( GraphicsDevice *device,
                         volatile void  *addr,
                         int             length )
{
     if (length < 0)
          length = device->shared->fix.mmio_len;

     if (munmap( (void*) addr, length ) < 0)
          PERRORMSG( "DirectFB/core/gfxcard: Could not unmap MMIO region "
                     "at %p (length %d)!\n", addr, length );
}

int gfxcard_get_accelerator( GraphicsDevice *device )
{
     return device->shared->fix.accel;
}

void gfxcard_sync()
{
     if (card->funcs.EngineSync)
          card->funcs.EngineSync( card->driver_data, card->device_data );
}

void gfxcard_flush_texture_cache()
{
     if (card->funcs.FlushTextureCache)
          card->funcs.FlushTextureCache( card->driver_data, card->device_data );
}

void gfxcard_after_set_var()
{
     if (card->funcs.AfterSetVar)
          card->funcs.AfterSetVar( card->driver_data, card->device_data );
}

DFBResult
gfxcard_adjust_heap_offset( unsigned int offset )
{
     return surfacemanager_adjust_heap_offset( Scard->surface_manager, offset );
}

SurfaceManager *
gfxcard_surface_manager()
{
     return Scard->surface_manager;
}

CardCapabilities
gfxcard_capabilities()
{
     return Scard->device_info.caps;
}

int
gfxcard_reserve_memory( GraphicsDevice *device, unsigned int size )
{
     if (device->shared->surface_manager)
          return -1;

     if (device->framebuffer.length < size)
          return -1;

     device->framebuffer.length -= size;
     
     return device->framebuffer.length;
}

unsigned long
gfxcard_memory_physical( unsigned int offset )
{
     return Scard->fix.smem_start + offset;
}

void *
gfxcard_memory_virtual( unsigned int offset )
{
     return (void*)((__u8*)(card->framebuffer.base) + offset);
}

unsigned int
gfxcard_memory_length()
{
     return card->framebuffer.length;
}

/** internal **/

#define GFXCARD_DRIVER_LINK(driver,fp,fn,handle,name) \
     (driver)->##fp = dlsym( handle, fn );                            \
     if (!(driver)->##fp) {                                           \
          DLERRORMSG( "DirectFB/core/gfxcard: "                       \
                      "Could not link '%s' of `%s'!\n", fn, name );   \
          return MODULE_REJECTED;                                     \
     }

static CoreModuleLoadResult gfxcard_driver_handle_func( void *handle,
                                                        char *name,
                                                        void *ctx )
{
     GraphicsDriverModule *driver = (GraphicsDriverModule*)ctx;

     GFXCARD_DRIVER_LINK( driver, CloseDevice,
                          "driver_close_device", handle, name );

     GFXCARD_DRIVER_LINK( driver, CloseDriver,
                          "driver_close_driver", handle, name );

     GFXCARD_DRIVER_LINK( driver, GetAbiVersion,
                          "driver_get_abi_version", handle, name );

     GFXCARD_DRIVER_LINK( driver, GetDriverInfo,
                          "driver_get_info", handle, name );

     GFXCARD_DRIVER_LINK( driver, InitDevice,
                          "driver_init_device", handle, name );

     GFXCARD_DRIVER_LINK( driver, InitDriver,
                          "driver_init_driver", handle, name );

     GFXCARD_DRIVER_LINK( driver, InitLayers,
                          "driver_init_layers", handle, name );

     GFXCARD_DRIVER_LINK( driver, Probe,
                          "driver_probe", handle, name );

     if (driver->GetAbiVersion() != DFB_GRAPHICS_DRIVER_ABI_VERSION) {
          ERRORMSG( "DirectFB/core/gfxcard: '%s' "
                    "was built for ABI version %d, but %d is required!\n", name,
                    driver->GetAbiVersion(), DFB_GRAPHICS_DRIVER_ABI_VERSION );

          return MODULE_REJECTED;
     }

     if (driver->Probe( card )) {
          driver->GetDriverInfo( card, &card->shared->driver_info );
          return MODULE_LOADED_STOP;
     }

     return MODULE_REJECTED;
}

/*
 * loads/probes/unloads one driver module after another until a suitable
 * driver is found and returns its symlinked functions
 */
static GraphicsDriverModule* gfxcard_find_driver()
{
     GraphicsDriverModule *driver;
     char                 *driver_dir = MODULEDIR"/gfxdrivers";

     if (dfb_config->software_only)
          return NULL;

     driver = DFBCALLOC( 1, sizeof(GraphicsDriverModule) );

     if (core_load_modules( driver_dir,
                            gfxcard_driver_handle_func, (void*)driver )) {
          DFBFREE( driver );
          driver = NULL;
     }

     return driver;
}

