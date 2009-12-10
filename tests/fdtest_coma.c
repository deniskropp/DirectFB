/*
   (c) Copyright 2006-2007  directfb.org

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>.

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
#include <stdio.h>
#include <unistd.h>

#include <fusiondale.h>

static char *enter_coma = "FDTestComa";

/**********************************************************************************************************************/

static bool parse_command_line( int argc, char *argv[] );

/**********************************************************************************************************************/

int
main( int argc, char *argv[] )
{
     DirectResult  ret;
     IFusionDale  *dale = NULL;
     IComa        *coma = NULL;
     void         *mem;

     /* Initialize FusionDale including command line parsing. */
     ret = FusionDaleInit( &argc, &argv );
     if (ret) {
          D_DERROR( ret, "FusionDale/Master: FusionDaleInit() failed!\n" );
          goto out;
     }

     /* Parse the command line. */
     if (!parse_command_line( argc, argv ))
          goto out;

     /* Create the super interface. */
     ret = FusionDaleCreate( &dale );
     if (ret) {
          D_DERROR( ret, "FusionDale/Master: FusionDaleCreate() failed!\n" );
          goto out;
     }

     ret = dale->EnterComa( dale, enter_coma, &coma );
     if (ret) {
          D_DERROR( ret, "FusionDale/Master: IFusionDale::EnterComa( '%s' ) failed!\n", enter_coma );
          goto out;
     }

     while (1) {
          coma->GetLocal( coma, 666, &mem );

          coma->FreeLocal( coma );
     }

     pause();


out:
     /* Release the component manager. */
     if (coma)
          coma->Release( coma );

     /* Release the super interface. */
     if (dale)
          dale->Release( dale );

     return ret;
}

/**********************************************************************************************************************/

static void
print_usage (const char *prg_name)
{
     fprintf (stderr, "\nFusionDale Coma Test (version %s)\n\n", FUSIONDALE_VERSION);
     fprintf (stderr, "Usage: %s [options]\n\n", prg_name);
     fprintf (stderr, "Options:\n");
     fprintf (stderr, "   -h   --help                             Show this help message\n");
     fprintf (stderr, "   -v   --version                          Print version information\n");
     fprintf (stderr, "   -c   --coma <name>                      Enter component manager\n");

     fprintf (stderr, "\n");
}

static bool
parse_command_line( int argc, char *argv[] )
{
     int n;

     for (n = 1; n < argc; n++) {
          const char *arg = argv[n];

          if (strcmp (arg, "-h") == 0 || strcmp (arg, "--help") == 0) {
               print_usage (argv[0]);
               return false;
          }

          if (strcmp (arg, "-v") == 0 || strcmp (arg, "--version") == 0) {
               fprintf (stderr, "fdtest_coma version %s\n", FUSIONDALE_VERSION);
               return false;
          }

          if (strcmp (arg, "-c") == 0 || strcmp (arg, "--coma") == 0) {
               if (++n == argc) {
                    print_usage( argv[0] );
                    return false;
               }

               enter_coma = argv[n];

               continue;
          }

          print_usage (argv[0]);
          return false;
     }

     return true;
}

