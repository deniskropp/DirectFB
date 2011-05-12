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

#include <direct/messages.h>
#include <direct/util.h>

#include <core/CoreLayerContext.h>
#include <core/CoreLayerContext_internal.h>

#include <core/core.h>
#include <core/layer_context.h>
#include <core/layers_internal.h>
#include <core/windows_internal.h>


D_DEBUG_DOMAIN( Core_LayerContext, "Core/LayerContext", "DirectFB Display Layer Context" );

/**********************************************************************************************************************/

static DFBWindowID
CoreLayerContext_Dispatch_CreateWindow( CoreLayerContext           *context,
                                        const DFBWindowDescription *desc )
{
     DFBResult   ret;
     CoreWindow *window;
     CoreLayer  *layer;

     D_DEBUG_AT( Core_LayerContext, "%s( %p, %p )\n", __FUNCTION__, context, desc );

     D_MAGIC_ASSERT( context, CoreLayerContext );
     D_ASSERT( desc != NULL );

     layer = dfb_layer_at( context->layer_id );

     ret = dfb_layer_context_create_window( layer->core, context, desc, &window );
     if (ret)
          return 0;

     return window->object.id;
}

/**********************************************************************************************************************/

FusionCallHandlerResult
CoreLayerContext_Dispatch( int           caller,   /* fusion id of the caller */
                           int           call_arg, /* optional call parameter */
                           void         *call_ptr, /* optional call parameter */
                           void         *ctx,      /* optional handler context */
                           unsigned int  serial,
                           int          *ret_val )
{
     CoreLayerContextCreateWindow *create_window = call_ptr;

     switch (call_arg) {
          case CORE_LAYERCONTEXT_CREATE_WINDOW:
               D_DEBUG_AT( Core_LayerContext, "=-> CORE_LAYERCONTEXT_CREATE_WINDOW\n" );

               *ret_val = CoreLayerContext_Dispatch_CreateWindow( ctx, &create_window->desc );
               break;

          default:
               D_BUG( "invalid call arg %d", call_arg );
               *ret_val = DFB_INVARG;
     }

     return FCHR_RETURN;
}

/**********************************************************************************************************************/

DFBResult
CoreLayerContext_CreateWindow( CoreDFB                     *core,
                               CoreLayerContext            *context,
                               const DFBWindowDescription  *desc,
                               CoreWindow                 **ret_window )
{
     DFBResult   ret;
     int         val;
     CoreWindow *window;

     D_DEBUG_AT( Core_LayerContext, "%s( %p, %p, %p, %p )\n", __FUNCTION__, core, context, desc, ret_window );

     D_MAGIC_ASSERT( context, CoreLayerContext );
     D_ASSERT( desc != NULL );
     D_ASSERT( ret_window != NULL );

     CoreLayerContextCreateWindow create;

     create.desc = *desc;

     ret = fusion_call_execute2( &context->call, FCEF_NONE, CORE_LAYERCONTEXT_CREATE_WINDOW, &create, sizeof(create), &val );
     if (ret) {
          D_DERROR( ret, "Core/LayerContext: fusion_call_execute2( CORE_LAYERCONTEXT_CREATE_WINDOW ) failed!\n" );
          return ret;
     }

     if (!val) {
          D_ERROR( "Core/LayerContext: CORE_LAYERCONTEXT_CREATE_WINDOW failed!\n" );
          return DFB_FAILURE;
     }

     ret = dfb_core_get_window( core, val, &window );
     if (ret) {
          D_DERROR( ret, "Core/LayerContext: Looking up window by ID %u failed!\n", (DFBWindowID) val );
          return ret;
     }

     *ret_window = window;

     return DFB_OK;
}

