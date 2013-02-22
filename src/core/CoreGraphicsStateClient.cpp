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

extern "C" {
#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>

#include <core/core.h>
#include <core/graphics_state.h>
#include <core/state.h>
#include <core/surface.h>

#include <fusion/conf.h>

#include <core/CoreGraphicsStateClient.h>
}

#include <direct/TLSObject.h>

#include <core/CoreDFB.h>
#include <core/CoreGraphicsState.h>
#include <core/Renderer.h>

D_DEBUG_DOMAIN( Core_GraphicsStateClient,       "Core/GfxState/Client",       "DirectFB Core Graphics State Client" );
D_DEBUG_DOMAIN( Core_GraphicsStateClient_Flush, "Core/GfxState/Client/Flush", "DirectFB Core Graphics State Client Flush" );

/**********************************************************************************************************************/

namespace DirectFB {

class ClientList {
public:
     ClientList()
     {
          direct_mutex_init( &lock );
     }

     ~ClientList()
     {
          direct_mutex_deinit( &lock );
     }

     void AddClient( CoreGraphicsStateClient *client )
     {
          direct_mutex_lock( &lock );

          clients.push_back( client );

          direct_mutex_unlock( &lock );
     }

     void RemoveClient( CoreGraphicsStateClient *client )
     {
          direct_mutex_lock( &lock );

          clients.remove( client );

          direct_mutex_unlock( &lock );
     }

     void FlushAll()
     {
          direct_mutex_lock( &lock );

          for (std::list<CoreGraphicsStateClient*>::const_iterator it = clients.begin(); it != clients.end(); ++it)
               CoreGraphicsStateClient_Flush( *it );

          direct_mutex_unlock( &lock );
     }

     void FlushAllDst( CoreSurface *surface )
     {
          direct_mutex_lock( &lock );

          for (std::list<CoreGraphicsStateClient*>::const_iterator it = clients.begin(); it != clients.end(); ++it) {
               if ((*it)->state->destination == surface)
                    CoreGraphicsStateClient_Flush( *it );
          }

          direct_mutex_unlock( &lock );
     }

private:
     DirectMutex                         lock;
     std::list<CoreGraphicsStateClient*> clients;
};

}

static DirectFB::ClientList client_list;




class StateHolder
{
     friend class Direct::TLSObject2<StateHolder>;

     static StateHolder *create( void *ctx, void *params )
     {
          D_DEBUG_AT( Core_GraphicsStateClient_Flush, "%s()\n", __FUNCTION__ );

          return new StateHolder();
     }

     static void destroy( void *ctx, StateHolder *holder )
     {
          D_DEBUG_AT( Core_GraphicsStateClient_Flush, "%s()\n", __FUNCTION__ );

          delete holder;
     }

     StateHolder()
          :
          client( NULL )
     {

     }

     CoreGraphicsStateClient *client;

public:
     void set( CoreGraphicsStateClient *client )
     {
          D_DEBUG_AT( Core_GraphicsStateClient_Flush, "%s( %p )\n", __FUNCTION__, client );

          if (client != this->client) {
               if (this->client) {
                    D_DEBUG_AT( Core_GraphicsStateClient_Flush, "  -> flushing previous (%p)\n", this->client );

                    CoreGraphicsStateClient_Flush( this->client );
               }

               this->client = client;
          }
     }

     void leave( CoreGraphicsStateClient *client )
     {
          D_DEBUG_AT( Core_GraphicsStateClient_Flush, "%s( %p )\n", __FUNCTION__, client );

          if (client == this->client) {
               D_DEBUG_AT( Core_GraphicsStateClient_Flush, "  -> setting NULL\n" );

               this->client = NULL;
          }
     }
};


static Direct::TLSObject2<StateHolder> state_holder_tls;










extern "C" {

DFBResult
CoreGraphicsStateClient_Init( CoreGraphicsStateClient *client,
                              CardState               *state )
{
     DFBResult ret;

     D_DEBUG_AT( Core_GraphicsStateClient, "%s( client %p, state %p )\n", __FUNCTION__, client, state );

     D_ASSERT( client != NULL );
     D_MAGIC_ASSERT( state, CardState );
     D_MAGIC_ASSERT( state->core, CoreDFB );

     client->magic    = 0;
     client->core     = state->core;
     client->state    = state;
     client->renderer = NULL;

     ret = CoreDFB_CreateState( state->core, &client->gfx_state );
     if (ret)
          return ret;

     D_DEBUG_AT( Core_GraphicsStateClient, "  -> gfxstate id 0x%x\n", client->gfx_state->object.ref.multi.id );

     if (dfb_config->task_manager) {
          if (dfb_config->call_nodirect) {
               if (direct_thread_get_tid( direct_thread_self() ) == fusion_dispatcher_tid(state->core->world)) {
                    client->renderer = new DirectFB::Renderer( client->state );
               }
          }
          else if (!fusion_config->secure_fusion || dfb_core_is_master( client->core ))
               client->renderer = new DirectFB::Renderer( client->state );
     }

     client->requestor = new DirectFB::IGraphicsState_Requestor( core_dfb, client->gfx_state );

     D_MAGIC_SET( client, CoreGraphicsStateClient );

     client_list.AddClient( client );

     return DFB_OK;
}

void
CoreGraphicsStateClient_Deinit( CoreGraphicsStateClient *client )
{
     D_DEBUG_AT( Core_GraphicsStateClient, "%s( client %p, gfxstate id 0x%x )\n", __FUNCTION__, client, client->gfx_state->object.ref.multi.id );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );

     StateHolder *holder = state_holder_tls.Get( NULL );
     D_ASSERT( holder != NULL );

     holder->leave( client );

     D_DEBUG_AT( Core_GraphicsStateClient_Flush, "  -> deinit, flushing (%p)\n", client );

     CoreGraphicsStateClient_Flush( client );

     if (client->renderer)
          delete client->renderer;

     delete (DirectFB::IGraphicsState_Requestor *) client->requestor;

     dfb_graphics_state_unref( client->gfx_state );

     client_list.RemoveClient( client );

     D_MAGIC_CLEAR( client );
}

#define STATE_HOLDER_SET(client)                                      \
     do {                                                             \
          StateHolder *holder = state_holder_tls.Get( NULL );         \
          D_ASSERT( holder != NULL );                                 \
                                                                      \
          holder->set( client );                                      \
     } while (0)

void
CoreGraphicsStateClient_Flush( CoreGraphicsStateClient *client )
{
     D_DEBUG_AT( Core_GraphicsStateClient_Flush, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );

     if (client->renderer) {
          D_DEBUG_AT( Core_GraphicsStateClient_Flush, "  -> flush renderer\n" );

          client->renderer->Flush();
     }
     else {
          if (!dfb_config->call_nodirect && (dfb_core_is_master( client->core ) || !fusion_config->secure_fusion)) {
               D_DEBUG_AT( Core_GraphicsStateClient_Flush, "  -> flush gfxcard\n" );

               dfb_gfxcard_flush();
          }
          else {
               DirectFB::IGraphicsState_Requestor *requestor = (DirectFB::IGraphicsState_Requestor*) client->requestor;

               D_DEBUG_AT( Core_GraphicsStateClient_Flush, "  -> flush via requestor\n" );

               requestor->Flush();
          }
     }
}

void
CoreGraphicsStateClient_FlushAll()
{
     D_DEBUG_AT( Core_GraphicsStateClient_Flush, "%s()\n", __FUNCTION__ );

     client_list.FlushAll();
}

void
CoreGraphicsStateClient_FlushCurrent()
{
     D_DEBUG_AT( Core_GraphicsStateClient_Flush, "%s()\n", __FUNCTION__ );

     StateHolder *holder = state_holder_tls.Get( NULL );
     D_ASSERT( holder != NULL );

     holder->set( NULL );

     // TODO: check if using renderer at all to avoid TLS creation
     DirectFB::Renderer::FlushCurrent();
}

DFBResult
CoreGraphicsStateClient_ReleaseSource( CoreGraphicsStateClient *client )
{
     D_DEBUG_AT( Core_GraphicsStateClient, "%s( %p )\n", __FUNCTION__, client );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );

     DirectFB::IGraphicsState_Requestor *requestor = (DirectFB::IGraphicsState_Requestor*) client->requestor;

     return requestor->ReleaseSource();
}

DFBResult
CoreGraphicsStateClient_SetColorAndIndex( CoreGraphicsStateClient *client,
                                          const DFBColor          *color,
                                          u32                      index )
{
     D_DEBUG_AT( Core_GraphicsStateClient, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );

     DirectFB::IGraphicsState_Requestor *requestor = (DirectFB::IGraphicsState_Requestor*) client->requestor;

     return requestor->SetColorAndIndex( color, index );
}

DFBResult
CoreGraphicsStateClient_SetState( CoreGraphicsStateClient *client,
                                  CardState               *state,
                                  StateModificationFlags   flags )
{
     DFBResult ret;

     D_DEBUG_AT( Core_GraphicsStateClient, "%s( client %p, state %p, flags 0x%08x )\n", __FUNCTION__, client, state, flags );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     D_MAGIC_ASSERT( state, CardState );

     DirectFB::IGraphicsState_Requestor *requestor = (DirectFB::IGraphicsState_Requestor*) client->requestor;

     if (flags & SMF_DRAWING_FLAGS) {
          ret = requestor->SetDrawingFlags( state->drawingflags );
          if (ret)
               return ret;
     }

     if (flags & SMF_BLITTING_FLAGS) {
          ret = requestor->SetBlittingFlags( state->blittingflags );
          if (ret)
               return ret;
     }

     if (flags & SMF_CLIP) {
          ret = requestor->SetClip( &state->clip );
          if (ret)
               return ret;
     }

     if (flags & SMF_COLOR) {
          ret = requestor->SetColor( &state->color );
          if (ret)
               return ret;
     }

     if (flags & SMF_SRC_BLEND) {
          ret = requestor->SetSrcBlend( state->src_blend );
          if (ret)
               return ret;
     }

     if (flags & SMF_DST_BLEND) {
          ret = requestor->SetDstBlend( state->dst_blend );
          if (ret)
               return ret;
     }

     if (flags & SMF_SRC_COLORKEY) {
          ret = requestor->SetSrcColorKey( state->src_colorkey );
          if (ret)
               return ret;
     }

     if (flags & SMF_DST_COLORKEY) {
          ret = requestor->SetDstColorKey( state->dst_colorkey );
          if (ret)
               return ret;
     }

     if (flags & SMF_DESTINATION) {
          D_DEBUG_AT( Core_GraphicsStateClient, "  -> DESTINATION %p [%d]\n", state->destination, state->destination->object.id );

          ret = requestor->SetDestination( state->destination );
          if (ret)
               return ret;
     }

     if (flags & SMF_SOURCE) {
          ret = requestor->SetSource( state->source );
          if (ret)
               return ret;
     }

     if (flags & SMF_SOURCE_MASK) {
          ret = requestor->SetSourceMask( state->source_mask );
          if (ret)
               return ret;
     }

     if (flags & SMF_SOURCE_MASK_VALS) {
          ret = requestor->SetSourceMaskVals( &state->src_mask_offset, state->src_mask_flags );
          if (ret)
               return ret;
     }

     if (flags & SMF_INDEX_TRANSLATION) {
          ret = requestor->SetIndexTranslation( state->index_translation, state->num_translation );
          if (ret)
               return ret;
     }

     if (flags & SMF_COLORKEY) {
          ret = requestor->SetColorKey( &state->colorkey );
          if (ret)
               return ret;
     }

     if (flags & SMF_RENDER_OPTIONS) {
          ret = requestor->SetRenderOptions( state->render_options );
          if (ret)
               return ret;
     }

     if (flags & SMF_MATRIX) {
          ret = requestor->SetMatrix( state->matrix );
          if (ret)
               return ret;
     }

     if (flags & SMF_SOURCE2) {
          ret = requestor->SetSource2( state->source2 );
          if (ret)
               return ret;
     }

     if (flags & SMF_FROM) {
          ret = requestor->SetFrom( state->from, state->from_eye );
          if (ret)
               return ret;
     }

     if (flags & SMF_TO) {
          ret = requestor->SetTo( state->to, state->to_eye );
          if (ret)
               return ret;
     }

     if (flags & SMF_SRC_CONVOLUTION) {
          ret = requestor->SetSrcConvolution( &state->src_convolution );
          if (ret)
               return ret;
     }

     return DFB_OK;
}

DFBResult
CoreGraphicsStateClient_Update( CoreGraphicsStateClient *client,
                                DFBAccelerationMask      accel,
                                CardState               *state )
{
     DFBResult              ret;
     StateModificationFlags flags = (StateModificationFlags)(SMF_TO | SMF_DESTINATION | SMF_CLIP | SMF_RENDER_OPTIONS);

     D_DEBUG_AT( Core_GraphicsStateClient, "%s( client %p )\n", __FUNCTION__, client );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     D_MAGIC_ASSERT( state, CardState );

     /*
      * dfb_gfxcard_state_check() moves flags to mod_hw,
      * called from IDirectFBSurface::GetAccelerationMask().
      *
      * FIXME: Add GetAccelerationMask() to CoreGraphicsState flux
      *        and do not load the graphics driver at slaves anymore.
      */
     state->modified = (StateModificationFlags)(state->modified | state->mod_hw);
     state->mod_hw   = SMF_NONE;

     if (state->render_options & DSRO_MATRIX)
          flags = (StateModificationFlags)(flags | SMF_MATRIX);

     if (DFB_DRAWING_FUNCTION( accel )) {
          flags = (StateModificationFlags)(flags | SMF_DRAWING_FLAGS | SMF_COLOR);

          if (state->drawingflags & DSDRAW_BLEND)
               flags = (StateModificationFlags)(flags | SMF_SRC_BLEND | SMF_DST_BLEND);

          if (state->drawingflags & DSDRAW_DST_COLORKEY)
               flags = (StateModificationFlags)(flags | SMF_DST_COLORKEY);
     }
     else {
          flags = (StateModificationFlags)(flags | SMF_BLITTING_FLAGS | SMF_FROM | SMF_SOURCE);

          if (accel == DFXL_BLIT2)
               flags = (StateModificationFlags)(flags | SMF_FROM | SMF_SOURCE2);

          if (state->blittingflags & (DSBLIT_BLEND_COLORALPHA |
                                      DSBLIT_COLORIZE |
                                      DSBLIT_SRC_PREMULTCOLOR))
               flags = (StateModificationFlags)(flags | SMF_COLOR);

          if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL |
                                      DSBLIT_BLEND_COLORALPHA))
               flags = (StateModificationFlags)(flags | SMF_SRC_BLEND | SMF_DST_BLEND);

          if (state->blittingflags & DSBLIT_SRC_COLORKEY)
               flags = (StateModificationFlags)(flags | SMF_SRC_COLORKEY);

          if (state->blittingflags & DSBLIT_DST_COLORKEY)
               flags = (StateModificationFlags)(flags | SMF_DST_COLORKEY);

          if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR))
               flags = (StateModificationFlags)(flags | SMF_FROM | SMF_SOURCE_MASK | SMF_SOURCE_MASK_VALS);

          if (state->blittingflags & DSBLIT_INDEX_TRANSLATION)
               flags = (StateModificationFlags)(flags | SMF_INDEX_TRANSLATION);

          if (state->blittingflags & DSBLIT_COLORKEY_PROTECT)
               flags = (StateModificationFlags)(flags | SMF_COLORKEY);

          if (state->blittingflags & DSBLIT_SRC_CONVOLUTION)
               flags = (StateModificationFlags)(flags | SMF_SRC_CONVOLUTION);
     }

     STATE_HOLDER_SET( client );

     ret = CoreGraphicsStateClient_SetState( client, state, (StateModificationFlags)(state->modified & flags) );
     if (ret)
          return ret;

     state->modified = (StateModificationFlags)(state->modified & ~flags);

     return DFB_OK;
}

DFBResult
CoreGraphicsStateClient_GetAccelerationMask( CoreGraphicsStateClient *client,
                                             DFBAccelerationMask     *ret_accel )
{
     D_DEBUG_AT( Core_GraphicsStateClient, "%s( client %p )\n", __FUNCTION__, client );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     D_ASSERT( ret_accel != NULL );

     if (client->renderer) {
          return dfb_state_get_acceleration_mask( client->state, ret_accel );
     }
     else {
          if (!dfb_config->call_nodirect && (dfb_core_is_master( client->core ) || !fusion_config->secure_fusion)) {
               return dfb_state_get_acceleration_mask( client->state, ret_accel );
          }
          else {
               DFBResult ret;

               CoreGraphicsStateClient_Update( client,
                                               client->state->source ?
                                                  (client->state->source2 ? DFXL_BLIT2 : DFXL_BLIT) : DFXL_FILLRECTANGLE, client->state );

               DirectFB::IGraphicsState_Requestor *requestor = (DirectFB::IGraphicsState_Requestor*) client->requestor;

               ret = requestor->GetAccelerationMask( ret_accel );
               if (ret)
                    return ret;
          }
     }

     return DFB_OK;
}

DFBResult
CoreGraphicsStateClient_DrawRectangles( CoreGraphicsStateClient *client,
                                        const DFBRectangle      *rects,
                                        unsigned int             num )
{
     D_DEBUG_AT( Core_GraphicsStateClient, "%s( client %p )\n", __FUNCTION__, client );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     D_ASSERT( rects != NULL );

     if (client->renderer)
          client->renderer->DrawRectangles( rects, num );
     else {
          if (!dfb_config->call_nodirect && (dfb_core_is_master( client->core ) || !fusion_config->secure_fusion)) {
               unsigned int i;

               for (i=0; i<num; i++)
                    // FIXME: will overwrite rects
                    dfb_gfxcard_drawrectangle( (DFBRectangle*) &rects[i], client->state );
          }
          else {
               DFBResult ret;

               CoreGraphicsStateClient_Update( client, DFXL_DRAWRECTANGLE, client->state );

               DirectFB::IGraphicsState_Requestor *requestor = (DirectFB::IGraphicsState_Requestor*) client->requestor;

               ret = requestor->DrawRectangles( rects, num );
               if (ret)
                    return ret;
          }
     }

     return DFB_OK;
}

DFBResult
CoreGraphicsStateClient_DrawLines( CoreGraphicsStateClient *client,
                                   const DFBRegion         *lines,
                                   unsigned int             num )
{
     D_DEBUG_AT( Core_GraphicsStateClient, "%s( client %p )\n", __FUNCTION__, client );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     D_ASSERT( lines != NULL );

     if (client->renderer)
          client->renderer->DrawLines( lines, num );
     else {
          if (!dfb_config->call_nodirect && (dfb_core_is_master( client->core ) || !fusion_config->secure_fusion)) {
               // FIXME: will overwrite lines
               dfb_gfxcard_drawlines( (DFBRegion*) lines, num, client->state );
          }
          else {
               DFBResult ret;

               CoreGraphicsStateClient_Update( client, DFXL_DRAWLINE, client->state );

               DirectFB::IGraphicsState_Requestor *requestor = (DirectFB::IGraphicsState_Requestor*) client->requestor;

               ret = requestor->DrawLines( lines, num );
               if (ret)
                    return ret;
          }
     }

     return DFB_OK;
}

DFBResult
CoreGraphicsStateClient_FillRectangles( CoreGraphicsStateClient *client,
                                        const DFBRectangle      *rects,
                                        unsigned int             num )
{
     D_DEBUG_AT( Core_GraphicsStateClient, "%s( client %p )\n", __FUNCTION__, client );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     D_ASSERT( rects != NULL );

     if (client->renderer)
          client->renderer->FillRectangles( rects, num );
     else {
          if (!dfb_config->call_nodirect && (dfb_core_is_master( client->core ) || !fusion_config->secure_fusion)) {
               dfb_gfxcard_fillrectangles( rects, num, client->state );
          }
          else {
               DFBResult ret;

               CoreGraphicsStateClient_Update( client, DFXL_FILLRECTANGLE, client->state );

               DirectFB::IGraphicsState_Requestor *requestor = (DirectFB::IGraphicsState_Requestor*) client->requestor;

               ret = requestor->FillRectangles( rects, num );
               if (ret)
                    return ret;
          }
     }

     return DFB_OK;
}

DFBResult
CoreGraphicsStateClient_FillTriangles( CoreGraphicsStateClient *client,
                                       const DFBTriangle       *triangles,
                                       unsigned int             num )
{
     D_DEBUG_AT( Core_GraphicsStateClient, "%s( client %p )\n", __FUNCTION__, client );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     D_ASSERT( triangles != NULL );

     if (client->renderer)
          client->renderer->FillTriangles( triangles, num );
     else {
          if (!dfb_config->call_nodirect && (dfb_core_is_master( client->core ) || !fusion_config->secure_fusion)) {
                    dfb_gfxcard_filltriangles( triangles, num, client->state );
          }
          else {
               DFBResult ret;

               CoreGraphicsStateClient_Update( client, DFXL_FILLTRIANGLE, client->state );

               DirectFB::IGraphicsState_Requestor *requestor = (DirectFB::IGraphicsState_Requestor*) client->requestor;

               ret = requestor->FillTriangles( triangles, num );
               if (ret)
                    return ret;
          }
     }

     return DFB_OK;
}

DFBResult
CoreGraphicsStateClient_FillTrapezoids( CoreGraphicsStateClient *client,
                                        const DFBTrapezoid      *trapezoids,
                                        unsigned int             num )
{
     D_DEBUG_AT( Core_GraphicsStateClient, "%s( client %p )\n", __FUNCTION__, client );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     D_ASSERT( trapezoids != NULL );

     if (client->renderer)
          client->renderer->FillTrapezoids( trapezoids, num );
     else {
          if (!dfb_config->call_nodirect && (dfb_core_is_master( client->core ) || !fusion_config->secure_fusion)) {
               dfb_gfxcard_filltrapezoids( trapezoids, num, client->state );
          }
          else {
               DFBResult ret;

               CoreGraphicsStateClient_Update( client, DFXL_FILLTRAPEZOID, client->state );

               DirectFB::IGraphicsState_Requestor *requestor = (DirectFB::IGraphicsState_Requestor*) client->requestor;

               ret = requestor->FillTrapezoids( trapezoids, num );
               if (ret)
                    return ret;
          }
     }

     return DFB_OK;
}

DFBResult
CoreGraphicsStateClient_FillSpans( CoreGraphicsStateClient *client,
                                   int                      y,
                                   const DFBSpan           *spans,
                                   unsigned int             num )
{
     D_DEBUG_AT( Core_GraphicsStateClient, "%s( client %p )\n", __FUNCTION__, client );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     D_ASSERT( spans != NULL );

     if (client->renderer)
          client->renderer->FillSpans( y, spans, num );
     else {
          if (!dfb_config->call_nodirect && (dfb_core_is_master( client->core ) || !fusion_config->secure_fusion)) {
               // FIXME: may overwrite spans
               dfb_gfxcard_fillspans( y, (DFBSpan*) spans, num, client->state );
          }
          else {
               DFBResult ret;

               CoreGraphicsStateClient_Update( client, DFXL_FILLRECTANGLE, client->state );

               DirectFB::IGraphicsState_Requestor *requestor = (DirectFB::IGraphicsState_Requestor*) client->requestor;

               ret = requestor->FillSpans( y, spans, num );
               if (ret)
                    return ret;
          }
     }

     return DFB_OK;
}

DFBResult
CoreGraphicsStateClient_Blit( CoreGraphicsStateClient *client,
                              const DFBRectangle      *rects,
                              const DFBPoint          *points,
                              unsigned int             num )
{
     D_DEBUG_AT( Core_GraphicsStateClient, "%s( client %p )\n", __FUNCTION__, client );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     D_ASSERT( rects != NULL );
     D_ASSERT( points != NULL );

     if (client->renderer)
          client->renderer->Blit( rects, points, num );
     else {
          if (!dfb_config->call_nodirect && (dfb_core_is_master( client->core ) || !fusion_config->secure_fusion)) {
               // FIXME: will overwrite rects, points
               dfb_gfxcard_batchblit( (DFBRectangle*) rects, (DFBPoint*) points, num, client->state );
          }
          else {
               DFBResult    ret;
               unsigned int i;

               CoreGraphicsStateClient_Update( client, DFXL_BLIT, client->state );

               DirectFB::IGraphicsState_Requestor *requestor = (DirectFB::IGraphicsState_Requestor*) client->requestor;

               for (i=0; i<num; i+=200) {
                    ret = requestor->Blit( &rects[i], &points[i], MIN(200, num-i) );
                    if (ret)
                         return ret;
               }
          }
     }

     return DFB_OK;
}

DFBResult
CoreGraphicsStateClient_Blit2( CoreGraphicsStateClient *client,
                               const DFBRectangle      *rects,
                               const DFBPoint          *points1,
                               const DFBPoint          *points2,
                               unsigned int             num )
{
     D_DEBUG_AT( Core_GraphicsStateClient, "%s( client %p )\n", __FUNCTION__, client );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     D_ASSERT( rects != NULL );
     D_ASSERT( points1 != NULL );
     D_ASSERT( points2 != NULL );

     if (client->renderer)
          client->renderer->Blit2( rects, points1, points2, num );
     else {
          if (!dfb_config->call_nodirect && (dfb_core_is_master( client->core ) || !fusion_config->secure_fusion)) {
               // FIXME: will overwrite rects, points
               dfb_gfxcard_batchblit2( (DFBRectangle*) rects, (DFBPoint*) points1, (DFBPoint*) points2, num, client->state );
          }
          else {
               DFBResult ret;

               CoreGraphicsStateClient_Update( client, DFXL_BLIT2, client->state );

               DirectFB::IGraphicsState_Requestor *requestor = (DirectFB::IGraphicsState_Requestor*) client->requestor;

               ret = requestor->Blit2( rects, points1, points2, num );
               if (ret)
                    return ret;
          }
     }

     return DFB_OK;
}

DFBResult
CoreGraphicsStateClient_StretchBlit( CoreGraphicsStateClient *client,
                                     const DFBRectangle      *srects,
                                     const DFBRectangle      *drects,
                                     unsigned int             num )
{
     D_DEBUG_AT( Core_GraphicsStateClient, "%s( client %p )\n", __FUNCTION__, client );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     D_ASSERT( srects != NULL );
     D_ASSERT( drects != NULL );

     if (num == 0)
          return DFB_OK;

     if (client->renderer)
          client->renderer->StretchBlit( srects, drects, num );
     else {
          if (!dfb_config->call_nodirect && (dfb_core_is_master( client->core ) || !fusion_config->secure_fusion)) {
               if (num == 1 && srects[0].w == drects[0].w && srects[0].h == drects[0].h) {
                    DFBPoint point = { drects[0].x, drects[0].y };

                    // FIXME: will overwrite rects, points
                    dfb_gfxcard_batchblit( (DFBRectangle*) srects, &point, 1, client->state );
               }
               else {
                    // FIXME: will overwrite rects
                    dfb_gfxcard_batchstretchblit( (DFBRectangle*) srects, (DFBRectangle*) drects, num, client->state );
               }
          }
          else {
               DFBResult ret;

               DirectFB::IGraphicsState_Requestor *requestor = (DirectFB::IGraphicsState_Requestor*) client->requestor;

               if (num == 1 && srects[0].w == drects[0].w && srects[0].h == drects[0].h) {
                    CoreGraphicsStateClient_Update( client, DFXL_BLIT, client->state );

                    DFBPoint point = { drects[0].x, drects[0].y };
                    ret = requestor->Blit( srects, &point, 1 );
                    if (ret)
                         return ret;
               }
               else {
                    CoreGraphicsStateClient_Update( client, DFXL_STRETCHBLIT, client->state );

                    ret = requestor->StretchBlit( srects, drects, num );
                    if (ret)
                         return ret;
               }
          }
     }

     return DFB_OK;
}

DFBResult
CoreGraphicsStateClient_TileBlit( CoreGraphicsStateClient *client,
                                  const DFBRectangle      *rects,
                                  const DFBPoint          *points1,
                                  const DFBPoint          *points2,
                                  unsigned int             num )
{
     D_DEBUG_AT( Core_GraphicsStateClient, "%s( client %p )\n", __FUNCTION__, client );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     D_ASSERT( rects != NULL );
     D_ASSERT( points1 != NULL );
     D_ASSERT( points2 != NULL );

     if (client->renderer)
          client->renderer->TileBlit( rects, points1, points2, num );
     else {
          if (!dfb_config->call_nodirect && (dfb_core_is_master( client->core ) || !fusion_config->secure_fusion)) {
               u32 i;

               // FIXME: will overwrite rects, points
               for (i=0; i<num; i++)
                    dfb_gfxcard_tileblit( (DFBRectangle*) &rects[i], points1[i].x, points1[i].y, points2[i].x, points2[i].y, client->state );
          }
          else {
               DFBResult ret;

               CoreGraphicsStateClient_Update( client, DFXL_BLIT, client->state );

               DirectFB::IGraphicsState_Requestor *requestor = (DirectFB::IGraphicsState_Requestor*) client->requestor;

               ret = requestor->TileBlit( rects, points1, points2, num );
               if (ret)
                    return ret;
          }
     }

     return DFB_OK;
}

DFBResult
CoreGraphicsStateClient_TextureTriangles( CoreGraphicsStateClient *client,
                                          const DFBVertex         *vertices,
                                          int                      num,
                                          DFBTriangleFormation     formation )
{
     D_DEBUG_AT( Core_GraphicsStateClient, "%s( client %p )\n", __FUNCTION__, client );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     D_ASSERT( vertices != NULL );

     if (client->renderer)
          client->renderer->TextureTriangles( vertices, num, formation );
     else {
          if (!dfb_config->call_nodirect && (dfb_core_is_master( client->core ) || !fusion_config->secure_fusion)) {
               // FIXME: may overwrite vertices
               dfb_gfxcard_texture_triangles( (DFBVertex*) vertices, num, formation, client->state );
          }
          else {
               DFBResult ret;

               CoreGraphicsStateClient_Update( client, DFXL_TEXTRIANGLES, client->state );

               DirectFB::IGraphicsState_Requestor *requestor = (DirectFB::IGraphicsState_Requestor*) client->requestor;

               ret = requestor->TextureTriangles( vertices, num, formation );
               if (ret)
                    return ret;
          }
     }

     return DFB_OK;
}

}

