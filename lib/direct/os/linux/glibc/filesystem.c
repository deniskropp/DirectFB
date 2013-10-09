/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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
direct_file_open( DirectFile *file, const char *name, int flags, mode_t mode )
{
     D_ASSERT( file != NULL );
     D_ASSERT( name != NULL );

     file->file = NULL;

     file->fd = open( name, flags, mode );
     if (file->fd < 0)
          return errno2result( errno );

     return DR_OK;
}

DirectResult
direct_file_read( DirectFile *file, void *buffer, size_t bytes, size_t *ret_bytes )
{
     ssize_t num;

     D_ASSERT( file != NULL );
     D_ASSERT( buffer != NULL );

     num = read( file->fd, buffer, bytes );
     if (num < 0)
          return errno2result( errno );

     if (ret_bytes)
          *ret_bytes = num;

     return DR_OK;
}

DirectResult
direct_file_write( DirectFile *file, const void *buffer, size_t bytes, size_t *ret_bytes )
{
     ssize_t num;

     D_ASSERT( file != NULL );
     D_ASSERT( buffer != NULL );

     num = write( file->fd, buffer, bytes );
     if (num < 0)
          return errno2result( errno );

     if (ret_bytes)
          *ret_bytes = num;

     return DR_OK;
}

DirectResult
direct_file_seek( DirectFile *file, off_t offset )
{
     D_ASSERT( file != NULL );

     if (lseek( file->fd, offset, SEEK_CUR ) < 0)
          return errno2result( errno );

     return DR_OK;
}

DirectResult
direct_file_seek_to( DirectFile *file, off_t offset )
{
     D_ASSERT( file != NULL );

     if (lseek( file->fd, offset, SEEK_SET ) < 0)
          return errno2result( errno );

     return DR_OK;
}

DirectResult
direct_file_close( DirectFile *file )
{
     int ret;

     D_ASSERT( file != NULL );

     if (file->file) {
          ret = fclose( file->file );

          file->file = NULL;
     } else
          ret = close( file->fd );

     file->fd = -1;

     if (ret < 0)
          return errno2result( errno );

     return DR_OK;
}

DirectResult
direct_file_map( DirectFile *file, void *addr, size_t offset, size_t bytes, DirectFilePermission flags, void **ret_addr )
{
     void *map;
     int   prot = 0;

     D_ASSERT( file != NULL );
     D_ASSERT( ret_addr != NULL );

     if (flags & DFP_READ)
          prot |= PROT_READ;

     if (flags & DFP_WRTIE)
          prot |= PROT_WRITE;

     map = mmap( addr, bytes, prot, MAP_SHARED, file->fd, offset );
     if (map == MAP_FAILED)
          return errno2result( errno );

     *ret_addr = map;

     return DR_OK;
}

DirectResult
direct_file_unmap( DirectFile *file, void *addr, size_t bytes )
{
     if (munmap( addr, bytes ))
          return errno2result( errno );

     return DR_OK;
}

DirectResult
direct_file_get_info( DirectFile *file, DirectFileInfo *ret_info )
{
     struct stat st;

     D_ASSERT( file != NULL );
     D_ASSERT( ret_info != NULL );

     if (fstat( file->fd, &st ))
          return errno2result( errno );

     ret_info->flags = DFIF_SIZE;
     ret_info->size  = st.st_size;

     return DR_OK;
}

DirectResult
direct_file_dup( DirectFile *file, const DirectFile *other )
{
     int fd;

     D_ASSERT( file != NULL );
     D_ASSERT( other != NULL );

     file->file = NULL;

     fd = dup( other->fd );
     if (fd < 0)
          return errno2result( errno );

     file->fd = fd;

     return DR_OK;
}

/**********************************************************************************************************************/

DirectResult
direct_fgets( DirectFile *file, char *buf, size_t length )
{
     D_ASSERT( file != NULL );
     D_ASSERT( buf != NULL );

     if (!file->file) {
          file->file = fdopen( file->fd, "r" );
          if (!file->file)
               return errno2result( errno );
     }

     if (!fgets( buf, length, file->file )) {
          if (feof( file->file ))
               return DR_EOF;

          return DR_FAILURE;
     }

     return DR_OK;
}

/**********************************************************************************************************************/

DirectResult
direct_popen( DirectFile *file, const char *name, int flags )
{
     D_ASSERT( file != NULL );
     D_ASSERT( name != NULL );

     file->file = popen( name, (flags & O_WRONLY) ? "w" : (flags & O_RDWR) ? "rw" : "r" );
     if (!file->file)
          return errno2result( errno );

     file->fd = fileno( file->file );

     return DR_OK;
}

DirectResult
direct_pclose( DirectFile *file )
{
     int ret;

     D_ASSERT( file != NULL );
     D_ASSERT( file->file != NULL );

     ret = pclose( file->file );

     if (ret < 0)
          return errno2result( errno );

     return DR_OK;
}

/**********************************************************************************************************************/

DirectResult
direct_readlink( const char *name, char *buf, size_t length, size_t *ret_length )
{
     int ret;

     D_ASSERT( name != NULL );

     ret = readlink( name, buf, length );
     if (ret < 0)
          return errno2result( errno );

     if (ret_length)
          *ret_length = ret;

     return DR_OK;
}

/**********************************************************************************************************************/

DirectResult
direct_access( const char *name, int flags )
{
     int ret;

     D_ASSERT( name != NULL );

     ret = access( name, flags );
     if (ret < 0)
          return errno2result( errno );

     return DR_OK;
}

