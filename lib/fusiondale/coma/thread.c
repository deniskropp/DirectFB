/*
   (c) Copyright 2007  directfb.org

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>.

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

#include <direct/debug.h>
#include <direct/interface.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <fusion/call.h>
#include <fusion/fusion.h>
#include <fusion/hash.h>
#include <fusion/lock.h>
#include <fusion/shmalloc.h>

#include <coma/coma.h>
#include <coma/thread.h>

D_DEBUG_DOMAIN( Coma_Thread, "Coma/Thread", "Coma Thread" );

/**********************************************************************************************************************/

static const ReactionFunc coma_thread_globals[] = {
          NULL
};

static void
thread_destructor( FusionObject *object, bool zombie, void *ctx )
{
     ComaThread *thread = (ComaThread*) object;

     D_MAGIC_ASSERT( thread, ComaThread );

     D_DEBUG_AT( Coma_Thread, "%s( %p [%lu] )%s\n", __FUNCTION__, thread, object->id, zombie ? " ZOMBIE!" : "" );

     if (thread->mem)
          SHFREE( thread->shmpool, thread->mem );

     D_MAGIC_CLEAR( thread );

     fusion_object_destroy( object );
}

FusionObjectPool *
coma_thread_pool_create( Coma *coma )
{
     return fusion_object_pool_create( "Thread", sizeof(ComaThread), sizeof(void*),
                                       thread_destructor, coma, coma_world(coma) );
}

/**********************************************************************************************************************/

DirectResult
coma_thread_init( ComaThread *thread,
                  Coma       *coma )
{
     FusionWorld *world;

     D_ASSERT( thread != NULL );
     D_ASSERT( coma != NULL );

     D_DEBUG_AT( Coma_Thread, "%s( %p, %p )\n", __FUNCTION__, thread, coma );

     world = coma_world( coma );

     thread->shmpool = coma_shmpool( coma );

     /* Remember creator. */
     thread->fusion_id = fusion_id( world );

     D_MAGIC_SET( thread, ComaThread );

     return DR_OK;
}

