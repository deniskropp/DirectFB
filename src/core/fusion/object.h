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

#ifndef __FUSION__OBJECT_H__
#define __FUSION__OBJECT_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "fusion_types.h"

#include <core/fusion/lock.h>
#include <core/fusion/list.h>
#include <core/fusion/ref.h>
#include <core/fusion/reactor.h>

typedef void (*FusionObjectDestructor)( FusionObject *object, bool zombie );

struct _FusionObject {
     FusionLink        link;
     FusionObjectPool *pool;

     FusionRef         ref;
     FusionReactor    *reactor;
};


FusionObjectPool *fusion_object_pool_create( const char            *name,
                                             int                    object_size,
                                             int                    message_size,
                                             FusionObjectDestructor destructor );

FusionResult      fusion_object_pool_destroy( FusionObjectPool     *pool );


FusionObject     *fusion_object_create  ( FusionObjectPool *pool );

FusionResult      fusion_object_attach  ( FusionObject     *object,
                                          React             react,
                                          void             *ctx );
FusionResult      fusion_object_detach  ( FusionObject     *object,
                                          React             react,
                                          void             *ctx );
FusionResult      fusion_object_dispatch( FusionObject     *object,
                                          void             *message );

FusionResult      fusion_object_ref     ( FusionObject     *object );
FusionResult      fusion_object_unref   ( FusionObject     *object );

FusionResult      fusion_object_link    ( FusionObject    **link,
                                          FusionObject     *object );
FusionResult      fusion_object_unlink  ( FusionObject     *object );

FusionResult      fusion_object_destroy ( FusionObject     *object );


#ifdef __cplusplus
}
#endif

#endif /* __FUSION__OBJECT_H__ */

