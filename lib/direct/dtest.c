/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

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

#include <direct/debug.h>
#include <direct/direct.h>
#include <direct/log.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/trace.h>

static int
show_usage()
{
     fprintf( stderr, "Usage: dtest [-f <file>] [-u <host>:<port>]\n" );

     return -1;
}

int
main( int argc, char *argv[] )
{
     int            i;
     DirectResult   ret;
     DirectLogType  log_type  = DLT_STDERR;
     const char    *log_param = NULL;
     DirectLog     *log;


     for (i=1; i<argc; i++) {
          if (!strcmp( argv[i], "-f" )) {
               if (++i < argc) {
                    log_type  = DLT_FILE;
                    log_param = argv[i];
               }
               else
                    return show_usage();
          }
          else if (!strcmp( argv[i], "-u" )) {
               if (++i < argc) {
                    log_type  = DLT_UDP;
                    log_param = argv[i];
               }
               else
                    return show_usage();
          }
          else
               return show_usage();
     }

     /* Initialize logging. */
     ret = direct_log_create( log_type, log_param, &log );
     if (ret)
          return -1;

     /* Set default log to use. */
     direct_log_set_default( log );


     /* Test memory leak detector by not freeing this one. */
     D_MALLOC( 1351 );

     D_INFO( "Direct/Test: Application starting...\n" );


     /* Initialize libdirect. */
     direct_initialize();

     direct_find_best_memcpy();


     /* Shutdown libdirect. */
     direct_shutdown();


     /* Shutdown logging. */
     direct_log_destroy( log );

     direct_config->debug = true;
     direct_print_memleaks();

     return 0;
}

