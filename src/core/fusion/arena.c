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


#ifndef FUSION_FAKE

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

FusionResult
fusion_arena_enter (const char     *name,
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

     /* Check if we are the first. */
     if (fusion_ref_zero_trylock( &arena->ref ) == FUSION_SUCCESS) {
          FDEBUG ("entering arena '%s' (establishing)\n", name);

          /* Call 'initialize' later. */
          func = initialize;

          /* Unlock the reference counter. */
          fusion_ref_unlock( &arena->ref );
     }
     else {
          FDEBUG ("entering arena '%s' (joining)\n", name);

          /* Call 'join' later. */
          func = join;
     }

     /* Increase reference counter. */
     fusion_ref_up (&arena->ref, false);

     /* Return the arena. */
     *ret_arena = arena;

     /* Call 'initialize' or 'join'. */
     error = func (arena, ctx);

     /* Return the return value of the callback. */
     if (ret_error)
          *ret_error = error;

     if (error) {
          FusionLink *l = arena->fields;

          fusion_ref_down (&arena->ref, false);

          if (func == initialize) {
               /* Destroy fields. */
               while (l) {
                    FusionLink *next  = l->next;
                    ArenaField *field = (ArenaField*) l;

                    /* Free allocated memory. */
                    SHFREE( field->name );
                    SHFREE( field );

                    l = next;
               }

               /* Destroy reference counter. */
               fusion_ref_destroy( &arena->ref );

               /* Destroy the arena lock. This has to happen before
                  locking the list. Otherwise a dead lock with lock_arena()
                  below could occur. */
               fusion_skirmish_destroy( &arena->lock );

               /* Lock the list and remove the arena. */
               fusion_skirmish_prevail( &_fusion_shared->arenas_lock );
               fusion_list_remove( &_fusion_shared->arenas, &arena->link );
               fusion_skirmish_dismiss( &_fusion_shared->arenas_lock );

               /* Free allocated memory. */
               SHFREE( arena->name );
               SHFREE( arena );

               return FUSION_SUCCESS;
          }
     }

     /* Unlock the arena. */
     unlock_arena( arena );

     return FUSION_SUCCESS;
}

FusionResult
fusion_arena_add_shared_field (FusionArena *arena,
                               const char  *name,
                               void        *data)
{
     ArenaField *field;

     DFB_ASSERT( arena != NULL );
     DFB_ASSERT( data != NULL );
     DFB_ASSERT( name != NULL );

     /* Lock the arena. */
     if (fusion_skirmish_prevail( &arena->lock ))
          return FUSION_FAILURE;

     /* Allocate memory for the field information. */
     field = SHCALLOC( 1, sizeof(ArenaField) );
     if (!field) {
          fusion_skirmish_dismiss( &arena->lock );
          return FUSION_FAILURE;
     }

     /* Give it the requested name. */
     field->name = SHSTRDUP( name );

     /* Assign the data pointer. */
     field->data = data;

     /* Add the field to the list. */
     fusion_list_prepend( &arena->fields, &field->link );

     /* Unlock the arena. */
     fusion_skirmish_dismiss( &arena->lock );

     return FUSION_SUCCESS;
}

FusionResult
fusion_arena_get_shared_field (FusionArena  *arena,
                               const char   *name,
                               void        **data)
{
     FusionLink *l;

     DFB_ASSERT( arena != NULL );
     DFB_ASSERT( data != NULL );
     DFB_ASSERT( name != NULL );

     /* Lock the arena. */
     if (fusion_skirmish_prevail( &arena->lock ))
          return FUSION_FAILURE;

     /* For each field in the arena... */
     fusion_list_foreach (l, arena->fields) {
          ArenaField *field = (ArenaField*) l;

          /* Check if the name matches. */
          if (! strcmp( field->name, name )) {
               /* Get its data pointer. */
               *data = field->data;

               /* Unlock the arena. */
               fusion_skirmish_dismiss( &arena->lock );

               /* Field has been found. */
               return FUSION_SUCCESS;
          }
     }

     /* Unlock the arena. */
     fusion_skirmish_dismiss( &arena->lock );

     /* No field by that name has been found. */
     return FUSION_NOTEXISTENT;
}

FusionResult
fusion_arena_exit (FusionArena   *arena,
                   ArenaExitFunc  shutdown,
                   ArenaExitFunc  leave,
                   void          *ctx,
                   bool           emergency,
                   int           *ret_error)
{
     int error = 0;

     DFB_ASSERT( arena != NULL );
     DFB_ASSERT( shutdown != NULL );

     /* Lock the arena. */
     if (fusion_skirmish_prevail( &arena->lock ))
          return FUSION_FAILURE;

     /* Decrease reference counter. */
     fusion_ref_down( &arena->ref, false );

     /* If we are the last... */
     if (fusion_ref_zero_trylock( &arena->ref ) == FUSION_SUCCESS) {
          FusionLink *l = arena->fields;

          /* Deinitialize everything. */
          error = shutdown( arena, ctx, emergency );

          /* Destroy fields. */
          while (l) {
               FusionLink *next  = l->next;
               ArenaField *field = (ArenaField*) l;

               /* Free allocated memory. */
               SHFREE( field->name );
               SHFREE( field );

               l = next;
          }

          /* Destroy reference counter. */
          fusion_ref_destroy( &arena->ref );

          /* Destroy the arena lock. This has to happen before
             locking the list. Otherwise a dead lock with lock_arena()
             below could occur. */
          fusion_skirmish_destroy( &arena->lock );

          /* Lock the list and remove the arena. */
          fusion_skirmish_prevail( &_fusion_shared->arenas_lock );
          fusion_list_remove( &_fusion_shared->arenas, &arena->link );
          fusion_skirmish_dismiss( &_fusion_shared->arenas_lock );

          /* Free allocated memory. */
          SHFREE( arena->name );
          SHFREE( arena );
     }
     else {
          if (!leave) {
               fusion_ref_up( &arena->ref, false );
               fusion_skirmish_dismiss( &arena->lock );
               return FUSION_INUSE;
          }

          /* Simply leave the arena. */
          error = leave( arena, ctx, emergency );

          /* Unlock the arena. */
          fusion_skirmish_dismiss( &arena->lock );
     }

     /* Return the return value of the callback. */
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

     /* Lock the list. */
     fusion_skirmish_prevail( &_fusion_shared->arenas_lock );

     /* For each exisiting arena... */
     fusion_list_foreach (l, _fusion_shared->arenas) {
          FusionArena *arena = (FusionArena*) l;

          /* Lock the arena.
             This would fail if the arena has been
             destroyed while waiting for the lock. */
          if (fusion_skirmish_prevail( &arena->lock ))
               continue;

          /* Check if the name matches. */
          if (! strcmp( arena->name, name )) {
               /* Check for an orphaned arena. */
               if (fusion_ref_zero_trylock( &arena->ref ) == FUSION_SUCCESS) {
                    FERROR( "orphaned arena '%s'!\n", name );

                    fusion_ref_unlock( &arena->ref );

//                    arena = NULL;
               }

               /* Unlock the list. */
               fusion_skirmish_dismiss( &_fusion_shared->arenas_lock );

               /* Return locked arena. */
               return arena;
          }

          /* Unlock mismatched arena. */
          fusion_skirmish_dismiss( &arena->lock );
     }

     /* If no arena name matched, create a new arena
        before unlocking the list again. */
     if (add) {
          FusionArena *arena = SHCALLOC( 1, sizeof(FusionArena) );

          /* Initialize lock and reference counter. */
          fusion_skirmish_init( &arena->lock );
          fusion_ref_init( &arena->ref );

          /* Give it the requested name. */
          arena->name = SHSTRDUP( name );

          /* Add it to the list. */
          fusion_list_prepend( &_fusion_shared->arenas, &arena->link );

          /* Lock the newly created arena. */
          fusion_skirmish_prevail( &arena->lock );

          /* Unlock the list. */
          fusion_skirmish_dismiss( &_fusion_shared->arenas_lock );

          /* Returned locked new arena. */
          return arena;
     }

     /* Unlock the list. */
     fusion_skirmish_dismiss( &_fusion_shared->arenas_lock );

     return NULL;
}

static void
unlock_arena( FusionArena *arena )
{
     DFB_ASSERT( arena != NULL );

     /* Unlock the arena. */
     fusion_skirmish_dismiss( &arena->lock );
}

#else

FusionResult
fusion_arena_enter (const char     *name,
                    ArenaEnterFunc  initialize,
                    ArenaEnterFunc  join,
                    void           *ctx,
                    FusionArena   **ret_arena,
                    int            *ret_error)
{
     int error;

     DFB_ASSERT( name != NULL );
     DFB_ASSERT( initialize != NULL );
     DFB_ASSERT( join != NULL );
     DFB_ASSERT( ret_arena != NULL );

     /* Always call 'initialize'. */
     error = initialize (NULL, ctx);

     /* Return the return value of the callback. */
     if (ret_error)
          *ret_error = error;

     return FUSION_SUCCESS;
}

FusionResult
fusion_arena_add_shared_field (FusionArena *arena,
                               const char  *name,
                               void        *data)
{
     DFB_ASSERT( data != NULL );
     DFB_ASSERT( name != NULL );

     return FUSION_SUCCESS;
}

FusionResult
fusion_arena_get_shared_field (FusionArena  *arena,
                               const char   *name,
                               void        **data)
{
     DFB_ASSERT( data != NULL );
     DFB_ASSERT( name != NULL );

     BUG( "should not call this in fake mode" );
     
     /* No field by that name has been found. */
     return FUSION_NOTEXISTENT;
}

FusionResult
fusion_arena_exit (FusionArena   *arena,
                   ArenaExitFunc  shutdown,
                   ArenaExitFunc  leave,
                   void          *ctx,
                   bool           emergency,
                   int           *ret_error)
{
     int error = 0;

     DFB_ASSERT( shutdown != NULL );

     /* Deinitialize everything. */
     error = shutdown( arena, ctx, emergency );

     /* Return the return value of the callback. */
     if (ret_error)
          *ret_error = error;

     return FUSION_SUCCESS;
}

#endif

