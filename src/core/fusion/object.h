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

typedef enum {
     FOS_INIT,
     FOS_ACTIVE,
     FOS_DEINIT
} FusionObjectState;

struct _FusionObject {
     FusionLink         link;
     FusionObjectPool  *pool;

     int                id;

     FusionObjectState  state;

     FusionRef          ref;
     FusionReactor     *reactor;
};


typedef bool (*FusionObjectCallback)( FusionObjectPool *pool,
                                      FusionObject     *object,
                                      void             *ctx );


FusionObjectPool *fusion_object_pool_create ( const char            *name,
                                              int                    object_size,
                                              int                    message_size,
                                              FusionObjectDestructor destructor );

FusionResult      fusion_object_pool_destroy( FusionObjectPool      *pool );


FusionResult      fusion_object_pool_enum   ( FusionObjectPool      *pool,
                                              FusionObjectCallback   callback,
                                              void                  *ctx );


FusionObject     *fusion_object_create  ( FusionObjectPool *pool );

FusionResult      fusion_object_activate( FusionObject     *object );

FusionResult      fusion_object_destroy ( FusionObject     *object );


#define FUSION_OBJECT_METHODS(type, prefix)                                    \
                                                                               \
static inline FusionResult                                                     \
prefix##_attach( type     *object,                                             \
                 React     react,                                              \
                 void     *ctx,                                                \
                 Reaction *reaction )                                          \
{                                                                              \
     return fusion_reactor_attach( ((FusionObject*)object)->reactor,           \
                                   react, ctx, reaction );                     \
}                                                                              \
                                                                               \
static inline FusionResult                                                     \
prefix##_detach( type     *object,                                             \
                 Reaction *reaction )                                          \
{                                                                              \
     return fusion_reactor_detach( ((FusionObject*)object)->reactor,           \
                                   reaction );                                 \
}                                                                              \
                                                                               \
static inline FusionResult                                                     \
prefix##_attach_global( type           *object,                                \
                        int             react_index,                           \
                        void           *ctx,                                   \
                        GlobalReaction *reaction )                             \
{                                                                              \
     return fusion_reactor_attach_global( ((FusionObject*)object)->reactor,    \
                                          react_index, ctx, reaction );        \
}                                                                              \
                                                                               \
static inline FusionResult                                                     \
prefix##_detach_global( type           *object,                                \
                        GlobalReaction *reaction )                             \
{                                                                              \
     return fusion_reactor_detach_global( ((FusionObject*)object)->reactor,    \
                                          reaction );                          \
}                                                                              \
                                                                               \
static inline FusionResult                                                     \
prefix##_dispatch( type        *object,                                        \
                   void        *message,                                       \
                   const React *globals )                                      \
{                                                                              \
     return fusion_reactor_dispatch( ((FusionObject*)object)->reactor,         \
                                     message, true, globals );                 \
}                                                                              \
                                                                               \
static inline FusionResult                                                     \
prefix##_ref( type *object )                                                   \
{                                                                              \
     return fusion_ref_up( &((FusionObject*)object)->ref, false );             \
}                                                                              \
                                                                               \
static inline FusionResult                                                     \
prefix##_unref( type *object )                                                 \
{                                                                              \
     return fusion_ref_down( &((FusionObject*)object)->ref, false );           \
}                                                                              \
                                                                               \
static inline FusionResult                                                     \
prefix##_link( type **link,                                                    \
               type  *object )                                                 \
{                                                                              \
     FusionResult ret;                                                         \
                                                                               \
     ret = fusion_ref_up( &((FusionObject*)object)->ref, true );               \
     if (ret)                                                                  \
          return ret;                                                          \
                                                                               \
     *link = object;                                                           \
                                                                               \
     return FUSION_SUCCESS;                                                    \
}                                                                              \
                                                                               \
static inline FusionResult                                                     \
prefix##_unlink( type **link )                                                 \
{                                                                              \
     type *object = *link;                                                     \
                                                                               \
     *link = NULL;                                                             \
                                                                               \
     return fusion_ref_down( &((FusionObject*)object)->ref, true );            \
}


#ifdef __cplusplus
}
#endif

#endif /* __FUSION__OBJECT_H__ */

