/*
   (c) Copyright 2001  Denis Oliver Kropp <dok@directfb.org>
   All rights reserved.

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

#include "fusion_types.h"
#include "lock.h"
#include "ref.h"
#include "reactor.h"
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
  int   id;
  void *addr;
} SHMallocSegment;

/*
 *
 */
typedef struct {
  FusionSkirmish   lock;      /* access lock for segment table */
  int              shm_id;    /* shared memory segment id of this struct */

  int              num;
  SHMallocSegment  segments[MAX_SHMALLOC_SEGMENTS];

  int              subscribe_ref;
  int              consume_ref;
  FusionRef        refs[2];

  FusionReactor   *reactor;   /* reactor for attach/detach messages */
} SHMalloc;

/*
 * This message is sent to all nodes for automatic attach/detach.
 */
typedef struct {
  bool  attach; /* attach/detach? */
  int   id;     /* id of segment to attach to (attach == true) */
  void *addr;   /* address to attach to or detach from */
} SHMallocMsg;

/*
 *
 */
ReactionResult _shmalloc_react (const void *msg_data, void *ctx);


/*******************
 *  Internal data  *
 *******************/

/*
 *
 */
static SHMalloc *shm_alloc = NULL;


/****************
 *  Public API  *
 ****************/

void *shmalloc (int size)
{
  SHMallocMsg mmsg;

  if (!shm_alloc)
    {
      FERROR ("called without prior fusion_init!\n");
      return NULL;
    }

  skirmish_prevail (&shm_alloc->lock);

  if (shm_alloc->num == MAX_SHMALLOC_SEGMENTS)
    {
      FERROR ("maximum number of segments (%d) reached!\n",
          MAX_SHMALLOC_SEGMENTS);
      return NULL;
    }

  if ((mmsg.id = shmget (IPC_PRIVATE, PAGE_ALIGN(size),
             IPC_CREAT | IPC_EXCL | 0660)) < 0)
    {
      FPERROR ("shmget failed");
      skirmish_dismiss (&shm_alloc->lock);
      return NULL;
    }

  mmsg.attach = true;
  mmsg.addr   = _fusion_shmat (mmsg.id);

  if (!mmsg.addr)
    {
      if (shmctl (mmsg.id, IPC_RMID, 0))
    FPERROR ("shmctl (IPC_RMID) failed");

      skirmish_dismiss (&shm_alloc->lock);
      return NULL;
    }

  shm_alloc->segments[shm_alloc->num].id   = mmsg.id;
  shm_alloc->segments[shm_alloc->num].addr = mmsg.addr;

  /* Mark the segment for destruction to have it destroyed
     automatically after the last fusionee detached. */
  if (shmctl (shm_alloc->segments[shm_alloc->num].id, IPC_RMID, 0) < 0)
    FPERROR ("shmctl (IPC_RMID) failed");

  shm_alloc->num++;

  /*
   * Let all other nodes attach to the segment.
   * This pointer is NULL at the first call of this function,
   * because 'reactor_new()' used to intialize this pointer
   * does itself call this function. In this case the message
   * doesn't need to be dispatched anyway.
   */
  if (shm_alloc->reactor)
    {
      shm_alloc->consume_ref   = shm_alloc->subscribe_ref;
      shm_alloc->subscribe_ref = shm_alloc->consume_ref ? 0 : 1;

      ref_up (&shm_alloc->refs[shm_alloc->subscribe_ref]);
      ref_down (&shm_alloc->refs[shm_alloc->consume_ref]);

      reactor_dispatch (shm_alloc->reactor, &mmsg, false);

      ref_zero_lock (&shm_alloc->refs[shm_alloc->consume_ref]);
      ref_unlock (&shm_alloc->refs[shm_alloc->consume_ref]);
    }

  skirmish_dismiss (&shm_alloc->lock);

  return mmsg.addr;
}

void *shcalloc (int num, int size)
{
  void *data = shmalloc (num * size);

  memset (data, 0, num * size);

  return data;
}

void shmfree (void *addr)
{
  int i;
  SHMallocMsg shmm;

  if (!shm_alloc)
    {
      FERROR ("called without prior fusion_init!\n");
      return;
    }

  shmm.attach = false;
  shmm.addr   = addr;

  skirmish_prevail (&shm_alloc->lock);

  /* FIXME: hash table? */
  for (i=0; i<shm_alloc->num; i++)
    {
      if (shm_alloc->segments[i].addr == addr)
    {
      shm_alloc->segments[i] =
        shm_alloc->segments[shm_alloc->num - 1];

      shm_alloc->num--;


          shm_alloc->consume_ref   = shm_alloc->subscribe_ref;
          shm_alloc->subscribe_ref = shm_alloc->consume_ref ? 0 : 1;

          reactor_dispatch (shm_alloc->reactor, &shmm, true);

          ref_zero_lock (&shm_alloc->refs[shm_alloc->consume_ref]);
          ref_unlock (&shm_alloc->refs[shm_alloc->consume_ref]);

      break;
    }
    }

  skirmish_dismiss (&shm_alloc->lock);
}


/*******************************
 *  Fusion internal functions  *
 *******************************/

int _shmalloc_init()
{
  int               shm_id;
  AcquisitionStatus as;

  if (shm_alloc)
    {
      FERROR ("_shmalloc_init called multiple times!\n");
      return -1;
    }

  /* acquire shared global data structure */
  as = _shm_acquire (FUSION_KEY_PREFIX | FUSION_KEY_SHMALLOC,
                     sizeof(SHMalloc), &shm_id);
  if (as == AS_Failure)
    return -1;

  /* attach to it */
  shm_alloc = _fusion_shmat (shm_id);

  /* initialize allocation table or attach to existing segments */
  if (as == AS_Initialize)
    {
      memset (shm_alloc, 0, sizeof (SHMalloc));

      if (skirmish_init (&shm_alloc->lock))
        {
          _shm_abolish (shm_id, shm_alloc);
          shm_alloc = NULL;
          return -1;
        }

      if (ref_init (&shm_alloc->refs[0]))
        {
          skirmish_destroy (&shm_alloc->lock);
          _shm_abolish (shm_id, shm_alloc);
          shm_alloc = NULL;
          return -1;
        }

      if (ref_init (&shm_alloc->refs[1]))
        {
          ref_destroy (&shm_alloc->refs[0]);
          skirmish_destroy (&shm_alloc->lock);
          _shm_abolish (shm_id, shm_alloc);
          shm_alloc = NULL;
          return -1;
        }

      shm_alloc->num           = 0;
      shm_alloc->subscribe_ref = 0;
      shm_alloc->consume_ref   = 1;
      shm_alloc->shm_id        = shm_id;

      /* create reactor for attach/detach messages, calls shmalloc! */
      shm_alloc->reactor = reactor_new (sizeof(SHMallocMsg));
      if (!shm_alloc->reactor)
        {
          skirmish_destroy (&shm_alloc->lock);
          _shm_abolish (shm_id, shm_alloc);
          shm_alloc = NULL;
          return -1;
        }

      ref_up (&shm_alloc->refs[shm_alloc->subscribe_ref]);

      /* attach to the reactor for attach/detach messages */
      reactor_attach (shm_alloc->reactor, _shmalloc_react, NULL);
    }
  else
    {
      int i;

      skirmish_prevail (&shm_alloc->lock);

      for (i=0; i<shm_alloc->num; i++)
    {
      if (shmat (shm_alloc->segments[i].id,
             shm_alloc->segments[i].addr, 0) == (void*)(-1))
        {
          int j;

          FPERROR ("shmat to existing segment failed");

          for (j=0; j<i; j++)
        shmdt (shm_alloc->segments[i].addr);

              skirmish_dismiss (&shm_alloc->lock);
              _shm_abolish (shm_id, shm_alloc);
              shm_alloc = NULL;
              return -1;
        }
    }

      ref_up (&shm_alloc->refs[shm_alloc->subscribe_ref]);

      /* attach to the reactor for attach/detach messages */
      reactor_attach (shm_alloc->reactor, _shmalloc_react, NULL);

      skirmish_dismiss (&shm_alloc->lock);
    }

  return 0;
}

void _shmalloc_exit()
{
  int            i;
  FusionReactor *reactor;

  if (!shm_alloc)
    return;

  skirmish_prevail (&shm_alloc->lock);

  reactor = shm_alloc->reactor;

  /* detach from reactor for attach/detach messages */
  reactor_detach (reactor, _shmalloc_react, NULL);

  ref_down (&shm_alloc->refs[shm_alloc->subscribe_ref]);

  if (ref_zero_trylock (&shm_alloc->refs[shm_alloc->subscribe_ref]) == FUSION_SUCCESS)
    {
      //FIXME!      reactor_free (shm_alloc->reactor);

      ref_destroy (&shm_alloc->refs[0]);
      ref_destroy (&shm_alloc->refs[1]);

      skirmish_destroy (&shm_alloc->lock);
    }
  else
    {
      skirmish_dismiss (&shm_alloc->lock);
    }

  for (i=0; i<shm_alloc->num; i++)
    shmdt (shm_alloc->segments[i].addr);

  _shm_abolish (shm_alloc->shm_id, shm_alloc);

  shm_alloc = NULL;
}


/*****************************
 *  File internal functions  *
 *****************************/

ReactionResult _shmalloc_react (const void *msg_data, void *ctx)
{
  const SHMallocMsg *mmsg = (const SHMallocMsg*) msg_data;

  if (mmsg->attach)
    {
      void *addr = shmat (mmsg->id, mmsg->addr, 0);

      if (addr == (void*)(-1))
    FPERROR ("shmat failed");
      else if (addr != mmsg->addr)
    FERROR ("address returned by shmat (%p) does not "
        "match the requested one (%p)!\n", addr, mmsg->addr);

      FDEBUG ("attached %p\n", mmsg->addr);
    }
  else
    {
      FDEBUG ("detaching %p\n", mmsg->addr);

      if (shmdt (mmsg->addr))
    FPERROR ("shmdt failed");
    }

  ref_up (&shm_alloc->refs[shm_alloc->subscribe_ref]);
  ref_down (&shm_alloc->refs[shm_alloc->consume_ref]);

  return RS_OK;
}

#endif /* !FUSION_FAKE */

