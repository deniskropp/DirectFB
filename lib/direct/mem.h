/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

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

#ifndef __DIRECT__MEM_H__
#define __DIRECT__MEM_H__

#include <stddef.h>

#include <direct/build.h>

void direct_print_memleaks( void );


void  direct_free   ( const char *file, int line,
                      const char *func, const char *what, void *mem );

void *direct_malloc ( const char *file, int line,
                      const char *func, size_t bytes );

void *direct_calloc ( const char *file, int line,
                      const char *func, size_t count, size_t bytes);

void *direct_realloc( const char *file, int line,
                      const char *func, const char *what, void *mem,
                      size_t bytes );

char *direct_strdup ( const char *file, int line,
                      const char *func, const char *string );


#if DIRECT_BUILD_DEBUG || defined(DIRECT_ENABLE_DEBUG) || defined(DIRECT_FORCE_DEBUG)

#if !DIRECT_BUILD_DEBUGS
#warning Building with debug, but library headers suggest that debug is not supported.
#endif


#define D_FREE(mem)           direct_free( __FILE__, __LINE__, __FUNCTION__, #mem, mem )
#define D_MALLOC(bytes)       direct_malloc( __FILE__, __LINE__, __FUNCTION__, bytes )
#define D_CALLOC(count,bytes) direct_calloc( __FILE__, __LINE__, __FUNCTION__, count, bytes )
#define D_REALLOC(mem,bytes)  direct_realloc( __FILE__, __LINE__, __FUNCTION__, #mem, mem, bytes )
#define D_STRDUP(string)      direct_strdup( __FILE__, __LINE__, __FUNCTION__, string )

#else

#include <stdlib.h>
#include <string.h>

#define D_FREE     free
#define D_MALLOC   malloc
#define D_CALLOC   calloc
#define D_REALLOC  realloc
#define D_STRDUP   strdup

#endif


#endif

