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

#include "fusion_types.h"


typedef enum {
     RS_OK,
     RS_REMOVE,
     RS_DROP
} ReactionResult;

typedef ReactionResult (*React) (const void    *msg_data,
                                 void          *ctx);

FusionReactor *reactor_new      (const int      msg_size);
FusionResult   reactor_free     (FusionReactor *reactor);
FusionResult   reactor_attach   (FusionReactor *reactor,
                                 React          react,
                                 void          *ctx);
FusionResult   reactor_detach   (FusionReactor *reactor,
                                 React          react,
                                 void          *ctx);
FusionResult   reactor_dispatch (FusionReactor *reactor,
                                 const void    *msg_data,
                                 bool           self);



#endif /* __FUSION__REACTOR_H__ */

