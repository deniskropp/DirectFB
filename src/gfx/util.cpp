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


#include <directfb.h>    // include here to prevent it being included indirectly causing nested extern "C"

extern "C" {
#include <direct/util.h>

#include <core/state.h>

#include <gfx/util.h>

#include <misc/util.h>
}

#include <direct/TLSObject.h>

#include <core/CoreSurface.h>


class StateClient {
public:
     CardState               state;
     CoreGraphicsStateClient client;

     StateClient()
     {
          DFBResult ret;

          /* Initialise the graphics state used for rendering */
          dfb_state_init( &state, core_dfb );

          /* Create a client to use the task manager if enabled */
          ret = CoreGraphicsStateClient_Init( &client, &state );
          if (ret) {
               dfb_state_destroy( &state );
               return;
          }
     }

     ~StateClient()
     {
          CoreGraphicsStateClient_Deinit( &client );
          dfb_state_destroy( &state );
     }


     static StateClient *create( void *ctx, void *params )
     {
          return new StateClient();
     }

     static void destroy( void *ctx, StateClient *client )
     {
          delete client;
     }
};


static Direct::TLSObject2<StateClient> state_client_tls;


extern "C" {


void
dfb_gfx_cleanup()
{
     state_client_tls.DeleteAll();
}

void
dfb_gfx_copy( CoreSurface *source, CoreSurface *destination, const DFBRectangle *rect )
{
     D_ASSERT( !dfb_config->task_manager );

     dfb_gfx_copy_stereo( source, DSSE_LEFT, destination, DSSE_LEFT, rect, rect ? rect->x : 0, rect ? rect->y : 0, false );
}

void
dfb_gfx_copy_to( CoreSurface        *source,
                 CoreSurface        *destination,
                 const DFBRectangle *rect,
                 int                 x,
                 int                 y,
                 bool                from_back )
{
     D_ASSERT( !dfb_config->task_manager );

     dfb_gfx_copy_stereo( source, DSSE_LEFT, destination, DSSE_LEFT, rect, x, y, from_back );
}

void
dfb_gfx_copy_stereo( CoreSurface         *source,
                     DFBSurfaceStereoEye  source_eye,
                     CoreSurface         *destination,
                     DFBSurfaceStereoEye  destination_eye,
                     const DFBRectangle  *rect,
                     int                  x,
                     int                  y,
                     bool                 from_back )
{
     DFBRectangle sourcerect = { 0, 0, source->config.size.w, source->config.size.h };

     StateClient *client = state_client_tls.Get();

     D_FLAGS_SET( client->state.modified, SMF_CLIP | SMF_SOURCE | SMF_DESTINATION | SMF_FROM | SMF_TO );

     client->state.clip.x2     = destination->config.size.w - 1;
     client->state.clip.y2     = destination->config.size.h - 1;
     client->state.source      = source;
     client->state.destination = destination;
     client->state.from        = from_back ? CSBR_BACK : CSBR_FRONT;
     client->state.from_eye    = source_eye;
     client->state.to          = CSBR_BACK;
     client->state.to_eye      = destination_eye;

     if (rect) {
          if (dfb_rectangle_intersect( &sourcerect, rect )) {
               DFBPoint point = { x + sourcerect.x - rect->x, y + sourcerect.y - rect->y };

               CoreGraphicsStateClient_Blit( &client->client, &sourcerect, &point, 1 );
          }
     }
     else {
          DFBPoint point = { x, y };

          CoreGraphicsStateClient_Blit( &client->client, &sourcerect, &point, 1 );
     }

     CoreGraphicsStateClient_Flush( &client->client, 0 );

     /* Signal end of sequence. */
     dfb_state_stop_drawing( &client->state );

     client->state.destination = NULL;
     client->state.source      = NULL;
}

void
dfb_gfx_clear( CoreSurface *surface, CoreSurfaceBufferRole role )
{
     DFBRectangle rect = { 0, 0, surface->config.size.w, surface->config.size.h };

     StateClient *client = state_client_tls.Get();

     D_FLAGS_SET( client->state.modified, SMF_CLIP | SMF_COLOR | SMF_DESTINATION | SMF_TO );

     client->state.clip.x2     = surface->config.size.w - 1;
     client->state.clip.y2     = surface->config.size.h - 1;
     client->state.destination = surface;
     client->state.to          = role;
     client->state.to_eye      = DSSE_LEFT;
     client->state.color.a     = 0;
     client->state.color.r     = 0;
     client->state.color.g     = 0;
     client->state.color.b     = 0;
     client->state.color_index = 0;

     CoreGraphicsStateClient_FillRectangles( &client->client, &rect, 1 );

     CoreGraphicsStateClient_Flush( &client->client, 0 );

     /* Signal end of sequence. */
     dfb_state_stop_drawing( &client->state );

     client->state.destination = NULL;
}

void
dfb_gfx_stretch_to( CoreSurface *source, CoreSurface *destination,
                    const DFBRectangle *srect, const DFBRectangle *drect, bool from_back )
{
     D_ASSERT( !dfb_config->task_manager );

     dfb_gfx_stretch_stereo( source, DSSE_LEFT, destination, DSSE_LEFT, srect, drect, from_back );
}

void dfb_gfx_stretch_stereo( CoreSurface         *source,
                             DFBSurfaceStereoEye  source_eye,
                             CoreSurface         *destination,
                             DFBSurfaceStereoEye  destination_eye,
                             const DFBRectangle  *srect,
                             const DFBRectangle  *drect,
                             bool                 from_back )
{
     DFBRectangle sourcerect = { 0, 0, source->config.size.w, source->config.size.h };
     DFBRectangle destrect =   { 0, 0, destination->config.size.w, destination->config.size.h };

     D_ASSERT( !dfb_config->task_manager );

     if (srect) {
          if (!dfb_rectangle_intersect( &sourcerect, srect ))
               return;
     }

     if (drect) {
          if (!dfb_rectangle_intersect( &destrect, drect ))
               return;
     }

     StateClient *client = state_client_tls.Get();

     D_FLAGS_SET( client->state.modified, SMF_CLIP | SMF_SOURCE | SMF_DESTINATION | SMF_FROM | SMF_TO );

     client->state.clip.x2     = destination->config.size.w - 1;
     client->state.clip.y2     = destination->config.size.h - 1;
     client->state.source      = source;
     client->state.destination = destination;
     client->state.from        = from_back ? CSBR_BACK : CSBR_FRONT;
     client->state.from_eye    = source_eye;
     client->state.to          = CSBR_BACK;
     client->state.to_eye      = destination_eye;

     CoreGraphicsStateClient_StretchBlit( &client->client, &sourcerect, &destrect, 1 );

     CoreGraphicsStateClient_Flush( &client->client, 0 );

     /* Signal end of sequence. */
     dfb_state_stop_drawing( &client->state );

     client->state.destination = NULL;
     client->state.source      = NULL;
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
     D_ASSERT( !dfb_config->task_manager );

     dfb_gfx_copy_regions_stereo( source, from, DSSE_LEFT, destination, to, DSSE_LEFT, regions, num, x, y );
}

void
dfb_gfx_copy_regions_stereo( CoreSurface           *source,
                             CoreSurfaceBufferRole  from,
                             DFBSurfaceStereoEye    source_eye,
                             CoreSurface           *destination,
                             CoreSurfaceBufferRole  to,
                             DFBSurfaceStereoEye    destination_eye,
                             const DFBRegion       *regions,
                             unsigned int           num,
                             int                    x,
                             int                    y )
{
     unsigned int i, n = 0;
     DFBRectangle rect = { 0, 0, source->config.size.w, source->config.size.h };
     DFBRectangle rects[num];
     DFBPoint     points[num];

     D_ASSERT( !dfb_config->task_manager );

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
          StateClient *client = state_client_tls.Get();

          D_FLAGS_SET( client->state.modified, SMF_CLIP | SMF_SOURCE | SMF_DESTINATION | SMF_FROM | SMF_TO );

          client->state.clip.x2     = destination->config.size.w - 1;
          client->state.clip.y2     = destination->config.size.h - 1;
          client->state.source      = source;
          client->state.destination = destination;
          client->state.from        = from;
          client->state.from_eye    = source_eye;
          client->state.to          = to;
          client->state.to_eye      = destination_eye;

          CoreGraphicsStateClient_Blit( &client->client, rects, points, n );

          CoreGraphicsStateClient_Flush( &client->client, 0 );

          /* Signal end of sequence. */
          dfb_state_stop_drawing( &client->state );

          client->state.destination = NULL;
          client->state.source      = NULL;
     }
}

void
dfb_gfx_copy_regions_client( CoreSurface             *source,
                             CoreSurfaceBufferRole    from,
                             DFBSurfaceStereoEye      source_eye,
                             CoreSurface             *destination,
                             CoreSurfaceBufferRole    to,
                             DFBSurfaceStereoEye      destination_eye,
                             const DFBRegion         *regions,
                             unsigned int             num,
                             int                      x,
                             int                      y,
                             CoreGraphicsStateClient *client )
{
     unsigned int  i, n = 0;
     DFBRectangle  rect = { 0, 0, source->config.size.w, source->config.size.h };
     DFBRectangle  rects[num];
     DFBPoint      points[num];
     CardState    *state = client->state;
     CardState     backup;

     for (i=0; i<num; i++) {
          DFB_REGION_ASSERT( &regions[i] );

          rects[n] = DFB_RECTANGLE_INIT_FROM_REGION( &regions[i] );

          if (dfb_rectangle_intersect( &rects[n], &rect )) {
               points[n].x = x + rects[n].x;
               points[n].y = y + rects[n].y;

               n++;
          }
     }

     if (n > 0) {
          backup.clip          = state->clip;
          backup.source        = state->source;
          backup.destination   = state->destination;
          backup.from          = state->from;
          backup.from_eye      = state->from_eye;
          backup.to            = state->to;
          backup.to_eye        = state->to_eye;
          backup.blittingflags = state->blittingflags;


          D_FLAGS_SET( state->modified, SMF_CLIP | SMF_SOURCE | SMF_DESTINATION | SMF_FROM | SMF_TO | SMF_BLITTING_FLAGS );

          state->clip.x1     = 0;
          state->clip.y1     = 0;
          state->clip.x2     = destination->config.size.w - 1;
          state->clip.y2     = destination->config.size.h - 1;
          state->source      = source;
          state->destination = destination;
          state->from        = from;
          state->from_eye    = source_eye;
          state->to          = to;
          state->to_eye      = destination_eye;
          state->blittingflags = DSBLIT_NOFX;

          CoreGraphicsStateClient_Blit( client, rects, points, n );


          D_FLAGS_SET( state->modified, SMF_CLIP | SMF_SOURCE | SMF_DESTINATION | SMF_FROM | SMF_TO | SMF_BLITTING_FLAGS );

          state->clip          = backup.clip;
          state->source        = backup.source;
          state->destination   = backup.destination;
          state->from          = backup.from;
          state->from_eye      = backup.from_eye;
          state->to            = backup.to;
          state->to_eye        = backup.to_eye;
          state->blittingflags = backup.blittingflags;
     }
}

/*********************************************************************************************************************/

static void
back_to_front_copy( CoreSurface             *surface,
                    DFBSurfaceStereoEye      eye,
                    const DFBRegion         *region,
                    DFBSurfaceBlittingFlags  flags,
                    int                      rotation)
{
     DFBRectangle  rect;
     DFBPoint      point;
     StateClient  *client = state_client_tls.Get();


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

     point.x = rect.x;
     point.y = rect.y;

     if (rotation == 90) {
          point.x = rect.y;
          point.y = surface->config.size.w - rect.w - rect.x;

          D_FLAGS_SET( flags, DSBLIT_ROTATE90 );
     }
     else if (rotation == 180) {
          point.x = surface->config.size.w - rect.w - rect.x;
          point.y = surface->config.size.h - rect.h - rect.y;

          D_FLAGS_SET( flags, DSBLIT_ROTATE180 );
     }
     else if (rotation == 270) {
          point.x = surface->config.size.h - rect.h - rect.y;
          point.y = rect.x;

          D_FLAGS_SET( flags, DSBLIT_ROTATE270 );
     }

     D_FLAGS_SET( client->state.modified, SMF_CLIP | SMF_SOURCE | SMF_DESTINATION | SMF_FROM | SMF_TO );

     client->state.clip.x2       = surface->config.size.w - 1;
     client->state.clip.y2       = surface->config.size.h - 1;
     client->state.source        = surface;
     client->state.destination   = surface;
     client->state.from          = CSBR_BACK;
     client->state.from_eye      = eye;
     client->state.to            = CSBR_FRONT;
     client->state.to_eye        = eye;
     client->state.blittingflags = flags;

     CoreGraphicsStateClient_Blit( &client->client, &rect, &point, 1 );

     CoreGraphicsStateClient_Flush( &client->client, 0 );

     /* Signal end of sequence. */
     dfb_state_stop_drawing( &client->state );

     client->state.destination = NULL;
     client->state.source      = NULL;
}

void
dfb_back_to_front_copy( CoreSurface *surface, const DFBRegion *region )
{
     back_to_front_copy( surface, DSSE_LEFT, region, DSBLIT_NOFX, 0);
}

void
dfb_back_to_front_copy_rotation( CoreSurface *surface, const DFBRegion *region, int rotation )
{
     back_to_front_copy( surface, DSSE_LEFT, region, DSBLIT_NOFX, rotation );
}

void
dfb_back_to_front_copy_stereo( CoreSurface         *surface,
                               DFBSurfaceStereoEye  eyes,
                               const DFBRegion     *left_region,
                               const DFBRegion     *right_region,
                               int                  rotation )
{
     if (eyes & DSSE_LEFT)
          back_to_front_copy( surface, DSSE_LEFT, left_region, DSBLIT_NOFX, rotation );

     if (eyes & DSSE_RIGHT)
          back_to_front_copy( surface, DSSE_RIGHT, right_region, DSBLIT_NOFX, rotation );
}

/*********************************************************************************************************************/

void
dfb_clear_depth( CoreSurface *surface, const DFBRegion *region )
{
     D_ASSERT( !dfb_config->task_manager );

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

     cd_state.modified   |= SMF_CLIP | SMF_DESTINATION | SMF_TO;

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

void dfb_sort_trapezoid( DFBTrapezoid *trap )
{
     int temp;

     if (trap->y1 > trap->y2) {
          temp = trap->x1;
          trap->x1 = trap->x2;
          trap->x2 = temp;

          temp = trap->y1;
          trap->y1 = trap->y2;
          trap->y2 = temp;

          temp = trap->w1;
          trap->w1 = trap->w2;
          trap->w2 = temp;
     }
}


}

