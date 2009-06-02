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

#ifndef __DIRECT__STREAM_H__
#define __DIRECT__STREAM_H__

#include <stdio.h>
#include <sys/time.h>

#include <direct/types.h>

/*
 * Create a stream wrapper.
 * 
 * 'filename' can be a plain file name or one of the following:
 *   http://<host>[:<port>]/<path>
 *   unsv://<host>[:<port>]/<path>
 *   ftp://<host>[:<port>]/<path>
 *   rtsp://<host>[:<port>]/<path>
 *   tcp://<host>:<port>
 *   udp://<host>:<port>
 *   file:/<path>
 *   fd:/<fileno>
 *   stdin:/
 */
DirectResult  direct_stream_create  ( const char     *filename,
                                      DirectStream  **ret_stream );
                                      
/*
 * Duplicate the stream (never fails).
 */
DirectStream *direct_stream_dup     ( DirectStream   *stream );     

/*
 * Return the file descriptor associated to the stream.
 */
int           direct_stream_fileno  ( DirectStream   *stream );

/*
 * True if stream is seekable.
 */
bool          direct_stream_seekable( DirectStream   *stream );

/*
 * True if stream originates from a remote host.
 */
bool          direct_stream_remote  ( DirectStream   *stream );

/*
 * Get the mime description of the stream.
 * Returns NULL if the information is not available.
 */
const char*   direct_stream_mime    ( DirectStream   *stream );

/*
 * Get stream length.
 */
unsigned int  direct_stream_length  ( DirectStream   *stream );

/*
 * Get stream position.
 */
unsigned int  direct_stream_offset  ( DirectStream   *stream );

/*
 * Wait for data to be available.
 * If 'timeout' is NULL, the function blocks indefinitely.
 * Set the 'timeout' to 0 to make the function return immediatly.
 */
DirectResult  direct_stream_wait    ( DirectStream   *stream,
                                      unsigned int    length,
                                      struct timeval *timeout );

/*
 * Peek 'length' bytes of data at offset 'offset' from the stream.
 */
DirectResult  direct_stream_peek    ( DirectStream   *stream,
                                      unsigned int    length,
                                      int             offset,
                                      void           *buf,
                                      unsigned int   *read_out );

/*
 * Fetch 'length' bytes of data from the stream.
 */
DirectResult  direct_stream_read    ( DirectStream   *stream,
                                      unsigned int    length,
                                      void           *buf,
                                      unsigned int   *read_out );

/*
 * Seek to the specified absolute offset within the stream.
 */
DirectResult  direct_stream_seek    ( DirectStream   *stream,
                                      unsigned int    offset );

/*
 * Destroy the stream wrapper.
 */
void          direct_stream_destroy ( DirectStream   *stream );


#endif /* __DIRECT__STREAM_H__ */

