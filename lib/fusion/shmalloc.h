/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

   Fusion shmalloc is based on GNU malloc. Please see below.

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

/* Declarations for `malloc' and friends.
   Copyright 1990, 1991, 1992 Free Software Foundation, Inc.
                  Written May 1989 by Mike Haertel.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.

   The author may be reached (Email) at the address mike@ai.mit.edu,
   or (US mail) as Mike Haertel c/o Free Software Foundation.  */

#ifndef __FUSION__SHMALLOC_H__
#define __FUSION__SHMALLOC_H__

#include <stddef.h>

#include <direct/build.h>
#include <fusion/build.h>

#include <fusion/types.h>
#include <fusion/shm/pool.h>


#if FUSION_BUILD_MULTI && DIRECT_BUILD_TEXT
#define D_OOSHM()        (direct_messages_warn( __FUNCTION__, __FILE__, __LINE__,              \
                                                "out of shared memory" ), DFB_NOSHAREDMEMORY)
#else
#define D_OOSHM()        D_OOM()
#endif



void  fusion_dbg_print_memleaks( FusionSHMPoolShared *pool );


void *fusion_dbg_shmalloc ( FusionSHMPoolShared *pool,
                            const char *file, int line,
                            const char *func, size_t __size );

void *fusion_dbg_shcalloc ( FusionSHMPoolShared *pool,
                            const char *file, int line,
                            const char *func, size_t __nmemb, size_t __size);

void *fusion_dbg_shrealloc( FusionSHMPoolShared *pool,
                            const char *file, int line,
                            const char *func, const char *what, void *__ptr,
                            size_t __size );

void  fusion_dbg_shfree   ( FusionSHMPoolShared *pool,
                            const char *file, int line,
                            const char *func, const char *what, void *__ptr );

char *fusion_dbg_shstrdup ( FusionSHMPoolShared *pool,
                            const char *file, int line,
                            const char *func, const char *string );


/* Allocate SIZE bytes of memory.  */
void *fusion_shmalloc (FusionSHMPoolShared *pool, size_t __size);

/* Allocate NMEMB elements of SIZE bytes each, all initialized to 0.  */
void *fusion_shcalloc (FusionSHMPoolShared *pool, size_t __nmemb, size_t __size);

/* Re-allocate the previously allocated block
   in __ptr, making the new block SIZE bytes long.  */
void *fusion_shrealloc (FusionSHMPoolShared *pool, void *__ptr, size_t __size);

/* Free a block allocated by `shmalloc', `shrealloc' or `shcalloc'.  */
void  fusion_shfree (FusionSHMPoolShared *pool, void *__ptr);

/* Duplicate string in shared memory. */
char *fusion_shstrdup (FusionSHMPoolShared *pool, const char *string);



#if DIRECT_BUILD_DEBUGS || DIRECT_BUILD_DEBUG || defined(DIRECT_ENABLE_DEBUG) || defined(DIRECT_FORCE_DEBUG)

#if !DIRECT_BUILD_DEBUGS
#warning Building with debug, but library headers suggest that debug is not supported.
#endif


#define SHMALLOC(pool,bytes)        fusion_dbg_shmalloc( pool, __FILE__, __LINE__, __FUNCTION__, bytes )
#define SHCALLOC(pool,count,bytes)  fusion_dbg_shcalloc( pool, __FILE__, __LINE__, __FUNCTION__, count, bytes )
#define SHREALLOC(pool,mem,bytes)   fusion_dbg_shrealloc( pool, __FILE__, __LINE__, __FUNCTION__, #mem, mem, bytes )
#define SHFREE(pool,mem)            fusion_dbg_shfree( pool, __FILE__, __LINE__, __FUNCTION__, #mem,mem )
#define SHSTRDUP(pool,string)       fusion_dbg_shstrdup( pool, __FILE__, __LINE__, __FUNCTION__, string )


#else


#define SHMALLOC   fusion_shmalloc
#define SHCALLOC   fusion_shcalloc
#define SHREALLOC  fusion_shrealloc
#define SHFREE     fusion_shfree
#define SHSTRDUP   fusion_shstrdup


#endif



#endif

