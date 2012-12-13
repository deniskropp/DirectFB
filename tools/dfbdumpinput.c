/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <direct/messages.h>
#include <direct/util.h>

#include <core/CoreInputDevice.h>

#include <core/input.h>

#include <directfb.h>
#include <directfb_keynames.h>
#include <directfb_strings.h>


/**************************************************************************************************/

static const DirectFBKeySymbolNames( symbol_names );

static const char *
symbol_name( DFBInputDeviceKeySymbol symbol )
{
     int i;
     static char buf[64];

     for (i=0; i<D_ARRAY_SIZE(symbol_names); i++) {
          if (symbol_names[i].symbol == symbol)
               return symbol_names[i].name;
     }

     snprintf( buf, sizeof(buf), "<0x%08x>", symbol );

     return buf;
}

/**************************************************************************************************/

static IDirectFB                 *dfb;
static IDirectFBEventBuffer      *events;
static unsigned int               sf_to_tt = false;

/**************************************************************************************************/

static bool parse_command_line( int argc, char *argv[] );

/**************************************************************************************************/

int
main( int argc, char *argv[] )
{
     DFBResult ret;

     /* Initialize DirectFB including command line parsing. */
     ret = DirectFBInit( &argc, &argv );
     if (ret) {
          D_DERROR( ret, "Tools/DumpInput: DirectFBInit() failed!\n" );
          goto error;
     }

     /* Parse the command line. */
     if (!parse_command_line( argc, argv ))
          goto error;

     /* Create the super interface. */
     ret = DirectFBCreate( &dfb );
     if (ret) {
          D_DERROR( ret, "Tools/DumpInput: DirectFBCreate() failed!\n" );
          goto error;
     }

     /* Get the input device. */
     ret = dfb->CreateInputEventBuffer( dfb, DICAPS_ALL, DFB_TRUE, &events );
     if (ret) {
          D_DERROR( ret, "Tools/DumpInput: IDirectFB::CreateInputEventBuffer() failed!\n" );
          goto error;
     }

     /* Dump the events. */
     while (true) {
          DFBInputEvent event[2];

          events->WaitForEvent( events );

          while (events->GetEvent( events, DFB_EVENT(event) ) == DFB_OK) {
               switch (event[0].type) {
                    case DIET_AXISMOTION:
                         if (event[0].flags & DIEF_AXISABS) {
                              event[0].axisabs = (event[0].axisabs - event[0].min) * 65535 / (event[0].max - event[0].min);
                              event[0].min = 0;
                              event[0].max = 65535;
                         }
                         break;

                    default:
                         break;
               }

               if (sf_to_tt) {
                    memcpy( &event[0].timestamp+4, (char*)(&event[0].timestamp)+8, sizeof(event[0]) );
                    memcpy( (char*)(&event[0].timestamp)+8, (char*)(&event[0].timestamp)+16, sizeof(event[0]) );

                    write( fileno(stdout), event, sizeof(event[0])-8 );
               }
               else
                    write( fileno(stdout), event, sizeof(event[0]) );
          }
     }

error:
     /* Release the buffer. */
     if (events)
          events->Release( events );

     /* Release the super interface. */
     if (dfb)
          dfb->Release( dfb );

     return ret;
}

/**************************************************************************************************/

typedef struct __AnyOption AnyOption;


typedef bool (*ParseFunc)( const AnyOption *option,
                           const char      *arg );

struct __AnyOption {
     const char   *short_name;
     const char   *long_name;

     const char   *arg_name;
     const char   *arg_desc;

     void         *value;

     unsigned int *flags;
     unsigned int  flag;

     ParseFunc     parse;
     const void   *data;
};

typedef struct {
     int           value;
     const char   *name;
} ValueName;

/**************************************************************************************************/

static bool
parse_int( const AnyOption *option, const char *arg )
{
     int   ret;
     char *end;

     ret = strtoul( arg, &end, option->data ? (unsigned long) option->data : 10 );

     if (*end || ret < 0) {
          fprintf( stderr, "\nInvalid argument to '%s' or '%s' specified!\n\n",
                   option->short_name, option->long_name );

          return false;
     }

     *((int*)option->value) = ret;

     return true;
}

/**************************************************************************************************/

static const AnyOption options[] = {
     { "-32",  "--convert-to-32bit",       "",           "Convert from 64bit to 32bit",
       NULL,     &sf_to_tt, true, NULL, NULL },
};

/**************************************************************************************************/

static void
print_usage (const char *prg_name)
{
     int i;

     fprintf (stderr, "\nDirectFB Input Device Configuration (version %s)\n\n", DIRECTFB_VERSION);
     fprintf (stderr, "Usage: %s [options]\n\n", prg_name);
     fprintf (stderr, "Options:\n");
     fprintf (stderr, "   -h   --help                             Show this help message\n");
     fprintf (stderr, "   -v   --version                          Print version information\n");

     for (i=0; i<D_ARRAY_SIZE(options); i++) {
          const AnyOption *option = &options[i];

          fprintf( stderr, "   %-3s  %-16s   %-12s    %s\n",
                   option->short_name, option->long_name, option->arg_name, option->arg_desc );
     }

     fprintf (stderr, "\n");
}

/**************************************************************************************************/

static bool
parse_option( const AnyOption *option, const char *arg )
{
     if (option->parse && !option->parse( option, arg ))
          return false;

     if (option->flags)
          *option->flags |= option->flag;

     return true;
}

static bool
parse_command_line( int argc, char *argv[] )
{
     int i, n;

     for (n = 1; n < argc; n++) {
          bool        ok  = false;
          const char *arg = argv[n];

          if (strcmp (arg, "-h") == 0 || strcmp (arg, "--help") == 0) {
               print_usage (argv[0]);
               return false;
          }

          if (strcmp (arg, "-v") == 0 || strcmp (arg, "--version") == 0) {
               fprintf (stderr, "dfbinput version %s\n", DIRECTFB_VERSION);
               return false;
          }

          for (i=0; i<D_ARRAY_SIZE(options); i++) {
               const AnyOption *opt = &options[i];

               if (!strcmp (arg, opt->short_name) || !strcmp (arg, opt->long_name)) {
                    if (opt->parse && ++n == argc) {
                         print_usage (argv[0]);
                         return false;
                    }

                    if (!parse_option( opt, argv[n] ))
                         return false;

                    ok = true;

                    break;
               }
          }

          if (!ok) {
               print_usage (argv[0]);
               return false;
          }
     }

     return true;
}

