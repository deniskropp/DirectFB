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

#include <stdlib.h>
#include <string.h>

#include <core/fusion/shmalloc.h>

#include <core/core.h>
#include <core/core_parts.h>
#include <core/clipboard.h>

#include <misc/memcpy.h>


typedef struct {
     FusionSkirmish  lock;
     char           *mime_type;
     void           *data;
     unsigned int    size;
     struct timeval  timestamp;
} CoreClip;

static CoreClip *core_clip = NULL;


DFB_CORE_PART( clipboard, 0, sizeof(CoreClip) )


static DFBResult
dfb_clipboard_initialize( CoreDFB *core, void *data_local, void *data_shared )
{
     DFB_ASSERT( core_clip == NULL );

     core_clip = data_shared;

     fusion_skirmish_init( &core_clip->lock );

     return DFB_OK;
}

static DFBResult
dfb_clipboard_join( CoreDFB *core, void *data_local, void *data_shared )
{
     DFB_ASSERT( core_clip == NULL );

     core_clip = data_shared;

     return DFB_OK;
}

static DFBResult
dfb_clipboard_shutdown( CoreDFB *core, bool emergency )
{
     DFB_ASSERT( core_clip != NULL );

     fusion_skirmish_destroy( &core_clip->lock );

     if (core_clip->data)
          SHFREE( core_clip->data );

     if (core_clip->mime_type)
          SHFREE( core_clip->mime_type );

     core_clip = NULL;

     return DFB_OK;
}

static DFBResult
dfb_clipboard_leave( CoreDFB *core, bool emergency )
{
     DFB_ASSERT( core_clip != NULL );

     core_clip = NULL;

     return DFB_OK;
}

static DFBResult
dfb_clipboard_suspend( CoreDFB *core )
{
     DFB_ASSERT( core_clip != NULL );

     return DFB_OK;
}

static DFBResult
dfb_clipboard_resume( CoreDFB *core )
{
     DFB_ASSERT( core_clip != NULL );

     return DFB_OK;
}


DFBResult
dfb_clipboard_set( const char     *mime_type,
                   const void     *data,
                   unsigned int    size,
                   struct timeval *timestamp )
{
     char *new_mime;
     void *new_data;

     DFB_ASSERT( core_clip != NULL );

     DFB_ASSERT( mime_type != NULL );
     DFB_ASSERT( data != NULL );
     DFB_ASSERT( size > 0 );

     new_mime = SHSTRDUP( mime_type );
     if (!new_mime)
          return DFB_NOSYSTEMMEMORY;

     new_data = SHMALLOC( size );
     if (!new_data) {
          SHFREE( new_mime );
          return DFB_NOSYSTEMMEMORY;
     }

     dfb_memcpy( new_data, data, size );

     if (fusion_skirmish_prevail( &core_clip->lock )) {
          SHFREE( new_data );
          SHFREE( new_mime );
          return DFB_FUSION;
     }

     if (core_clip->data)
          SHFREE( core_clip->data );

     if (core_clip->mime_type)
          SHFREE( core_clip->mime_type );

     core_clip->mime_type = new_mime;
     core_clip->data      = new_data;
     core_clip->size      = size;

     gettimeofday( &core_clip->timestamp, NULL );

     if (timestamp)
          *timestamp = core_clip->timestamp;

     fusion_skirmish_dismiss( &core_clip->lock );

     return DFB_OK;
}

DFBResult
dfb_clipboard_get( char **mime_type, void **data, unsigned int *size )
{
     DFB_ASSERT( core_clip != NULL );

     if (fusion_skirmish_prevail( &core_clip->lock ))
          return DFB_FUSION;

     if (!core_clip->mime_type || !core_clip->data) {
          fusion_skirmish_dismiss( &core_clip->lock );
          return DFB_BUFFEREMPTY;
     }

     if (mime_type)
          *mime_type = strdup( core_clip->mime_type );

     if (data) {
          *data = malloc( core_clip->size );
          dfb_memcpy( *data, core_clip->data, core_clip->size );
     }

     if (size)
          *size = core_clip->size;

     fusion_skirmish_dismiss( &core_clip->lock );

     return DFB_OK;
}

DFBResult
dfb_clipboard_get_timestamp( struct timeval *timestamp )
{
     DFB_ASSERT( core_clip != NULL );
     DFB_ASSERT( timestamp != NULL );

     if (fusion_skirmish_prevail( &core_clip->lock ))
          return DFB_FUSION;

     *timestamp = core_clip->timestamp;

     fusion_skirmish_dismiss( &core_clip->lock );

     return DFB_OK;
}

