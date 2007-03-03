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
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>

#include <pthread.h>

#include <direct/debug.h>
#include <direct/list.h>
#include <direct/mem.h>
#include <direct/messages.h>

#include <fusion/build.h>
#include <fusion/types.h>
#include <fusion/lock.h>
#include <fusion/ref.h>
#include <fusion/arena.h>
#include <fusion/shm/shm.h>
#include <fusion/shmalloc.h>

#include "fusion_internal.h"


#if FUSION_BUILD_MULTI

typedef struct {
     DirectLink  link;

     char       *name;
     void       *data;
} ArenaField;

struct __Fusion_FusionArena {
     DirectLink         link;

     int                magic;

     FusionWorldShared *shared;

     FusionSkirmish     lock;
     FusionRef          ref;

     char              *name;

     DirectLink        *fields;
};

/**********************************************************************************************************************/

static FusionArena *lock_arena  ( FusionWorld *world,
                                  const char  *name,
                                  bool         add );

static void         unlock_arena( FusionArena *arena );

/**********************************************************************************************************************/

DirectResult
fusion_arena_enter (FusionWorld     *world,
                    const char      *name,
                    ArenaEnterFunc   initialize,
                    ArenaEnterFunc   join,
                    void            *ctx,
                    FusionArena    **ret_arena,
                    int             *ret_error)
{
     FusionArena       *arena;
     FusionWorldShared *shared;
     ArenaEnterFunc     func;
     int                error = 0;

     D_MAGIC_ASSERT( world, FusionWorld );

     D_ASSERT( name != NULL );
     D_ASSERT( initialize != NULL );
     D_ASSERT( join != NULL );
     D_ASSERT( ret_arena != NULL );

     shared = world->shared;

     D_MAGIC_ASSERT( shared, FusionWorldShared );

     /* Lookup arena and lock it. If it doesn't exist create it. */
     arena = lock_arena( world, name, true );
     if (!arena)
          return DFB_FAILURE;

     /* Check if we are the first. */
     if (fusion_ref_zero_trylock( &arena->ref ) == DFB_OK) {
          D_DEBUG ("Fusion/Arena: entering arena '%s' (establishing)\n", name);

          /* Call 'initialize' later. */
          func = initialize;

          /* Unlock the reference counter. */
          fusion_ref_unlock( &arena->ref );
     }
     else {
          D_DEBUG ("Fusion/Arena: entering arena '%s' (joining)\n", name);

          fusion_shm_attach_unattached( world );

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
          DirectLink *l = arena->fields;

          fusion_ref_down (&arena->ref, false);

          if (func == initialize) {
               /* Destroy fields. */
               while (l) {
                    DirectLink *next  = l->next;
                    ArenaField *field = (ArenaField*) l;

                    /* Free allocated memory. */
                    SHFREE( shared->main_pool, field->name );
                    SHFREE( shared->main_pool, field );

                    l = next;
               }

               /* Destroy reference counter. */
               fusion_ref_destroy( &arena->ref );

               /* Destroy the arena lock. This has to happen before
                  locking the list. Otherwise a dead lock with lock_arena()
                  below could occur. */
               fusion_skirmish_destroy( &arena->lock );

               /* Lock the list and remove the arena. */
               fusion_skirmish_prevail( &shared->arenas_lock );
               direct_list_remove( &shared->arenas, &arena->link );
               fusion_skirmish_dismiss( &shared->arenas_lock );

               D_MAGIC_CLEAR( arena );

               /* Free allocated memory. */
               SHFREE( shared->main_pool, arena->name );
               SHFREE( shared->main_pool, arena );

               return DFB_OK;
          }
     }

     /* Unlock the arena. */
     unlock_arena( arena );

     return DFB_OK;
}

DirectResult
fusion_arena_add_shared_field (FusionArena *arena,
                               const char  *name,
                               void        *data)
{
     ArenaField        *field;
     FusionWorldShared *shared;

     D_ASSERT( arena != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( name != NULL );

     D_MAGIC_ASSERT( arena, FusionArena );

     shared = arena->shared;

     D_MAGIC_ASSERT( shared, FusionWorldShared );

     /* Lock the arena. */
     if (fusion_skirmish_prevail( &arena->lock ))
          return DFB_FAILURE;

     /* Allocate memory for the field information. */
     field = SHCALLOC( shared->main_pool, 1, sizeof(ArenaField) );
     if (!field) {
          fusion_skirmish_dismiss( &arena->lock );
          return DFB_FAILURE;
     }

     /* Give it the requested name. */
     field->name = SHSTRDUP( shared->main_pool, name );

     /* Assign the data pointer. */
     field->data = data;

     /* Add the field to the list. */
     direct_list_prepend( &arena->fields, &field->link );

     /* Unlock the arena. */
     fusion_skirmish_dismiss( &arena->lock );

     return DFB_OK;
}

DirectResult
fusion_arena_get_shared_field (FusionArena  *arena,
                               const char   *name,
                               void        **data)
{
     DirectLink *l;

     D_ASSERT( arena != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( name != NULL );

     D_MAGIC_ASSERT( arena, FusionArena );

     /* Lock the arena. */
     if (fusion_skirmish_prevail( &arena->lock ))
          return DFB_FAILURE;

     /* For each field in the arena... */
     direct_list_foreach (l, arena->fields) {
          ArenaField *field = (ArenaField*) l;

          /* Check if the name matches. */
          if (! strcmp( field->name, name )) {
               /* Get its data pointer. */
               *data = field->data;

               /* Unlock the arena. */
               fusion_skirmish_dismiss( &arena->lock );

               /* Field has been found. */
               return DFB_OK;
          }
     }

     /* Unlock the arena. */
     fusion_skirmish_dismiss( &arena->lock );

     /* No field by that name has been found. */
     return DFB_ITEMNOTFOUND;
}

DirectResult
fusion_arena_exit (FusionArena   *arena,
                   ArenaExitFunc  shutdown,
                   ArenaExitFunc  leave,
                   void          *ctx,
                   bool           emergency,
                   int           *ret_error)
{
     int error = 0;
     FusionWorldShared *shared;

     D_MAGIC_ASSERT( arena, FusionArena );
     D_ASSERT( shutdown != NULL );

     shared = arena->shared;

     D_MAGIC_ASSERT( shared, FusionWorldShared );

     /* Lock the arena. */
     if (fusion_skirmish_prevail( &arena->lock ))
          return DFB_FAILURE;

     /* Decrease reference counter. */
     fusion_ref_down( &arena->ref, false );

     /* If we are the last... */
     if (fusion_ref_zero_trylock( &arena->ref ) == DFB_OK) {
          DirectLink *l = arena->fields;

          /* Deinitialize everything. */
          error = shutdown( arena, ctx, emergency );

          /* Destroy fields. */
          while (l) {
               DirectLink *next  = l->next;
               ArenaField *field = (ArenaField*) l;

               /* Free allocated memory. */
               SHFREE( shared->main_pool, field->name );
               SHFREE( shared->main_pool, field );

               l = next;
          }

          /* Destroy reference counter. */
          fusion_ref_destroy( &arena->ref );

          /* Destroy the arena lock. This has to happen before
             locking the list. Otherwise a dead lock with lock_arena()
             below could occur. */
          fusion_skirmish_destroy( &arena->lock );

          /* Lock the list and remove the arena. */
          fusion_skirmish_prevail( &shared->arenas_lock );
          direct_list_remove( &shared->arenas, &arena->link );
          fusion_skirmish_dismiss( &shared->arenas_lock );

          D_MAGIC_CLEAR( arena );

          /* Free allocated memory. */
          SHFREE( shared->main_pool, arena->name );
          SHFREE( shared->main_pool, arena );
     }
     else {
          if (!leave) {
               fusion_ref_up( &arena->ref, false );
               fusion_skirmish_dismiss( &arena->lock );
               return DFB_BUSY;
          }

          /* Simply leave the arena. */
          error = leave( arena, ctx, emergency );

          /* Unlock the arena. */
          fusion_skirmish_dismiss( &arena->lock );
     }

     /* Return the return value of the callback. */
     if (ret_error)
          *ret_error = error;

     return DFB_OK;
}


/*****************************
 *  File internal functions  *
 *****************************/

static FusionArena *
lock_arena( FusionWorld *world,
            const char  *name,
            bool         add )
{
     FusionArena       *arena;
     FusionWorldShared *shared;

     D_MAGIC_ASSERT( world, FusionWorld );

     shared = world->shared;

     D_MAGIC_ASSERT( shared, FusionWorldShared );

     /* Lock the list. */
     if (fusion_skirmish_prevail( &shared->arenas_lock ))
          return NULL;

     /* For each exisiting arena... */
     direct_list_foreach (arena, shared->arenas) {
          /* Lock the arena.
             This would fail if the arena has been
             destroyed while waiting for the lock. */
          if (fusion_skirmish_prevail( &arena->lock ))
               continue;

          D_MAGIC_ASSERT( arena, FusionArena );

          /* Check if the name matches. */
          if (! strcmp( arena->name, name )) {
               /* Check for an orphaned arena. */
               if (fusion_ref_zero_trylock( &arena->ref ) == DFB_OK) {
                    D_ERROR( "Fusion/Arena: orphaned arena '%s'!\n", name );

                    fusion_ref_unlock( &arena->ref );

//                    arena = NULL;
               }

               /* Unlock the list. */
               fusion_skirmish_dismiss( &shared->arenas_lock );

               /* Return locked arena. */
               return arena;
          }

          /* Unlock mismatched arena. */
          fusion_skirmish_dismiss( &arena->lock );
     }

     /* If no arena name matched, create a new arena
        before unlocking the list again. */
     if (add) {
          char         buf[64];
          FusionArena *arena = SHCALLOC( shared->main_pool, 1, sizeof(FusionArena) );

          if (!arena) {
               D_OOSHM();
               fusion_skirmish_dismiss( &shared->arenas_lock );
               return NULL;
          }

          snprintf( buf, sizeof(buf), "Arena '%s'", name );

          /* Initialize lock and reference counter. */
          if (fusion_skirmish_init( &arena->lock, buf, world )) {
               SHFREE( shared->main_pool, arena );
               fusion_skirmish_dismiss( &shared->arenas_lock );
               return NULL;
          }

          if (fusion_ref_init( &arena->ref, buf, world )) {
               fusion_skirmish_destroy( &arena->lock );
               SHFREE( shared->main_pool, arena );
               fusion_skirmish_dismiss( &shared->arenas_lock );
               return NULL;
          }

          /* Give it the requested name. */
          arena->name = SHSTRDUP( shared->main_pool, name );

          /* Add it to the list. */
          direct_list_prepend( &shared->arenas, &arena->link );

          /* Lock the newly created arena. */
          fusion_skirmish_prevail( &arena->lock );

          /* Unlock the list. */
          fusion_skirmish_dismiss( &shared->arenas_lock );

          arena->shared = shared;

          D_MAGIC_SET( arena, FusionArena );

          /* Returned locked new arena. */
          return arena;
     }

     /* Unlock the list. */
     fusion_skirmish_dismiss( &shared->arenas_lock );

     return NULL;
}

static void
unlock_arena( FusionArena *arena )
{
     D_ASSERT( arena != NULL );

     D_MAGIC_ASSERT( arena, FusionArena );

     /* Unlock the arena. */
     fusion_skirmish_dismiss( &arena->lock );
}

#else

DirectResult
fusion_arena_enter (FusionWorld    *world,
                    const char     *name,
                    ArenaEnterFunc  initialize,
                    ArenaEnterFunc  join,
                    void           *ctx,
                    FusionArena   **ret_arena,
                    int            *ret_error)
{
     int error;

     D_ASSERT( name != NULL );
     D_ASSERT( initialize != NULL );
     D_ASSERT( join != NULL );
     D_ASSERT( ret_arena != NULL );

     /* Always call 'initialize'. */
     error = initialize (NULL, ctx);

     /* Return the return value of the callback. */
     if (ret_error)
          *ret_error = error;

     return DFB_OK;
}

DirectResult
fusion_arena_add_shared_field (FusionArena *arena,
                               const char  *name,
                               void        *data)
{
     D_ASSERT( data != NULL );
     D_ASSERT( name != NULL );

     return DFB_OK;
}

DirectResult
fusion_arena_get_shared_field (FusionArena  *arena,
                               const char   *name,
                               void        **data)
{
     D_ASSERT( data != NULL );
     D_ASSERT( name != NULL );

     D_BUG( "should not call this in fake mode" );

     /* No field by that name has been found. */
     return DFB_ITEMNOTFOUND;
}

DirectResult
fusion_arena_exit (FusionArena   *arena,
                   ArenaExitFunc  shutdown,
                   ArenaExitFunc  leave,
                   void          *ctx,
                   bool           emergency,
                   int           *ret_error)
{
     int error = 0;

     D_ASSERT( shutdown != NULL );

     /* Deinitialize everything. */
     error = shutdown( arena, ctx, emergency );

     /* Return the return value of the callback. */
     if (ret_error)
          *ret_error = error;

     return DFB_OK;
}

#endif

