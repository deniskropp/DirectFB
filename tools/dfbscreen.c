/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#include <directfb.h>
#include <directfb_strings.h>

static const DirectFBScreenMixerTreeNames( tree_names );
static const DirectFBScreenEncoderScanModeNames( scan_mode_names );
static const DirectFBScreenEncoderTestPictureNames( test_picture_names );
static const DirectFBScreenEncoderTVStandardsNames( tv_standard_names );
static const DirectFBScreenOutputSignalsNames( signal_names );

/**************************************************************************************************/

static IDirectFB            *dfb;
static IDirectFBScreen      *screen;
static DFBScreenDescription  desc;

/**************************************************************************************************/

static DFBScreenID id      = DSCID_PRIMARY;
static int         mixer   = 0;
static int         encoder = 0;
static int         output  = 0;

/**************************************************************************************************/

static DFBScreenMixerConfig   mixer_config;
static DFBScreenEncoderConfig encoder_config;
static DFBScreenOutputConfig  output_config;

/**************************************************************************************************/

static bool parse_command_line( int argc, char *argv[] );

/**************************************************************************************************/

static void dump_mixer_config  ( const DFBScreenMixerConfig   *config );
static void dump_encoder_config( const DFBScreenEncoderConfig *config );

/**************************************************************************************************/

int
main( int argc, char *argv[] )
{
     DFBResult ret;

     /* Initialize DirectFB including command line parsing. */
     ret = DirectFBInit( &argc, &argv );
     if (ret) {
          D_DERROR( ret, "Tools/Screen: DirectFBInit() failed!\n" );
          goto error;
     }

     /* Parse the command line. */
     if (!parse_command_line( argc, argv ))
          goto error;

     /* Create the super interface. */
     ret = DirectFBCreate( &dfb );
     if (ret) {
          D_DERROR( ret, "Tools/Screen: DirectFBCreate() failed!\n" );
          goto error;
     }

     /* Get the primary display screen. */
     ret = dfb->GetScreen( dfb, id, &screen );
     if (ret) {
          if (ret == DFB_IDNOTFOUND)
               fprintf (stderr, "\nUnknown screen id, check 'dfbinfo' for valid values.\n\n");
          else
               D_DERROR( ret, "Tools/Screen: IDirectFB::GetScreen() failed!\n" );

          goto error;
     }

     /* Get a description of the screen. */
     ret = screen->GetDescription( screen, &desc );
     if (ret) {
          D_DERROR( ret, "Tools/Screen: IDirectFBScreen::GetDescription() failed!\n" );
          goto error;
     }

     /* Check arguments. */
     if (mixer_config.flags && (mixer < 0 || mixer >= desc.mixers)) {
          fprintf (stderr, "\nUnknown mixer (%d), check 'dfbinfo' for valid values.\n\n", mixer);
          goto error;
     }

     if (encoder_config.flags && (encoder < 0 || encoder >= desc.encoders)) {
          fprintf (stderr, "\nUnknown encoder (%d), check 'dfbinfo' for valid values.\n\n", encoder);
          goto error;
     }

     if (output_config.flags && (output < 0 || output >= desc.outputs)) {
          fprintf (stderr, "\nUnknown output (%d), check 'dfbinfo' for valid values.\n\n", output);
          goto error;
     }

     printf( "\n" );

     /* Do mixer. */
     if (mixer < desc.mixers && mixer >= 0) {
          DFBScreenMixerConfig config;

          printf( "\nMixer %d\n", mixer );

          if (mixer_config.flags) {
               ret = screen->SetMixerConfiguration( screen, mixer, &mixer_config );
               if (ret) {
                    D_DERROR( ret, "Tools/Screen: "
                              "IDirectFBScreen::SetMixerConfiguration(%d) failed!\n", mixer );
                    goto error;
               }
          }

          ret = screen->GetMixerConfiguration( screen, mixer, &config );
          if (ret) {
               D_DERROR( ret, "Tools/Screen: "
                         "IDirectFBScreen::GetMixerConfiguration(%d) failed!\n", mixer );
               goto error;
          }

          dump_mixer_config( &config );
     }

     /* Do encoder. */
     if (encoder < desc.encoders && encoder >= 0) {
          DFBScreenEncoderConfig config;

          printf( "\nEncoder %d\n", encoder );

          if (encoder_config.flags) {
               ret = screen->SetEncoderConfiguration( screen, encoder, &encoder_config );
               if (ret) {
                    D_DERROR( ret, "Tools/Screen: "
                              "IDirectFBScreen::SetEncoderConfiguration(%d) failed!\n", encoder );
                    goto error;
               }
          }

          ret = screen->GetEncoderConfiguration( screen, encoder, &config );
          if (ret) {
               D_DERROR( ret, "Tools/Screen: "
                         "IDirectFBScreen::GetEncoderConfiguration(%d) failed!\n", mixer );
               goto error;
          }

          dump_encoder_config( &config );
     }

error:
     /* Release the display screen. */
     if (screen)
          screen->Release( screen );

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

static bool parse_int  ( const AnyOption *option,
                         const char      *arg );

static bool parse_enum ( const AnyOption *option,
                         const char      *arg );

static bool parse_ids  ( const AnyOption *option,
                         const char      *arg );

static bool parse_color( const AnyOption *option,
                         const char      *arg );

/**************************************************************************************************/

static const AnyOption options[] = {
     { "-s",  "--screen",       "<id>",       "ID of screen to use",
       &id,      NULL, 0, parse_int, NULL },


     { "-m",  "--mixer",        "<index>",    "Index of mixer to use",
       &mixer,   NULL, 0, parse_int, NULL },

     { "-mt", "--tree",         "<mode>",     "Set (sub) tree mode",
       &mixer_config.tree, &mixer_config.flags,
       DSMCONF_TREE, parse_enum, tree_names },

     { "-mm", "--max-level",    "<level>",    "Set maximum level for SUB_LEVEL mode",
       &mixer_config.level, &mixer_config.flags,
       DSMCONF_LEVEL, parse_int, NULL },

     { "-ml", "--layers",       "<layers>",   "Select layers for SUB_LAYERS mode",
       &mixer_config.layers, &mixer_config.flags,
       DSMCONF_LAYERS, parse_ids, NULL },

     { "-mb", "--background",   "<rgb>",      "Set background color (hex)",
       &mixer_config.background, &mixer_config.flags,
       DSMCONF_BACKGROUND, parse_color, NULL },


     { "-e",  "--encoder",      "<index>",    "Index of encoder to use",
       &encoder, NULL, 0, parse_int, NULL },

     { "-et", "--tv-standard",  "<standard>", "Set TV standard",
       &encoder_config.tv_standard, &encoder_config.flags,
       DSECONF_TV_STANDARD, parse_enum, tv_standard_names },

     { "-ep", "--test-picture", "<mode>",     "Set test picture mode",
       &encoder_config.test_picture, &encoder_config.flags,
       DSECONF_TEST_PICTURE, parse_enum, test_picture_names },

     { "-em", "--sel-mixer",    "<index>",    "Select mixer for input",
       &encoder_config.mixer, &encoder_config.flags,
       DSECONF_MIXER, parse_int, NULL },

     { "-es", "--encode-sigs",  "<signals>",  "Select signal(s) to encode",
       &encoder_config.out_signals, &encoder_config.flags,
       DSECONF_OUT_SIGNALS, parse_enum, signal_names },

     { "-ec", "--scan-mode",    "<mode>",     "Set scan mode",
       &encoder_config.scanmode, &encoder_config.flags,
       DSECONF_SCANMODE, parse_enum, scan_mode_names },


     { "-o",  "--output",       "<index>",    "Index of output to use",
       &output,  NULL, 0, parse_int, NULL }
};

static void
print_usage (const char *prg_name)
{
     int i;

     fprintf (stderr, "\nDirectFB Screen Configuration (version %s)\n\n", DIRECTFB_VERSION);
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

static bool
parse_int( const AnyOption *option, const char *arg )
{
     int   ret;
     char *end;

     ret = strtoul( arg, &end, option->data ? (int) option->data : 10 );

     if (*end || ret < 0) {
          fprintf( stderr, "\nInvalid argument to '%s' or '%s' specified!\n\n",
                   option->short_name, option->long_name );

          return false;
     }

     *((int*)option->value) = ret;

     return true;
}

static bool
parse_enum( const AnyOption *option, const char *arg )
{
     int val = 0;

     if (! strcasecmp( arg, "help" )) {
          const ValueName *vn = option->data;

          fprintf( stderr, "\nPossible arguments to '%s' or '%s':\n",
                   option->short_name, option->long_name );

          do {
               fprintf( stderr, "    %s\n", vn->name );
          } while (vn++->value);

          fprintf (stderr, "\n");

          return false;
     }

     while (arg[0] == ',')
          arg++;

     while (arg[0]) {
          char            *p;
          int              len;
          int              vc = 0;
          bool             ok = false;
          const ValueName *vn = option->data;

          p = strchr( arg, ',' );
          if (p)
               len = p - arg;
          else
               len = strlen( arg );

          do {
               int vlen = strlen(vn->name);

               if (strncasecmp( vn->name, arg, len ))
                    continue;

               vc = vn->value;
               ok = true;

               if (vlen == len)
                    break;
          } while (vn++->value);

          if (ok)
               val |= vc;
          else {
               fprintf( stderr, "\nInvalid argument to '%s' or '%s' specified, "
                        "pass 'help' for a list!\n\n", option->short_name, option->long_name );
               return false;
          }

          arg += len;

          while (arg[0] == ',')
               arg++;
     }

     *((int*)option->value) = val;

     return true;
}

static bool
parse_ids( const AnyOption *option, const char *arg )
{
     __u32  val  = 0;
     int    alen = strlen( arg );
     char  *abuf = alloca( alen + 1 );

     memcpy( abuf, arg, alen + 1 );

     if (! strcasecmp( arg, "help" )) {
          fprintf( stderr, "\nCheck 'dfbinfo' for valid values.\n\n" );
          return false;
     }

     while (abuf[0] == ',')
          abuf++;

     while (abuf[0]) {
          char *p;
          int   len;
          int   ret;

          p = strchr( abuf, ',' );
          if (p) {
               len = p - abuf;

               abuf[len++] = 0;
          }
          else
               len = strlen( abuf );

          ret = strtoul( abuf, &p, 10 );

          if (*p || ret < 0) {
               fprintf( stderr, "\nInvalid argument (%s) to '%s' or '%s' specified!\n\n",
                        p, option->short_name, option->long_name );

               return false;
          }

          val |= (1 << ret);

          abuf += len;

          while (abuf[0] == ',')
               abuf++;
     }

     *((__u32*)option->value) = val;

     return true;
}

static bool
parse_color( const AnyOption *option, const char *arg )
{
     unsigned long  ret;
     char          *end;
     DFBColor      *color = option->value;

     ret = strtoul( arg, &end, 16 );

     if (*end) {
          fprintf( stderr, "\nInvalid argument (%s) to '%s' or '%s' specified!\n\n",
                   end, option->short_name, option->long_name );

          return false;
     }

     color->a = (ret >> 24) & 0xff;
     color->r = (ret >> 16) & 0xff;
     color->g = (ret >>  8) & 0xff;
     color->b = (ret      ) & 0xff;

     return true;
}

static bool
parse_option( const AnyOption *option, const char *arg )
{
     if (!option->parse( option, arg ))
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
               fprintf (stderr, "dfbscreen version %s\n", DIRECTFB_VERSION);
               return false;
          }

          for (i=0; i<D_ARRAY_SIZE(options); i++) {
               const AnyOption *opt = &options[i];

               if (!strcmp (arg, opt->short_name) || !strcmp (arg, opt->long_name)) {
                    if (++n == argc) {
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

/**************************************************************************************************/

static const char *
tree_name( DFBScreenMixerTree tree )
{
     int i;

     for (i=0; i<D_ARRAY_SIZE(tree_names); i++) {
          if (tree_names[i].tree == tree)
               return tree_names[i].name;
     }

     return "<invalid>";
}

static void
dump_mixer_config( const DFBScreenMixerConfig *config )
{
     int n;

     printf( "\n" );

     if (config->flags & DSMCONF_TREE)
          printf( "Tree:          %s\n", tree_name( config->tree ) );

     if (config->flags & DSMCONF_LEVEL)
          printf( "Level:         %d\n", config->level );

     if (config->flags & DSMCONF_LAYERS) {
          printf( "Layers:        " );

          for (n=0; n<DFB_DISPLAYLAYER_IDS_MAX; n++) {
               if (DFB_DISPLAYLAYER_IDS_HAVE( config->layers, n ))
                    printf( "(%02x) ", n );
          }

          printf( "\n" );
     }

     if (config->flags & DSMCONF_BACKGROUND)
          printf( "Background:    0x%02x, 0x%02x, 0x%02x (RGB)\n",
                  config->background.r, config->background.g, config->background.b );

     printf( "\n" );
}

/**************************************************************************************************/

static const char *
test_picture_name( DFBScreenEncoderTestPicture test_picture )
{
     int i;

     for (i=0; i<D_ARRAY_SIZE(test_picture_names); i++) {
          if (test_picture_names[i].test_picture == test_picture)
               return test_picture_names[i].name;
     }

     return "<invalid>";
}

static const char *
scan_mode_name( DFBScreenEncoderTestPicture scan_mode )
{
     int i;

     for (i=0; i<D_ARRAY_SIZE(scan_mode_names); i++) {
          if (scan_mode_names[i].scan_mode == scan_mode)
               return scan_mode_names[i].name;
     }

     return "<invalid>";
}

static void
dump_encoder_config( const DFBScreenEncoderConfig *config )
{
     int i;

     printf( "\n" );

     if (config->flags & DSECONF_TV_STANDARD) {
          printf( "TV Standard:   " );

          for (i=0; i<D_ARRAY_SIZE(tv_standard_names); i++) {
               if (config->tv_standard & tv_standard_names[i].standard)
                    printf( "%s ", tv_standard_names[i].name );
          }

          printf( "\n" );
     }

     if (config->flags & DSECONF_TEST_PICTURE)
          printf( "Test Picture:  %s\n", test_picture_name( config->test_picture ) );

     if (config->flags & DSECONF_MIXER)
          printf( "Mixer:         %d\n", config->mixer );

     if (config->flags & DSECONF_OUT_SIGNALS) {
          printf( "Signals:       " );

          for (i=0; i<D_ARRAY_SIZE(signal_names); i++) {
               if (config->out_signals & signal_names[i].signal)
                    printf( "%s ", signal_names[i].name );
          }

          printf( "\n" );
     }

     if (config->flags & DSECONF_SCANMODE)
          printf( "Scan Mode:     %s\n", scan_mode_name( config->scanmode ) );

     printf( "\n" );
}

