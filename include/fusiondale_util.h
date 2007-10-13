/*
   (c) Copyright 2006-2007  directfb.org

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>.

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

#ifndef __FUSIONDALE_UTIL_H__
#define __FUSIONDALE_UTIL_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <fusiondale.h>

#include <direct/debug.h>
#include <direct/memcpy.h>
#include <direct/messages.h>


#define COMA_GENCALL_PREPARE( coma, length )                                                   \
     do {                                                                                      \
          DirectResult ret;                                                                    \
                                                                                               \
          D_ASSERT( coma != NULL );                                                            \
          D_ASSERT( length > 0 );                                                              \
                                                                                               \
          ret = coma->GetLocal( coma, length, &ptr );                                          \
          if (ret) {                                                                           \
               D_DERROR( ret, "IComa::GetLocal( %zu ) failed!\n", length );                    \
               return ret;                                                                     \
          }                                                                                    \
     } while (0)

#define COMA_GENCALL_PREPARE_FROM( coma, from )                                                \
     do {                                                                                      \
          DirectResult ret;                                                                    \
                                                                                               \
          D_ASSERT( coma != NULL );                                                            \
          D_ASSERT( from != NULL );                                                            \
                                                                                               \
          ret = coma->GetLocal( coma, sizeof(*from), &ptr );                                   \
          if (ret) {                                                                           \
               D_DERROR( ret, "IComa::GetLocal( %zu ) failed!\n", sizeof(*from) );             \
               return ret;                                                                     \
          }                                                                                    \
                                                                                               \
          direct_memcpy( ptr, from, sizeof(*from) );                                           \
     } while (0)

#define COMA_GENCALL_EXECUTE( component, methodid )                                            \
     do {                                                                                      \
          DirectResult ret;                                                                    \
                                                                                               \
          D_ASSERT( component != NULL );                                                       \
                                                                                               \
          ret = component->Call( component, methodid, ptr, &result );                          \
          if (ret) {                                                                           \
               D_DERROR( ret, "IComaComponent::Call( %lu, %p ) failed!\n",                     \
                         (ComaMethodID) methodid, ptr );                                       \
               return ret;                                                                     \
          }                                                                                    \
     } while (0)

#define COMA_GENCALL_EXECUTE_TO( component, methodid, to )                                     \
     do {                                                                                      \
          DirectResult ret;                                                                    \
                                                                                               \
          D_ASSERT( component != NULL );                                                       \
          D_ASSERT( to != NULL );                                                              \
                                                                                               \
          ret = component->Call( component, methodid, ptr, &result );                          \
          if (ret) {                                                                           \
               D_DERROR( ret, "IComaComponent::Call( %lu, %p ) failed!\n",                     \
                         (ComaMethodID) methodid, ptr );                                       \
               return ret;                                                                     \
          }                                                                                    \
                                                                                               \
          direct_memcpy( to, ptr, sizeof(*to) );                                               \
     } while (0)

/**********************************************************************************************************************/

#define COMA_GENCALL_DEFINE___( NAME, METHOD, METHOD_ID )                                      \
     static inline DirectResult                                                                \
     NAME ## _GenCall_ ## METHOD( IComa                  *coma,                                \
                                  IComaComponent         *component )                          \
     {                                                                                         \
          void *ptr = NULL;                                                                    \
          int   result;                                                                        \
                                                                                               \
          D_ASSERT( coma != NULL );                                                            \
          D_ASSERT( component != NULL );                                                       \
                                                                                               \
          COMA_GENCALL_EXECUTE( component, METHOD_ID );                                        \
                                                                                               \
          return result;                                                                       \
     }

#define COMA_GENCALL_DEFINE_I_( NAME, METHOD, METHOD_ID )                                      \
     static inline DirectResult                                                                \
     NAME ## _GenCall_ ## METHOD( IComa                  *coma,                                \
                                  IComaComponent         *component,                           \
                                  NAME ## Call ## METHOD *data )                               \
     {                                                                                         \
          void *ptr;                                                                           \
          int   result;                                                                        \
                                                                                               \
          D_ASSERT( coma != NULL );                                                            \
          D_ASSERT( component != NULL );                                                       \
          D_ASSERT( data != NULL );                                                            \
                                                                                               \
          COMA_GENCALL_PREPARE_FROM( coma, data );                                             \
                                                                                               \
          COMA_GENCALL_EXECUTE( component, METHOD_ID );                                        \
                                                                                               \
          return result;                                                                       \
     }

#define COMA_GENCALL_DEFINE__O( NAME, METHOD, METHOD_ID )                                      \
     static inline DirectResult                                                                \
     NAME ## _GenCall_ ## METHOD( IComa                  *coma,                                \
                                  IComaComponent         *component,                           \
                                  NAME ## Call ## METHOD *data )                               \
     {                                                                                         \
          void *ptr;                                                                           \
          int   result;                                                                        \
                                                                                               \
          D_ASSERT( coma != NULL );                                                            \
          D_ASSERT( component != NULL );                                                       \
          D_ASSERT( data != NULL );                                                            \
                                                                                               \
          COMA_GENCALL_PREPARE( coma, sizeof(MediaPlayerCallInfo) );                           \
                                                                                               \
          COMA_GENCALL_EXECUTE_TO( component, METHOD_ID, data );                               \
                                                                                               \
          return result;                                                                       \
     }

#define COMA_GENCALL_DEFINE_IO( NAME, METHOD, METHOD_ID )                                      \
     static inline DirectResult                                                                \
     NAME ## _GenCall_ ## METHOD( IComa                  *coma,                                \
                                  IComaComponent         *component,                           \
                                  NAME ## Call ## METHOD *data )                               \
     {                                                                                         \
          void *ptr;                                                                           \
          int   result;                                                                        \
                                                                                               \
          D_ASSERT( coma != NULL );                                                            \
          D_ASSERT( component != NULL );                                                       \
          D_ASSERT( data != NULL );                                                            \
                                                                                               \
          COMA_GENCALL_PREPARE_FROM( coma, data );                                             \
                                                                                               \
          COMA_GENCALL_EXECUTE_TO( component, METHOD_ID, data );                               \
                                                                                               \
          return result;                                                                       \
     }


#ifdef __cplusplus
}
#endif

#endif

