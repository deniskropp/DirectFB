/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#include <directfb.h>

#include <direct/debug.h>
#include <direct/messages.h>

#include <fusion/object.h>

#include <core/coretypes.h>
#include <core/windowstack.h>

#include <unique/context.h>
#include <unique/internal.h>


D_DEBUG_DOMAIN( UniQuE_Context, "UniQuE/Context", "UniQuE's Stack Context" );


static const React unique_context_globals[] = {
          NULL
};


static void
context_destructor( FusionObject *object, bool zombie )
{
     UniqueContext *context = (UniqueContext*) object;

     D_DEBUG_AT( UniQuE_Context, "destroying %p (stack data %p)%s\n",
                 context, context->stack_data, zombie ? " (ZOMBIE)" : "");

     D_MAGIC_CLEAR( context );

     fusion_object_destroy( object );
}

/** public **/

FusionObjectPool *
unique_context_pool_create()
{
     return fusion_object_pool_create( "UniQuE Context Pool", sizeof(UniqueContext),
                                       sizeof(UniqueContextNotification), context_destructor );
}

DFBResult
unique_context_create( StackData      *data,
                       UniqueContext **ret_context )
{
     UniqueContext *context;

     D_ASSERT( data != NULL );
     D_ASSERT( ret_context != NULL );

     context = unique_wm_create_context();
     if (!context)
          return DFB_FUSION;

     context->stack_data = data;
     context->color      = (DFBColor) { 0xc0, 0x30, 0x50, 0x80 };

     D_MAGIC_SET( context, UniqueContext );

     /* activate object */
     fusion_object_activate( &context->object );

     /* return the new context */
     *ret_context = context;

     return DFB_OK;
}

DFBResult
unique_context_close( UniqueContext *context )
{
     UniqueContextNotification notification;

     D_MAGIC_ASSERT( context, UniqueContext );

     notification.flags   = UCNF_CLOSE;
     notification.context = context;

     unique_context_dispatch( context, &notification, unique_context_globals );

     return DFB_OK;
}

DFBResult
unique_context_set_color( UniqueContext  *context,
                          const DFBColor *color )
{
     D_MAGIC_ASSERT( context, UniqueContext );

     D_ASSERT( color != NULL );

     context->color = *color;

     return dfb_windowstack_repaint_all( context->stack_data->stack );
}

