/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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

#include <directfb.h>

#include <direct/debug.h>
#include <direct/memcpy.h>

#include <fusion/shmalloc.h>

#include <core/core.h>
#include <core/core_parts.h>
#include <core/clipboard.h>


D_DEBUG_DOMAIN( Core_Clipboard, "Core/Clipboard", "DirectFB Clipboard Core" );

/**********************************************************************************************************************/

typedef struct {
     int                     magic;

     FusionSkirmish          lock;
     char                   *mime_type;
     void                   *data;
     unsigned int            size;
     struct timeval          timestamp;

     FusionSHMPoolShared    *shmpool;
} DFBClipboardCoreShared;

struct __DFB_DFBClipboardCore {
     int                     magic;

     CoreDFB                *core;

     DFBClipboardCoreShared *shared;
};


DFB_CORE_PART( clipboard_core, ClipboardCore );

/**********************************************************************************************************************/

static DFBResult
dfb_clipboard_core_initialize( CoreDFB                *core,
                               DFBClipboardCore       *data,
                               DFBClipboardCoreShared *shared )
{
     D_DEBUG_AT( Core_Clipboard, "dfb_clipboard_core_initialize( %p, %p, %p )\n", core, data, shared );

     D_ASSERT( data != NULL );
     D_ASSERT( shared != NULL );

     data->core   = core;
     data->shared = shared;

     shared->shmpool = dfb_core_shmpool( core );

     fusion_skirmish_init( &shared->lock, "Clipboard Core", dfb_core_world(core) );

     D_MAGIC_SET( data, DFBClipboardCore );
     D_MAGIC_SET( shared, DFBClipboardCoreShared );

     return DFB_OK;
}

static DFBResult
dfb_clipboard_core_join( CoreDFB                *core,
                         DFBClipboardCore       *data,
                         DFBClipboardCoreShared *shared )
{
     D_DEBUG_AT( Core_Clipboard, "dfb_clipboard_core_join( %p, %p, %p )\n", core, data, shared );

     D_ASSERT( data != NULL );
     D_MAGIC_ASSERT( shared, DFBClipboardCoreShared );

     data->core   = core;
     data->shared = shared;

     D_MAGIC_SET( data, DFBClipboardCore );

     return DFB_OK;
}

static DFBResult
dfb_clipboard_core_shutdown( DFBClipboardCore *data,
                             bool              emergency )
{
     DFBClipboardCoreShared *shared;

     D_DEBUG_AT( Core_Clipboard, "dfb_clipboard_core_shutdown( %p, %semergency )\n", data, emergency ? "" : "no " );

     D_MAGIC_ASSERT( data, DFBClipboardCore );

     shared = data->shared;

     D_MAGIC_ASSERT( shared, DFBClipboardCoreShared );

     fusion_skirmish_destroy( &shared->lock );

     if (shared->data)
          SHFREE( shared->shmpool, shared->data );

     if (shared->mime_type)
          SHFREE( shared->shmpool, shared->mime_type );

     D_MAGIC_CLEAR( data );
     D_MAGIC_CLEAR( shared );

     return DFB_OK;
}

static DFBResult
dfb_clipboard_core_leave( DFBClipboardCore *data,
                          bool              emergency )
{
     D_DEBUG_AT( Core_Clipboard, "dfb_clipboard_core_leave( %p, %semergency )\n", data, emergency ? "" : "no " );

     D_MAGIC_ASSERT( data, DFBClipboardCore );
     D_MAGIC_ASSERT( data->shared, DFBClipboardCoreShared );

     D_MAGIC_CLEAR( data );

     return DFB_OK;
}

static DFBResult
dfb_clipboard_core_suspend( DFBClipboardCore *data )
{
     D_DEBUG_AT( Core_Clipboard, "dfb_clipboard_core_suspend( %p )\n", data );

     D_MAGIC_ASSERT( data, DFBClipboardCore );
     D_MAGIC_ASSERT( data->shared, DFBClipboardCoreShared );

     return DFB_OK;
}

static DFBResult
dfb_clipboard_core_resume( DFBClipboardCore *data )
{
     D_DEBUG_AT( Core_Clipboard, "dfb_clipboard_core_resume( %p )\n", data );

     D_MAGIC_ASSERT( data, DFBClipboardCore );
     D_MAGIC_ASSERT( data->shared, DFBClipboardCoreShared );

     return DFB_OK;
}

/**********************************************************************************************************************/

DFBResult
dfb_clipboard_set( DFBClipboardCore *core,
                   const char       *mime_type,
                   const void       *data,
                   unsigned int      size,
                   struct timeval   *timestamp )
{
     DFBClipboardCoreShared *shared;

     char *new_mime;
     void *new_data;

     D_MAGIC_ASSERT( core, DFBClipboardCore );
     D_ASSERT( mime_type != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( size > 0 );

     shared = core->shared;

     D_MAGIC_ASSERT( shared, DFBClipboardCoreShared );

     new_mime = SHSTRDUP( shared->shmpool, mime_type );
     if (!new_mime)
          return D_OOSHM();

     new_data = SHMALLOC( shared->shmpool, size );
     if (!new_data) {
          SHFREE( shared->shmpool, new_mime );
          return D_OOSHM();
     }

     direct_memcpy( new_data, data, size );

     if (fusion_skirmish_prevail( &shared->lock )) {
          SHFREE( shared->shmpool, new_data );
          SHFREE( shared->shmpool, new_mime );
          return DFB_FUSION;
     }

     if (shared->data)
          SHFREE( shared->shmpool, shared->data );

     if (shared->mime_type)
          SHFREE( shared->shmpool, shared->mime_type );

     shared->mime_type = new_mime;
     shared->data      = new_data;
     shared->size      = size;

     gettimeofday( &shared->timestamp, NULL );

     if (timestamp)
          *timestamp = shared->timestamp;

     fusion_skirmish_dismiss( &shared->lock );

     return DFB_OK;
}

DFBResult
dfb_clipboard_get( DFBClipboardCore  *core,
                   char             **mime_type,
                   void             **data,
                   unsigned int      *size )
{
     DFBClipboardCoreShared *shared;

     D_MAGIC_ASSERT( core, DFBClipboardCore );

     shared = core->shared;

     D_MAGIC_ASSERT( shared, DFBClipboardCoreShared );

     if (fusion_skirmish_prevail( &shared->lock ))
          return DFB_FUSION;

     if (!shared->mime_type || !shared->data) {
          fusion_skirmish_dismiss( &shared->lock );
          return DFB_BUFFEREMPTY;
     }

     if (mime_type)
          *mime_type = strdup( shared->mime_type );

     if (data) {
          *data = malloc( shared->size );
          direct_memcpy( *data, shared->data, shared->size );
     }

     if (size)
          *size = shared->size;

     fusion_skirmish_dismiss( &shared->lock );

     return DFB_OK;
}

DFBResult
dfb_clipboard_get_timestamp( DFBClipboardCore *core,
                             struct timeval   *timestamp )
{
     DFBClipboardCoreShared *shared;

     D_MAGIC_ASSERT( core, DFBClipboardCore );
     D_ASSERT( timestamp != NULL );

     shared = core->shared;

     D_MAGIC_ASSERT( shared, DFBClipboardCoreShared );

     if (fusion_skirmish_prevail( &shared->lock ))
          return DFB_FUSION;

     *timestamp = shared->timestamp;

     fusion_skirmish_dismiss( &shared->lock );

     return DFB_OK;
}

