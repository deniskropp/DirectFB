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

#include <config.h>

#include <direct/debug.h>
#include <direct/filesystem.h>
#include <direct/util.h>

/**********************************************************************************************************************/

DirectResult
direct_open( DirectFile *file, const char *name, int flags, mode_t mode )
{
     D_ASSERT( file != NULL );

     file->file = filp_open( name, flags, mode );
     if (!file->file)
          return DR_FAILURE;

     file->count = (atomic_t) ATOMIC_INIT(1);

     D_MAGIC_SET( file, DirectFile );

     return DR_OK;
}

DirectResult
direct_read( DirectFile *file, void *buffer, size_t bytes, size_t *ret_bytes )
{
     ssize_t num;

     D_MAGIC_ASSERT( file, DirectFile );

     num = vfs_read( file->file, buffer, bytes, NULL );
     if (num < 0)
          return errno2result( -num );

     if (ret_bytes)
          *ret_bytes = num;

     return DR_OK;
}

DirectResult
direct_write( DirectFile *file, const void *buffer, size_t bytes, size_t *ret_bytes )
{
     ssize_t num;

     D_MAGIC_ASSERT( file, DirectFile );

     num = vfs_write( file->file, buffer, bytes, NULL );
     if (num < 0)
          return errno2result( -num );

     if (ret_bytes)
          *ret_bytes = num;

     return DR_OK;
}

DirectResult
direct_close( DirectFile *file )
{
     int ret;

     D_MAGIC_ASSERT( file, DirectFile );

     if (!atomic_dec_return( &file->count )) {
          ret = filp_close( file->file, NULL );
          if (ret < 0)
               return errno2result( -ret );

          D_MAGIC_CLEAR( file );
     }

     return DR_OK;
}

DirectResult
direct_access( const char *name, int flags )
{
     int ret;

     D_ASSERT( name != NULL );

     ret = sys_access( name, flags );
     if (ret < 0)
          return errno2result( -ret );

     return DR_OK;
}

