/*
   (c) Copyright 2008  Denis Oliver Kropp

   All rights reserved.

   This file is subject to the terms and conditions of the MIT License:

   Permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation
   files (the "Software"), to deal in the Software without restriction,
   including without limitation the rights to use, copy, modify, merge,
   publish, distribute, sublicense, and/or sell copies of the Software,
   and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <config.h>

#include <string.h>
#include <unistd.h>

#include <direct/debug.h>
#include <direct/direct.h>
#include <direct/messages.h>
#include <direct/stream.h>
#include <direct/util.h>


D_DEBUG_DOMAIN( Direct_Cat, "Direct/Cat", "libdirect cat" );


static int
show_usage( const char *prg )
{
     fprintf( stderr, "Usage: %s <url>\n", prg );

     return -1;
}

int
main( int argc, char *argv[] )
{
     DirectResult   ret;
     int            i, fdo;
     DirectStream  *stream;
     const char    *url = NULL;

     /* Parse arguments. */
     for (i=1; i<argc; i++) {
          if (!strcmp( argv[i], "-h" ))
               return show_usage( argv[0] );
          else if (!url)
               url = argv[i];
          else
               return show_usage( argv[0] );
     }

     /* Check if we got an URL. */
     if (!url)
          return show_usage( argv[0] );
          
     /* Initialize libdirect. */
     direct_initialize();

     D_INFO( "Direct/Cat: Start from '%s'...\n", url );

     /* Open input. */
     ret = direct_stream_create( url, &stream );
     if (ret) {
          D_DERROR( ret, "Direct/Cat: Opening '%s' failed!\n", url );
          goto out;
     }

     /* Open output. */
     fdo = dup( fileno(stdout) );
     if (fdo < 0) {
          ret = errno2result( errno );
          D_PERROR( "Direct/Cat: Duplicating stdout (%d) failed!\n", fileno(stdout) );
          goto close_in;
     }

     /* Copy loop. */
     while (true) {
          char         buf[16384];
          unsigned int length;

          /* Wait for full buffer, if supported, otherwise waits for any data. */
          ret = direct_stream_wait( stream, sizeof(buf), NULL );
          if (ret) {
               D_DERROR( ret, "Direct/Cat: Waiting for data from '%s' failed!\n", url );
               goto close_both;
          }

          /* Read buffer. */
          ret = direct_stream_read( stream, sizeof(buf), buf, &length );
          if (ret) {
               D_DERROR( ret, "Direct/Cat: Reading from '%s' failed!\n", url );
               goto close_both;
          }

          D_DEBUG_AT( Direct_Cat, "Read  %5u bytes\n", length );

          /* Write buffer. */
          length = write( fdo, buf, length );
          if (length < 0) {
               ret = errno2result( errno );
               D_PERROR( "Direct/Cat: Writing to stdout (%d) failed!\n", fileno(stdout) );
               goto close_both;
          }

          D_DEBUG_AT( Direct_Cat, "Wrote %5u bytes\n", length );
     }

close_both:
     /* Close output. */
     close( fdo );

close_in:
     /* Close input. */
     direct_stream_destroy( stream );

out:
     /* Shutdown libdirect. */
     direct_shutdown();

     return ret;
}

