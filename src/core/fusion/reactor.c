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
#include <signal.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>

#include <pthread.h>

#include <core/sig.h>
#include <misc/mem.h>

#include "fusion_types.h"
#include "list.h"
#include "lock.h"
#include "ref.h"
#include "shmalloc.h"
#include "reactor.h"

#include "fusion_internal.h"

#ifndef FUSION_FAKE

/***************************
 *  Internal declarations  *
 ***************************/

/*
 *
 */
typedef struct {
     FusionLink link;  /* fusion list link */

     React      react; /* the reaction callback  */
     void      *ctx;   /* optional callback context */
} Reaction;

/*
 *
 */
struct _FusionReactor {
     FusionSkirmish  lock;      /* access synchronization lock         */

     int             queue;     /* message queue id                    */
     int             msg_size;  /* size of sent messages               */

     int             nodes;     /* number of attached fusionees        */

     struct {
          int           id;        /* fusion id                           */
          FusionLink   *reactions; /* local list of reactions             */
          pthread_t     receiver;  /* receiving thread of the node        */
          FusionRef     ref;       /* reference for receiver              */
     } node[MAX_REACTOR_NODES];
};

/*
 * Thread that receives the messages and handles local reactions.
 */
static void *_reactor_receive (void *arg);

/*
 * Locally dispatch a message.
 */
static void  _reactor_process_reactions (FusionLink **reactions, const void *msg_data);

/*
 * Get the index to our node.
 */
static int   _reactor_get_node_index (const FusionReactor *reactor);

/*
 * Get the index to the first free node.
 */
static int   _reactor_get_free_index (const FusionReactor *reactor);


/****************
 *  Public API  *
 ****************/

FusionReactor *
reactor_new (const int msg_size)
{
     FusionReactor *reactor;

     /* allocate shared reactor data */
     reactor = shcalloc (1, sizeof (FusionReactor));

     /* set the static message size, should we make dynamic? (TODO?) */
     reactor->msg_size = msg_size;

     /* create the message queue for dispatching (TODO: drop message queue usage) */
     if ((reactor->queue = msgget (IPC_PRIVATE, IPC_CREAT | IPC_EXCL | 0660)) < 0) {
          FPERROR ("msgget for creating the queue failed");

          shfree (reactor);
          return NULL;
     }

     /* initialize skirmish */
     skirmish_init (&reactor->lock);

     return reactor;
}

FusionResult
reactor_attach (FusionReactor *reactor,
                React          react,
                void          *ctx)
{
     int       index;
     Reaction *reaction;

     /* lock reactor */
     skirmish_prevail (&reactor->lock);

     /* find our node, needs to be replaced by a binary space tree (TODO) */
     index = _reactor_get_node_index (reactor);

     /* if our node hasn't been found add us to the nodes */
     if (index < 0) {
          /* check for maximum number of nodes, currently limited (TODO) */
          if (index >= MAX_REACTOR_NODES) {
               FERROR ("maximum number of reactor nodes (%d) reached!\n",
                       MAX_REACTOR_NODES);
               skirmish_dismiss (&reactor->lock);
               return FUSION_LIMITREACHED;
          }

          /* increase the number of nodes and
             write our fusion id into the last node (the new one) */
          index = _reactor_get_free_index (reactor);
          if (index < 0) {
               FERROR ("something went wrong, couldn't find a free node!\n");
               skirmish_dismiss (&reactor->lock);
               return FUSION_BUG;
          }

          reactor->node[index].id = _fusion_id();

          reactor->nodes++;

          fusion_ref_init (&reactor->node[index].ref);

          /* start our local receiver thread and detach it */
          pthread_create (&reactor->node[index].receiver,
                          NULL, _reactor_receive, reactor);
          pthread_detach (reactor->node[index].receiver);

          /* wait for receiver thread to get ready */
          while (fusion_ref_zero_trylock (&reactor->node[index].ref) == FUSION_SUCCESS) {
               fusion_ref_unlock (&reactor->node[index].ref);
               sched_yield();
          }
     }

     /* allocate information for local dispatching */
     reaction = (Reaction*)calloc (1, sizeof(Reaction));

     /* fill out callback information */
     reaction->react = react;
     reaction->ctx   = ctx;

     /* prepend the reaction to the local reaction list */
     fusion_list_prepend (&reactor->node[index].reactions, (FusionLink*) reaction);

     /* unlock reactor */
     skirmish_dismiss (&reactor->lock);

     return FUSION_SUCCESS;
}

FusionResult
reactor_detach (FusionReactor *reactor,
                React          react,
                void          *ctx)
{
     int         index;
     FusionLink *link;
     FusionLink *remove = NULL;

     /* lock reactor */
     skirmish_prevail (&reactor->lock);

     /* find our node and return if it hasn't been found */
     index = _reactor_get_node_index (reactor);
     if (index < 0) {
          skirmish_dismiss (&reactor->lock);
          return FUSION_BUG;
     }

     /* find the reaction to remove */
     fusion_list_foreach (link, reactor->node[index].reactions) {
          Reaction *reaction = (Reaction*) link;

          /* found if reaction callback and context match */
          if (reaction->react == react && reaction->ctx == ctx) {
               remove = link;
               break;
          }
     }

     /* remove and free it if found, should return error otherwise (TODO) */
     if (remove) {
          fusion_list_remove (&reactor->node[index].reactions, remove);
          free (remove);
     }

     /* if it was the last reaction cancel our receiver thread and free the node */
     if (!reactor->node[index].reactions) {
          pthread_cancel (reactor->node[index].receiver);

          fusion_ref_zero_lock (&reactor->node[index].ref);
          fusion_ref_destroy (&reactor->node[index].ref);

          reactor->node[index].id = 0;
          reactor->nodes--;
     }

     /* unlock reactor */
     skirmish_dismiss (&reactor->lock);

     return FUSION_SUCCESS;
}

FusionResult
reactor_dispatch (FusionReactor *reactor,
                  const void    *msg_data,
                  bool           self)
{
     int   i;
     void *message;

     /* lock reactor */
     skirmish_prevail (&reactor->lock);

     /* allocate 'real' message memory (message id + message) */
     message = alloca (sizeof(long) + reactor->msg_size);

     /* copy the original message data into the new area */
     memcpy ((void*)((long*)message+1), msg_data, reactor->msg_size);

     /* loop through all nodes */
     for (i = 0; i < MAX_REACTOR_NODES; i++) {
          /* check if it's a free node */
          if (!reactor->node[i].id)
               continue;

          /* <DEBUG> */
          if (fusion_ref_zero_trylock (&reactor->node[i].ref) == FUSION_SUCCESS) {
               printf(__FUNCTION__": node '%d' with id '%d' is dead, freeing it!\n",
                      i, reactor->node[i].id);
               fusion_ref_destroy (&reactor->node[i].ref);
               reactor->node[i].id        = 0;
               reactor->node[i].reactions = NULL;
               reactor->nodes--;
               continue;
          }
          /* </DEBUG> */

          /* set the message id to the destinations fusion id */
          *((long*)message) = reactor->node[i].id;

          /* if this node belongs to us... */
          if (reactor->node[i].id == _fusion_id()) {
               /* ...and the message is send to ourself, too... */
               if (self) {
#if 1
                    FDEBUG ("dispatching locally for fid %x (%d byte)\n",
                            reactor->node[i].id, reactor->msg_size);

                    /* ...dispatch it locally (directly) */
                    _reactor_process_reactions (&reactor->node[i].reactions, msg_data);

                    /* if there's no remaining reaction (reactions may be removed
                       because of RS_REMOVE) free the node */
                    if (!reactor->node[i].reactions) {
                         pthread_cancel (reactor->node[i].receiver);

                         fusion_ref_zero_lock (&reactor->node[i].ref);
                         fusion_ref_destroy (&reactor->node[i].ref);

                         reactor->node[i].id = 0;
                         reactor->nodes--;
                    }
#else
                    FDEBUG ("sending to queue %d for fid %x (%d byte), that's me\n",
                            reactor->queue, reactor->node[i].id, reactor->msg_size);

                    /* send the complete message (fusion id + data) */
                    if (msgsnd (reactor->queue, message, reactor->msg_size, IPC_NOWAIT) < 0)
                         if (errno != EAGAIN)
                              FPERROR ("msgsnd failed");
#endif
               }
          }
          else {
               FDEBUG ("sending to queue %d for fid %x (%d byte)\n",
                       reactor->queue, reactor->node[i].id, reactor->msg_size);

               /* send the complete message (fusion id + data) */
               if (msgsnd (reactor->queue, message, reactor->msg_size, IPC_NOWAIT) < 0)
                    if (errno != EAGAIN)
                         FPERROR ("msgsnd failed");
          }
     }

     /* unlock reactor */
     skirmish_dismiss (&reactor->lock);

     return FUSION_SUCCESS;
}

FusionResult
reactor_free (FusionReactor *reactor)
{
     int i;

     /* lock reactor */
     skirmish_prevail (&reactor->lock);

     msgctl (reactor->queue, IPC_RMID, NULL);

     if (reactor->nodes) {
          /* loop through remaining nodes and print error messages */
          for (i=0; i<MAX_REACTOR_NODES; i++) {
               if (!reactor->node[i].id)
                    continue;

               FDEBUG ("reactor_free: fusionee '%d' still attached (reactions: %p)!\n",
                       reactor->node[i].id, reactor->node[i].reactions);
               
               fusion_ref_destroy (&reactor->node[i].ref);
          }

          //kill (getpid(), 5);
     }

     /* unlock reactor and destroy the skirmish */
     skirmish_dismiss (&reactor->lock);
     skirmish_destroy (&reactor->lock);

     /* free shared reactor data */
     shfree (reactor);

     return FUSION_SUCCESS;
}


/*****************************
 *  File internal functions  *
 *****************************/

void *_reactor_receive (void *arg)
{
     int            index;
     void          *message;
     FusionReactor *reactor = (FusionReactor*) arg;

     dfb_sig_block_all();

     /* find our node and return if it hasn't been found */
     index = _reactor_get_node_index (reactor);
     if (index < 0) {
          FERROR ("_reactor_receive: "
                  "could not find node with fusion id '%d'!\n", _fusion_id());
          return NULL;
     }

     if (fusion_ref_up (&reactor->node[index].ref, false))
          return NULL;

     /* allocate local buffer for received messages */
     message = alloca (sizeof(long) + reactor->msg_size);

     while (true) {
          pthread_testcancel();

          /* receive the next messages matching our fusion id */
          if (msgrcv (reactor->queue, message,
                      reactor->msg_size, _fusion_id(), 0) < 0)
          {
               if (errno == EINTR)
                    continue;

               if (errno == EIDRM)
                    FDEBUG("reactor vanished");
               else
                    FPERROR("msgrcv failed");
               
               return NULL;
          }

          pthread_testcancel();

          /* dispatch the message locally */
          _reactor_process_reactions (&reactor->node[index].reactions, (void*)((long*)message+1));

          pthread_testcancel();

          /* if there's no remaining reaction (reactions may be removed
             because of RS_REMOVE) free the node and exit this thread */
          if (!reactor->node[index].reactions) {
               /*  skirmish_prevail (&reactor->lock);
                   reactor->node[index] = reactor->node[ --reactor->nodes ];
                   skirmish_dismiss (&reactor->lock);*/

               return NULL;
          }
     }

     return NULL;
}

static void _reactor_process_reactions (FusionLink **reactions, const void *msg_data)
{
     FusionLink *link = *reactions;

     /* if any reactions (should not happen, because
        the local receiver thread is canceled after the last local detach */
     if (!link) {
          FDEBUG ("no reactions for dispatching locally!");
          return;
     }

     /* loop through local reactions */
     do {
          Reaction   *r   = (Reaction*) link;
          FusionLink *del = NULL;

          /* invoke reaction callback, mark deletion if it returns RS_REMOVE */
          if (r->react (msg_data, r->ctx) == RS_REMOVE)
               del = link;

          /* fetch the next list entry */
          link = link->next;

          /* if RS_REMOVE has been returned remove the reaction */
          if (del) {
               /* remove from list and free */
               fusion_list_remove (reactions, del);
               free (del);
          }
     } while (link);
}

static int _reactor_get_node_index (const FusionReactor *reactor)
{
     int i;
     int my_id = _fusion_id();

     /* loop through nodes and check the id, should be a binary space tree (TODO) */
     for (i=0; i<MAX_REACTOR_NODES; i++) {
          if (reactor->node[i].id == my_id)
               break;
     }

     /* if the id wasn't found return -1 */
     if (i == MAX_REACTOR_NODES)
          return -1;

     /* return index to node */
     return i;
}

static int _reactor_get_free_index (const FusionReactor *reactor)
{
     int i;

     /* loop through nodes and check the id, should be a binary space tree (TODO) */
     for (i=0; i<MAX_REACTOR_NODES; i++) {
          if (reactor->node[i].id == 0)
               break;
     }

     /* if no free node was found return -1 */
     if (i == MAX_REACTOR_NODES)
          return -1;

     /* return index to node */
     return i;
}

#else /* !FUSION_FAKE */


/***************************
 *  Internal declarations  *
 ***************************/

/*
 *
 */
typedef struct _Reaction {
     React  react;
     void  *ctx;

     struct _Reaction *next;
     struct _Reaction *prev;
} Reaction;

/*
 *
 */
struct _FusionReactor {
     Reaction        *reactions; /* reactor listeners attached to node  */

     pthread_mutex_t  reactions_lock;
};


/****************
 *  Public API  *
 ****************/

FusionReactor *
reactor_new (int msg_size)
{
     FusionReactor           *reactor;

     reactor = (FusionReactor*)DFBMALLOC( sizeof(FusionReactor) );

     reactor->reactions = NULL;
     pthread_mutex_init( &reactor->reactions_lock, NULL );

     return reactor;
}

FusionResult
reactor_attach (FusionReactor *reactor,
                React          react,
                void          *ctx)
{
     Reaction *reaction;

//     DEBUGMSG("DirectFB/core/fusion: reactor_attach (%p, %p, %p) entered\n",
//              reactor, react, ctx);
     
     reaction = (Reaction*)DFBCALLOC( 1, sizeof(Reaction) );

     reaction->react = react;
     reaction->ctx   = ctx;

     pthread_mutex_lock( &reactor->reactions_lock );

     if (reactor->reactions) {
          reaction->next = reactor->reactions;
          reactor->reactions->prev = reaction;
     }

     reactor->reactions = reaction;

     pthread_mutex_unlock( &reactor->reactions_lock );
     
//     DEBUGMSG("DirectFB/core/fusion: reactor_attach (%p, %p, %p) exitting\n",
//              reactor, react, ctx);

     return FUSION_SUCCESS;
}

FusionResult
reactor_detach (FusionReactor *reactor,
                React          react,
                void          *ctx)
{
     Reaction *r;

//     DEBUGMSG("DirectFB/core/fusion: reactor_detach (%p, %p, %p) entered\n",
//              reactor, react, ctx);
     
     pthread_mutex_lock( &reactor->reactions_lock );

     r = reactor->reactions;

     while (r) {
          if (r->react == react  &&  r->ctx == ctx) {
               if (r->next)
                    r->next->prev = r->prev;

               if (r->prev)
                    r->prev->next = r->next;
               else
                    reactor->reactions = r->next;

               DFBFREE( r );

               break;
          }

          r = r->next;
     }

     pthread_mutex_unlock( &reactor->reactions_lock );

//     DEBUGMSG("DirectFB/core/fusion: reactor_detach (%p, %p, %p) exitting\n",
//              reactor, react, ctx);

     return FUSION_SUCCESS;
}

FusionResult
reactor_dispatch (FusionReactor *reactor,
                  const void    *msg_data,
                  bool           self)
{
     Reaction *r, *to_free = NULL;

     if (!self)
          return FUSION_SUCCESS;

//     DEBUGMSG("DirectFB/core/fusion: reactor_dispatch (%p) entered\n", reactor);
     
     pthread_mutex_lock( &reactor->reactions_lock );

     r = reactor->reactions;

     while (r) {
          switch (r->react( msg_data, r->ctx )) {
               case RS_REMOVE:
                    if (r->next)
                         r->next->prev = r->prev;

                    if (r->prev)
                         r->prev->next = r->next;
                    else
                         reactor->reactions = r->next;

                    to_free = r;
                    break;

               case RS_DROP:
                    pthread_mutex_unlock( &reactor->reactions_lock );
//                    DEBUGMSG("DirectFB/core/fusion: reactor_dispatch (%p) exitting\n", reactor);
                    return FUSION_SUCCESS;

               case RS_OK:
                    ;
          }

          r = r->next;

          if (to_free) {
               DFBFREE( to_free );
               to_free = NULL;
          }
     }

     pthread_mutex_unlock( &reactor->reactions_lock );
     
//     DEBUGMSG("DirectFB/core/fusion: reactor_dispatch (%p) exitting\n", reactor);

     return FUSION_SUCCESS;
}

FusionResult
reactor_free (FusionReactor *reactor)
{
//     DEBUGMSG("DirectFB/core/fusion: reactor_free (%p) entered\n", reactor);
     
     pthread_mutex_lock( &reactor->reactions_lock );

     while (reactor->reactions) {
          Reaction *next = reactor->reactions->next;

          DFBFREE( reactor->reactions );

          reactor->reactions = next;
     }

     pthread_mutex_unlock( &reactor->reactions_lock );

     DFBFREE( reactor );
     
//     DEBUGMSG("DirectFB/core/fusion: reactor_free (%p) exitting\n", reactor);

     return FUSION_SUCCESS;
}


#endif /* FUSION_FAKE */

