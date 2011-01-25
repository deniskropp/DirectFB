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

     DFP_ALL   = 3
} DirectFilePermission;

typedef enum {
     DFIF_NONE = 0,

     DFIF_SIZE = 1,

     DFIF_ALL  = 1
} DirectFileInfoFlags;

typedef struct {
     DirectFileInfoFlags flags;

     size_t              size;
} DirectFileInfo;


DirectResult DIRECT_API  direct_file_open ( DirectFile *file, const char *name, int flags, mode_t mode );

DirectResult DIRECT_API  direct_file_read ( DirectFile *file, void *buffer, size_t bytes, size_t *ret_bytes );

DirectResult DIRECT_API  direct_file_write( DirectFile *file, const void *buffer, size_t bytes, size_t *ret_bytes );

DirectResult DIRECT_API  direct_file_seek( DirectFile *file, off_t offset );
DirectResult DIRECT_API  direct_file_seek_to( DirectFile *file, off_t offset );

DirectResult DIRECT_API  direct_file_close( DirectFile *file );

DirectResult DIRECT_API  direct_file_map( DirectFile *file, void *addr, size_t offset, size_t bytes, DirectFilePermission flags, void **ret_addr );

DirectResult DIRECT_API  direct_file_unmap( DirectFile *file, void *addr, size_t bytes );

DirectResult DIRECT_API  direct_file_get_info( DirectFile *file, DirectFileInfo *ret_info );

DirectResult DIRECT_API  direct_file_dup( DirectFile *file, const DirectFile *other );

/**********************************************************************************************************************/

DirectResult DIRECT_API  direct_fgets ( DirectFile *file, char *buf, size_t length );

/**********************************************************************************************************************/

DirectResult DIRECT_API  direct_popen ( DirectFile *file, const char *name, int flags );

DirectResult DIRECT_API  direct_pclose( DirectFile *file );

/**********************************************************************************************************************/

DirectResult DIRECT_API  direct_readlink( const char *name, char *buf, size_t length, size_t *ret_length );

DirectResult DIRECT_API  direct_access( const char *name, int flags );


#define	R_OK	4
#define	W_OK	2
#define	X_OK	1
#define	F_OK	0

#endif

