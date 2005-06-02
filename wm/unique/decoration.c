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
#include <core/layers_internal.h>  /* FIXME */
#include <core/windows_internal.h>  /* FIXME */

#include <misc/util.h>

#include <unique/decoration.h>
#include <unique/internal.h>
#include <unique/window.h>


D_DEBUG_DOMAIN( UniQuE_Decoration, "UniQuE/Decoration", "UniQuE's Decoration Object" );


static const ReactionFunc unique_decoration_globals[] = {
/*     _unique_foo_decoration_listener,*/
     NULL
};

/**************************************************************************************************/

static void
decoration_destructor( FusionObject *object, bool zombie )
{
     UniqueDecoration *decoration = (UniqueDecoration*) object;

     D_MAGIC_ASSERT( decoration, UniqueDecoration );

     D_DEBUG_AT( UniQuE_Decoration, "destroying %p%s\n", decoration, zombie ? " (ZOMBIE)" : "");

     unique_window_unlink( &decoration->window );
     unique_context_unlink( &decoration->context );

     D_MAGIC_CLEAR( decoration );

     fusion_object_destroy( object );
}

FusionObjectPool *
unique_decoration_pool_create()
{
     return fusion_object_pool_create( "UniQuE Decoration Pool",
                                       sizeof(UniqueDecoration),
                                       sizeof(UniqueDecorationNotification),
                                       decoration_destructor );
}

/**************************************************************************************************/

DFBResult
unique_decoration_create( UniqueWindow           *window,
                          UniqueDecorationFlags   flags,
                          UniqueDecoration      **ret_decoration )
{
     DFBResult         ret;
     UniqueDecoration *decoration;
     UniqueContext    *context;

     D_ASSERT( window != NULL );
     D_ASSERT( D_FLAGS_ARE_IN( flags, UDF_ALL ) );
     D_ASSERT( ret_decoration != NULL );

     context = window->context;

     D_MAGIC_ASSERT( context, UniqueContext );


     /* Create a decoration object. */
     decoration = unique_wm_create_decoration();
     if (!decoration)
          return DFB_FUSION;

     /* Initialize deocration data. */
     decoration->flags = flags;

     ret = unique_window_link( &decoration->window, window );
     if (ret)
          goto error;

     ret = unique_context_link( &decoration->context, window->context );
     if (ret)
          goto error;


     D_MAGIC_SET( decoration, UniqueDecoration );


     /* Change global reaction lock. */
     fusion_object_set_lock( &decoration->object, &context->stack->context->lock );

     /* activate object */
     fusion_object_activate( &decoration->object );

     /* return the new decoration */
     *ret_decoration = decoration;

     return DFB_OK;

error:
     if (decoration->context)
          unique_context_unlink( &decoration->context );

     if (decoration->window)
          unique_window_unlink( &decoration->window );

     fusion_object_destroy( &decoration->object );

     return ret;
}

DFBResult
unique_decoration_destroy( UniqueDecoration *decoration )
{
     D_MAGIC_ASSERT( decoration, UniqueDecoration );

     D_FLAGS_SET( decoration->flags, UDF_DESTROYED );

     unique_decoration_notify( decoration, UDNF_DESTROYED );

     return DFB_OK;
}

DFBResult
unique_decoration_notify( UniqueDecoration                  *decoration,
                          UniqueDecorationNotificationFlags  flags )
{
     UniqueDecorationNotification notification;

     D_MAGIC_ASSERT( decoration, UniqueDecoration );

     D_ASSERT( flags != UDNF_NONE );

     D_ASSERT( ! (flags & ~UDNF_ALL) );

     notification.flags      = flags;
     notification.decoration = decoration;

     return unique_decoration_dispatch( decoration, &notification, unique_decoration_globals );
}

DFBResult
unique_decoration_update( UniqueDecoration *decoration,
                          const DFBRegion  *region )
{
     D_MAGIC_ASSERT( decoration, UniqueDecoration );

     DFB_REGION_ASSERT_IF( region );

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

