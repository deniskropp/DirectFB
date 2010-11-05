/*
   (c) Copyright 2001-2008  The world wide DirectFB Open Source Community (directfb.org)
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

#ifndef __DIRECT__OS__FILESYSTEM_H__
#define __DIRECT__OS__FILESYSTEM_H__

#include <direct/os/types.h>

/**********************************************************************************************************************/


/* API is subject to change! */


typedef enum {
     DFP_NONE  = 0,

     DFP_READ  = 1,
     DFP_WRTIE = 2,

     DFP_ALL   = 3,
} DirectFilePermission;


__attribute__((no_instrument_function))
DirectResult  direct_open ( DirectFile *file, const char *name, int flags, mode_t mode );

__attribute__((no_instrument_function))
DirectResult  direct_read ( DirectFile *file, void *buffer, size_t bytes, size_t *ret_bytes );

__attribute__((no_instrument_function))
DirectResult  direct_write( DirectFile *file, const void *buffer, size_t bytes, size_t *ret_bytes );

__attribute__((no_instrument_function))
DirectResult  direct_close( DirectFile *file );

__attribute__((no_instrument_function))
DirectResult  direct_mmap( DirectFile *file, void *addr, size_t offset, size_t bytes, DirectFilePermission flags );

__attribute__((no_instrument_function))
DirectResult  direct_unmap( DirectFile *file, void *addr, size_t bytes );

/**********************************************************************************************************************/

__attribute__((no_instrument_function))
DirectResult  direct_fgets ( DirectFile *file, char *buf, size_t length );

/**********************************************************************************************************************/

__attribute__((no_instrument_function))
DirectResult  direct_popen ( DirectFile *file, const char *name, int flags );

__attribute__((no_instrument_function))
DirectResult  direct_pclose( DirectFile *file );

/**********************************************************************************************************************/

__attribute__((no_instrument_function))
DirectResult  direct_readlink( const char *name, char *buf, size_t length, size_t *ret_length );

__attribute__((no_instrument_function))
DirectResult  direct_access( const char *name, int flags );


#define	R_OK	4
#define	W_OK	2
#define	X_OK	1
#define	F_OK	0

#endif

