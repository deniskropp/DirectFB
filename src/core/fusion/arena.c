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

#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>

#include <pthread.h>

#include <misc/mem.h>

#include "fusion_types.h"
#include "list.h"
#include "lock.h"
#include "ref.h"
#include "arena.h"
#include "shmalloc.h"

#include "fusion_internal.h"


/***************************
 *  Internal declarations  *
 ***************************/

typedef struct {
     FusionLink  link;

     char       *name;
     void       *data;
} ArenaField;

/*
 *
 */
struct _FusionArena {
     FusionLink      link;

     FusionSkirmish  lock;
     FusionRef       ref;

     char           *name;

     FusionLink     *fields;
};

static FusionArena *lock_arena( const char *name, bool add );

static void         unlock_arena( FusionArena *arena );

/****************
 *  Public API  *
 ****************/

FusionResult arena_enter (const char     *name,
                          ArenaEnterFunc  initialize,
                          ArenaEnterFunc  join,
                          void           *ctx,
                          FusionArena   **ret_arena,
                          int            *ret_error)
{
     FusionArena    *arena;
     ArenaEnterFunc  func;
     int             error = 0;

     DFB_ASSERT( name != NULL );
     DFB_ASSERT( initialize != NULL );
     DFB_ASSERT( join != NULL );
     DFB_ASSERT( ret_arena != NULL );

     /* Lookup arena and lock it. If it doesn't exist create it. */
     arena = lock_arena( name, true );
     if (!arena)
          return FUSION_FAILURE;

     if (fusion_ref_zero_trylock( &arena->ref ) == FUSION_SUCCESS) {
          FDEBUG ("entering arena `%s´ (establishing)\n", name);
          
          func = initialize;

          fusion_ref_unlock( &arena->ref );
     }
     else {
          FDEBUG ("entering arena `%s´ (joining)\n", name);
          
          func = join;
     }
     
     fusion_ref_up (&arena->ref, false);
     
     
     *ret_arena = arena;
     
     error = func (arena, ctx);
     
     if (ret_error)
          *ret_error = error;
     
     unlock_arena( arena );

     return FUSION_SUCCESS;
}

FusionResult arena_add_shared_field (FusionArena *arena,
                                     const char  *name,
                                     void        *data)
{
     ArenaField *field;

     DFB_ASSERT( arena != NULL );
     DFB_ASSERT( data != NULL );
     DFB_ASSERT( name != NULL );

     if (skirmish_prevail( &arena->lock ))
          return FUSION_FAILURE;

     field = shcalloc( 1, sizeof(ArenaField) );
     if (!field) {
          skirmish_dismiss( &arena->lock );
          return FUSION_FAILURE;
     }

     field->name = shstrdup( name );
     field->data = data;

     fusion_list_prepend( &arena->fields, &field->link );

     skirmish_dismiss( &arena->lock );

     return FUSION_SUCCESS;
}

FusionResult arena_get_shared_field (FusionArena  *arena,
                                     const char   *name,
                                     void        **data)
{
     FusionLink *l;

     DFB_ASSERT( arena != NULL );
     DFB_ASSERT( data != NULL );
     DFB_ASSERT( name != NULL );
     
     if (skirmish_prevail( &arena->lock ))
          return FUSION_FAILURE;

     fusion_list_foreach (l, arena->fields) {
          ArenaField *field = (ArenaField*) l;

          if (! strcmp( field->name, name )) {
               skirmish_dismiss( &arena->lock );

               *data = field->data;

               return FUSION_SUCCESS;
          }
     }
     
     skirmish_dismiss( &arena->lock );

     return FUSION_NOTEXISTENT;
}

FusionResult arena_exit (FusionArena   *arena,
                         ArenaExitFunc  shutdown,
                         ArenaExitFunc  leave,
                         void          *ctx,
                         bool           emergency,
                         int           *ret_error)
{
     int error = 0;

     DFB_ASSERT( arena != NULL );
     DFB_ASSERT( shutdown != NULL );
     DFB_ASSERT( leave != NULL );
     
     if (skirmish_prevail( &arena->lock ))
          return FUSION_FAILURE;

     fusion_ref_down( &arena->ref, false );

     if (fusion_ref_zero_trylock( &arena->ref ) == FUSION_SUCCESS) {
          FusionLink *l = arena->fields;

          error = shutdown( arena, ctx, emergency );

          while (l) {
               FusionLink *next  = l->next;
               ArenaField *field = (ArenaField*) l;

               shfree( field->name );
               shfree( field );

               l = next;
          }

          fusion_ref_destroy( &arena->ref );
          skirmish_destroy( &arena->lock );
          
          skirmish_prevail( &fusion_shared->arenas_lock );
          fusion_list_remove( &fusion_shared->arenas, &arena->link );
          skirmish_dismiss( &fusion_shared->arenas_lock );

          shfree( arena->name );
          shfree( arena );
     }
     else {
          error = leave( arena, ctx, emergency );

          skirmish_dismiss( &arena->lock );
     }

     if (ret_error)
          *ret_error = error;

     return FUSION_SUCCESS;
}


/*****************************
 *  File internal functions  *
 *****************************/

static FusionArena *
lock_arena( const char *name, bool add )
{
     FusionLink *l;

     skirmish_prevail( &fusion_shared->arenas_lock );

     fusion_list_foreach (l, fusion_shared->arenas) {
          FusionArena *arena = (FusionArena*) l;

          if (skirmish_prevail( &arena->lock ))
               continue;

          if (! strcmp( arena->name, name )) {
               skirmish_dismiss( &fusion_shared->arenas_lock );

               return arena;
          }

          skirmish_dismiss( &arena->lock );
     }

     if (add) {
          FusionArena *arena = shcalloc( 1, sizeof(FusionArena) );

          skirmish_init( &arena->lock );
          fusion_ref_init( &arena->ref );

          arena->name = shstrdup( name );

          fusion_list_prepend( &fusion_shared->arenas, &arena->link );
          
          skirmish_prevail( &arena->lock );
          skirmish_dismiss( &fusion_shared->arenas_lock );

          return arena;
     }
     
     skirmish_dismiss( &fusion_shared->arenas_lock );

     return NULL;
}

static void
unlock_arena( FusionArena *arena )
{
     DFB_ASSERT( arena != NULL );

     skirmish_dismiss( &arena->lock );
}

