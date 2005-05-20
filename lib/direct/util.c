/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <direct/messages.h>
#include <direct/util.h>

/*
 * translates errno to DirectResult
 */
DirectResult
errno2result( int erno )
{
     switch (erno) {
          case 0:
               return DFB_OK;
          case ENOENT:
               return DFB_FILENOTFOUND;
          case EACCES:
          case EPERM:
               return DFB_ACCESSDENIED;
          case EBUSY:
          case EAGAIN:
               return DFB_BUSY;
          case ECONNREFUSED:
               return DFB_ACCESSDENIED;
          case ENODEV:
          case ENXIO:
#ifdef ENOTSUP
          /* ENOTSUP is not defined on NetBSD */
          case ENOTSUP:
#endif
               return DFB_UNSUPPORTED;
     }

     return DFB_FAILURE;
}

const char *
DirectResultString( DirectResult result )
{
     switch (result) {
          case DFB_OK:
               return "OK";
          case DFB_FAILURE:
               return "General failure!";
          case DFB_INIT:
               return "Initialization error!";
          case DFB_BUG:
               return "Internal bug!";
          case DFB_DEAD:
               return "Interface was released!";
          case DFB_UNSUPPORTED:
               return "Not supported!";
          case DFB_UNIMPLEMENTED:
               return "Not implemented!";
          case DFB_ACCESSDENIED:
               return "Access denied!";
          case DFB_INVARG:
               return "Invalid argument!";
          case DFB_NOSYSTEMMEMORY:
               return "Out of memory!";
          case DFB_NOVIDEOMEMORY:
               return "Out of video memory!";
          case DFB_LOCKED:
               return "Resource is locked!";
          case DFB_BUFFEREMPTY:
               return "Buffer is empty!";
          case DFB_FILENOTFOUND:
               return "File not found!";
          case DFB_IO:
               return "General I/O error!";
          case DFB_NOIMPL:
               return "No (suitable) implementation found!";
          case DFB_MISSINGFONT:
               return "No font has been set!";
          case DFB_TIMEOUT:
               return "Operation timed out!";
          case DFB_MISSINGIMAGE:
               return "No image has been set!";
          case DFB_BUSY:
               return "Resource is busy!";
          case DFB_THIZNULL:
               return "'thiz' argument is NULL!";
          case DFB_IDNOTFOUND:
               return "Requested ID not found!";
          case DFB_INVAREA:
               return "Invalid area present!";
          case DFB_DESTROYED:
               return "Resource was destroyed!";
          case DFB_FUSION:
               return "Fusion IPC error detected!";
          case DFB_BUFFERTOOLARGE:
               return "Buffer is too large!";
          case DFB_INTERRUPTED:
               return "Operation has been interrupted!";
          case DFB_NOCONTEXT:
               return "No context available!";
          case DFB_TEMPUNAVAIL:
               return "Resource temporarily unavailable!";
          case DFB_LIMITEXCEEDED:
               return "Limit has been exceeded!";
          case DFB_NOSUCHMETHOD:
               return "No such (remote) method!";
          case DFB_NOSUCHINSTANCE:
               return "No such (remote) instance!";
          case DFB_ITEMNOTFOUND:
               return "Appropriate item not found!";
          case DFB_VERSIONMISMATCH:
               return "Some versions didn't match!";
          case DFB_NOSHAREDMEMORY:
               return "Out of shared memory!";
          case DFB_EOF:
               return "End of file!";
     }

     return "UNKNOWN RESULT CODE!";
}

int
direct_try_open( const char *name1, const char *name2, int flags )
{
     int fd;

     fd = open (name1, flags);
     if (fd >= 0)
          return fd;

     if (errno != ENOENT) {
          D_PERROR( "Direct/Util: opening '%s' failed\n", name1 );
          return -1;
     }

     fd = open (name2, flags);
     if (fd >= 0)
          return fd;

     if (errno == ENOENT)
          D_PERROR( "Direct/Util: opening '%s' and '%s' failed\n", name1, name2 );
     else
          D_PERROR( "Direct/Util: opening '%s' failed\n", name2 );

     return -1;
}

void
direct_trim( char **s )
{
     int i;
     int len = strlen( *s );

     for (i = len-1; i >= 0; i--)
          if ((*s)[i] <= ' ')
               (*s)[i] = 0;
          else
               break;

     while (**s)
          if (**s <= ' ')
               (*s)++;
          else
               return;
}

