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

#ifndef __FUSION__REACTOR_H__
#define __FUSION__REACTOR_H__

#include <core/fusion/fusion_types.h>
#include <core/fusion/list.h>

typedef enum {
     RS_OK,
     RS_REMOVE,
     RS_DROP
} ReactionResult;

typedef ReactionResult (*React) (const void    *msg_data,
                                 void          *ctx);

typedef struct {
     FusionLink  link;
     React       react;
     void       *ctx;
     bool        attached;
} Reaction;

typedef struct {
     FusionLink  link;
     int         react_index;
     void       *ctx;
     bool        attached;
} GlobalReaction;

FusionReactor *fusion_reactor_new           (int             msg_size);
FusionResult   fusion_reactor_free          (FusionReactor  *reactor);
FusionResult   fusion_reactor_attach        (FusionReactor  *reactor,
                                             React           react,
                                             void           *ctx,
                                             Reaction       *reaction);
FusionResult   fusion_reactor_detach        (FusionReactor  *reactor,
                                             Reaction       *reaction);
FusionResult   fusion_reactor_attach_global (FusionReactor  *reactor,
                                             int             react_index,
                                             void           *ctx,
                                             GlobalReaction *reaction);
FusionResult   fusion_reactor_detach_global (FusionReactor  *reactor,
                                             GlobalReaction *reaction);
FusionResult   fusion_reactor_dispatch      (FusionReactor  *reactor,
                                             const void     *msg_data,
                                             bool            self,
                                             const React    *globals);



#endif /* __FUSION__REACTOR_H__ */

