/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include <pthread.h>

#include "directfb.h"

#include "coretypes.h"

#include "reactor.h"

#include "misc/mem.h"

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
struct _Reactor {
     Reaction        *reactions; /* reactor listeners attached to node  */

     pthread_mutex_t  reactions_lock;
};


/****************
 *  Public API  *
 ****************/

Reactor *reactor_new ()
{
     Reactor           *reactor;

     reactor = (Reactor*)DFBMALLOC( sizeof(Reactor) );

     reactor->reactions = NULL;
     pthread_mutex_init( &reactor->reactions_lock, NULL );

     return reactor;
}

void reactor_attach (Reactor *reactor,
                     React    react,
                     void    *ctx)
{
     Reaction *reaction;

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
}

void reactor_detach (Reactor *reactor,
                     React    react,
                     void    *ctx)
{
     Reaction *r;

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
}

void reactor_dispatch (Reactor    *reactor,
                       const void *msg_data)
{
     Reaction *r, *to_free = NULL;

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
                    return;

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
}

void reactor_free (Reactor *reactor)
{
     pthread_mutex_lock( &reactor->reactions_lock );
  
     while (reactor->reactions) {
          Reaction *next = reactor->reactions->next;
          
          DFBFREE( reactor->reactions );

          reactor->reactions = next;
     }

     pthread_mutex_unlock( &reactor->reactions_lock );
  
     DFBFREE( reactor );
}

