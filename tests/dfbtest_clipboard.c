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

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <direct/messages.h>

#include <directfb.h>
#include <directfb_strings.h>

/**********************************************************************************************************************/

static int
print_usage( const char *prg )
{
     fprintf (stderr, "\n");
     fprintf (stderr, "== DirectFB Clipboard Test (version %s) ==\n", DIRECTFB_VERSION);
     fprintf (stderr, "\n");

     fprintf (stderr, "\n");
     fprintf (stderr, "Usage: %s [options]\n", prg);
     fprintf (stderr, "\n");
     fprintf (stderr, "Options:\n");
     fprintf (stderr, "  -h, --help                        Show this help message\n");
     fprintf (stderr, "  -v, --version                     Print version information\n");

     return -1;
}

/**********************************************************************************************************************/

static DFBResult
Test_Clipboard_Set( IDirectFB  *dfb,
                    const char *mime_type,
                    const char *data )
{
     DFBResult      ret;
     struct timeval tv;

     gettimeofday( &tv, NULL );

     direct_log_printf( NULL, "Setting clipboard data to mime type '%s' with data '%s' and timestamp %ld.%06ld\n",
                        mime_type, data, tv.tv_sec, tv.tv_usec );

     ret = dfb->SetClipboardData( dfb, mime_type, data, strlen(data) + 1, &tv );
     if (ret) {
          D_DERROR( ret, "DFBTest/Clipboard: IDirectFB::SetClipboardData() failed!\n" );
          return ret;
     }

     return ret;
}

static DFBResult
Test_Clipboard_Get( IDirectFB *dfb )
{
     DFBResult       ret;
     struct timeval  tv;
     char           *mime_type;
     void           *data;
     unsigned int    data_size;

     ret = dfb->GetClipboardTimeStamp( dfb, &tv );
     if (ret) {
          D_DERROR( ret, "DFBTest/Clipboard: IDirectFB::GetClipboardTimeStamp() failed!\n" );
          return ret;
     }

     ret = dfb->GetClipboardData( dfb, &mime_type, &data, &data_size );
     if (ret) {
          D_DERROR( ret, "DFBTest/Clipboard: IDirectFB::GetClipboardData() failed!\n" );
          return ret;
     }

     direct_log_printf( NULL, "Got clipboard data of mime type '%s' with data '%s'\n", mime_type, (const char*) data );
     direct_log_printf( NULL, "Got clipboard timestamp %ld.%06ld\n", tv.tv_sec, tv.tv_usec );

     return ret;
}

int
main( int argc, char *argv[] )
{
     DFBResult  ret;
     int        i;
     IDirectFB *dfb;

     /* Initialize DirectFB. */
     ret = DirectFBInit( &argc, &argv );
     if (ret) {
          D_DERROR( ret, "DFBTest/Blit: DirectFBInit() failed!\n" );
          return ret;
     }

     /* Parse arguments. */
     for (i=1; i<argc; i++) {
          const char *arg = argv[i];

          if (strcmp( arg, "-h" ) == 0 || strcmp (arg, "--help") == 0)
               return print_usage( argv[0] );
          else if (strcmp (arg, "-v") == 0 || strcmp (arg, "--version") == 0) {
               fprintf (stderr, "dfbtest_blit version %s\n", DIRECTFB_VERSION);
               return false;
          }
          else
               return print_usage( argv[0] );
     }

     /* Create super interface. */
     ret = DirectFBCreate( &dfb );
     if (ret) {
          D_DERROR( ret, "DFBTest/Blit: DirectFBCreate() failed!\n" );
          return ret;
     }

     Test_Clipboard_Set( dfb, "text/plain", "This is a test string for clipboard" );
     Test_Clipboard_Get( dfb );

     /* Shutdown DirectFB. */
     dfb->Release( dfb );

     return ret;
}

