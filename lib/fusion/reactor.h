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

#ifndef __FUSION__REACTOR_H__
#define __FUSION__REACTOR_H__

#include <direct/list.h>

#include <fusion/types.h>
#include <fusion/lock.h>

typedef enum {
     RS_OK,
     RS_REMOVE,
     RS_DROP
} ReactionResult;

typedef ReactionResult (*ReactionFunc)( const void *msg_data,
                                        void       *ctx );

typedef struct {
     DirectLink    link;
     ReactionFunc  func;
     void         *ctx;
     bool          attached;
} Reaction;

typedef struct {
     DirectLink  link;
     int         index;
     void       *ctx;
     bool        attached;
} GlobalReaction;


/*
 * Create a new reactor configured for the specified message data size.
 */
FusionReactor *fusion_reactor_new          ( int                 msg_size,
                                             const char         *name,
                                             const FusionWorld  *world );

/*
 * Destroy the reactor.
 */
DirectResult   fusion_reactor_destroy      ( FusionReactor      *reactor );

/*
 * Free the reactor.
 */
DirectResult   fusion_reactor_free         ( FusionReactor      *reactor );


/*
 * Makes the reactor use the specified lock for managing global reactions.
 *
 * After creating the reactor a global default lock is set which is created
 * by Fusion once during initialization.
 *
 * To avoid dead locks caused by alternating lock orders of the global reaction
 * lock and another lock, the default lock is replaced by the other lock.
 */
DirectResult   fusion_reactor_set_lock     ( FusionReactor      *reactor,
                                             FusionSkirmish     *skirmish );

/*
 * Attach a local reaction to the reactor.
 */
DirectResult   fusion_reactor_attach       ( FusionReactor      *reactor,
                                             ReactionFunc        func,
                                             void               *ctx,
                                             Reaction           *reaction );

/*
 * Detach an attached local reaction from the reactor.
 */
DirectResult   fusion_reactor_detach       ( FusionReactor      *reactor,
                                             Reaction           *reaction );


/*
 * Attach a global reaction to the reactor.
 *
 * It's always called directly, no matter which Fusionee calls fusion_reactor_dispatch().
 * Any data referenced by the reaction function has to be in shared memory, unless it uses a
 * mechanism to lookup a local counter part or representative, based on shared information.
 *
 * A global reaction is not defined directly as a function pointer, because that's always a
 * local address. Instead, it's specified by an index into a built in function table that
 * must be passed to fusion_reactor_dispatch() each time it is called.
 */
DirectResult   fusion_reactor_attach_global( FusionReactor      *reactor,
                                             int                 index,
                                             void               *ctx,
                                             GlobalReaction     *reaction );

/*
 * Detach an attached global reaction from the reactor.
 */
DirectResult   fusion_reactor_detach_global( FusionReactor      *reactor,
                                             GlobalReaction     *reaction );

/*
 * Dispatch a message to any attached reaction.
 *
 * Setting 'self' to false excludes the caller's local reactions.
 */
DirectResult   fusion_reactor_dispatch     ( FusionReactor      *reactor,
                                             const void         *msg_data,
                                             bool                self,
                                             const ReactionFunc *globals );

/*
 * Dispatch a message to any attached reaction with a given size. Instead of
 * using the size defined by the reactor, the caller can specify the size of
 * the data.
 *
 * Setting 'self' to false excludes the caller's local reactions.
 */
DirectResult   fusion_reactor_sized_dispatch( FusionReactor      *reactor,
                                              const void         *msg_data,
                                              int                 msg_size,
                                              bool                self,
                                              const ReactionFunc *globals );
                                              
/*
 * Specify whether message dispatching must happen synchronously or not.
 * 
 * Set 'sync' to false to dispatch messages immediatly to local reactions.
 */
DirectResult   fusion_reactor_sync          ( FusionReactor      *reactor,
                                              bool                sync );

#endif

