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
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>

#include <pthread.h>

#include <misc/mem.h>

#include "fusion_types.h"
#include "lock.h"
#include "ref.h"
#include "arena.h"
#include "shmalloc.h"

#include "fusion_internal.h"


#ifndef FUSION_FAKE

/***************************
 *  Internal declarations  *
 ***************************/

/*
 *
 */
typedef struct {
     int  num;
     long ids[MAX_ARENA_NODES];
} ArenaNodes;

typedef struct {
     char  name[MAX_ARENA_FIELD_NAME_LENGTH+1];
     void *data;
} ArenaField;

/*
 *
 */
typedef struct {
     FusionSkirmish  lock;
     FusionRef       ref;

     ArenaField      fields[MAX_ARENA_FIELDS];
     ArenaNodes      nodes;       /* list of attached nodes           */
} ArenaShared;

/*
 *
 */
struct _FusionArena {
     /* shared data */
     ArenaShared *shared;     /* FusionArena internal shared data  */
     int          shared_shm; /* ID of shared memory segment */

     /* local data */
     void        *ctx;
};


/****************
 *  Public API  *
 ****************/

FusionArena *arena_enter (const char     *name,
                          ArenaEnterFunc  initialize,
                          ArenaEnterFunc  join,
                          void           *ctx)
{
     key_t              key;
     FusionArena       *arena;
     ArenaShared       *shared;
     ArenaEnterFunc     func;
     AcquisitionStatus  as;

     key = keygen (name, FUSION_KEY_ARENA);

     /* allocate local Arena data */
     arena = (FusionArena*)DFBCALLOC (1, sizeof(FusionArena));

     /* store local data */
     arena->ctx = ctx;

     /* acquire shared Arena data */
     as = _shm_acquire (key, sizeof(ArenaShared), &arena->shared_shm);
     if (as == AS_Failure) {
          DFBFREE (arena);
          return NULL;
     }

     arena->shared = shared = shmat (arena->shared_shm, NULL, 0);

     if (as == AS_Initialize) {
          memset (shared, 0, sizeof (ArenaShared));

          skirmish_init (&shared->lock);
          skirmish_prevail (&shared->lock);

          fusion_ref_init (&shared->ref);
          fusion_ref_up (&shared->ref, false);

          FDEBUG ("entered arena `%s´ (establishing)\n", name);

          func = initialize;
     }
     else {
          skirmish_prevail (&arena->shared->lock);

          if (arena->shared->nodes.num == MAX_ARENA_NODES) {
               FERROR ("maximum number of nodes reached (%d)\n", MAX_ARENA_NODES);

               shmdt (arena->shared);

               skirmish_dismiss (&arena->shared->lock);

               DFBFREE (arena);
               return NULL;
          }

          fusion_ref_up (&shared->ref, false);

          FDEBUG ("entered arena `%s´ (joining)\n", name);

          func = join;
     }

     arena->shared->nodes.ids[arena->shared->nodes.num++] = _fusion_id();

     FDEBUG ("added fid %x to arena nodes (%d)\n", _fusion_id(), arena->shared->nodes.num);

     if (func)
          func (arena, ctx);

     skirmish_dismiss (&arena->shared->lock);

     return arena;
}

FusionResult arena_add_shared_field (FusionArena *arena,
                                     void        *data,
                                     const char  *name)
{
     int          i;
     ArenaShared *shared;

     if (!arena || !data || !name)
          return FUSION_INVARG;

     if (strlen (name) > MAX_ARENA_FIELD_NAME_LENGTH)
          return FUSION_TOOLONG;

     //skirmish_prevail (&arena->shared->lock);

     shared = arena->shared;

     for (i=0; i<MAX_ARENA_FIELDS; i++) {
          if (shared->fields[i].data == NULL) {
               shared->fields[i].data = data;
               strcpy (shared->fields[i].name, name);

               skirmish_dismiss (&shared->lock);

               return FUSION_SUCCESS;
          }
     }

     //skirmish_dismiss (&shared->lock);

     return FUSION_LIMITREACHED;
}

FusionResult arena_get_shared_field (FusionArena  *arena,
                                     void        **data,
                                     const char   *name)
{
     int          i;
     ArenaShared *shared;

     if (!arena || !name)
          return FUSION_INVARG;

     if (strlen (name) > MAX_ARENA_FIELD_NAME_LENGTH)
          return FUSION_TOOLONG;

     //skirmish_prevail (&arena->shared->lock);

     shared = arena->shared;

     for (i=0; i<MAX_ARENA_FIELDS; i++) {
          if (strcmp (shared->fields[i].name, name) == 0) {
               *data = shared->fields[i].data;

               skirmish_dismiss (&shared->lock);

               return FUSION_SUCCESS;
          }
     }

     //skirmish_dismiss (&shared->lock);

     return FUSION_NOTEXISTENT;
}

void arena_exit (FusionArena   *arena,
                 ArenaExitFunc  shutdown,
                 ArenaExitFunc  leave,
                 bool           emergency)
{
     skirmish_prevail (&arena->shared->lock);

     if (arena->shared->nodes.num > 1) {
          int i;

          for (i = 0; i < arena->shared->nodes.num; i++) {
               if (arena->shared->nodes.ids[i] == _fusion_id()) {
                    arena->shared->nodes.ids[i] =
                    arena->shared->nodes.ids[arena->shared->nodes.num - 1];

                    arena->shared->nodes.num--;
               }
          }
     }

     fusion_ref_down (&arena->shared->ref, false);

     if (fusion_ref_zero_trylock (&arena->shared->ref) == FUSION_SUCCESS) {
          shutdown (arena, arena->ctx, emergency);

          fusion_ref_destroy (&arena->shared->ref);

          skirmish_destroy (&arena->shared->lock);
     }
     else {
          leave (arena, arena->ctx, emergency);
          skirmish_dismiss (&arena->shared->lock);
     }

     if (_shm_abolish (arena->shared_shm, arena->shared) == AB_Destroyed) {
     }
     else {
     }

     DFBFREE (arena);
}


/*****************************
 *  File internal functions  *
 *****************************/


#endif /* !FUSION_FAKE */

