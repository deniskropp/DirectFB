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

#include "config.h"

#include <unistd.h>
#include <stdio.h>

#include <directfb.h>
#include <directfb_strings.h>

#include <core/state.h>

#include <gfx/convert.h>


static DirectFBSurfaceBlittingFlagsNames( m_bflags );
static DirectFBSurfaceBlendFunctionNames( m_bfuncs );

#define MODULATE(a,b)    do { (a) = (((int)(a) * ((int)(b) + 1)) >> 8); } while (0)


static DFBColor
blit_pixel( CardState *state, DFBColor src, DFBColor dst )
{
     /* Scratch for blending stage. */
     DFBColor x;

     /*
      * Input => short circuit to Output? (simple blits)
      */

     /* Without any flag the source is simply copied. */
     if (!state->blittingflags)
          return src;

     /* Source color keying is the 2nd simplest operation. */
     if (state->blittingflags & DSBLIT_SRC_COLORKEY) {
          /* If the source matches the color key, keep the destination. */
          if (PIXEL_RGB32(src.r,src.g,src.b) == state->src_colorkey)
               return dst;
     }

     /* Destination color keying already requires reading the destination. */
     if (state->blittingflags & DSBLIT_DST_COLORKEY) {
          /* If the destination does not match the color key, keep the destination. */
          if (PIXEL_RGB32(dst.r,dst.g,dst.b) != state->dst_colorkey)
               return dst;
     }

     /*
      * Modulation stage
      */

     /* Modulate source alpha value with global alpha factor? */
     if (state->blittingflags & DSBLIT_BLEND_COLORALPHA) {
          /* Combine with source alpha value... */
          if (state->blittingflags & DSBLIT_BLEND_ALPHACHANNEL)
               MODULATE( src.a, state->color.a );
          else
               /* ...or replace it. */
               src.a = state->color.a;
     }

     /* Modulate source colors with global color factors? */
     if (state->blittingflags & DSBLIT_COLORIZE) {
          MODULATE( src.r, state->color.r );
          MODULATE( src.g, state->color.g );
          MODULATE( src.b, state->color.b );
     }

     /*
      * Premultiplication stage
      */

     /* Premultiply source colors with (modulated) source alpha value? */
     if (state->blittingflags & DSBLIT_SRC_PREMULTIPLY) {
          MODULATE( src.r, src.a );
          MODULATE( src.g, src.a );
          MODULATE( src.b, src.a );
     }

     /* Premultiply source colors with global alpha factor only? */
     if (state->blittingflags & DSBLIT_SRC_PREMULTCOLOR) {
          MODULATE( src.r, state->color.a );
          MODULATE( src.g, state->color.a );
          MODULATE( src.b, state->color.a );
     }

     /* Premultiply destination colors with destination alpha value? */
     if (state->blittingflags & DSBLIT_DST_PREMULTIPLY) {
          MODULATE( dst.r, dst.a );
          MODULATE( dst.g, dst.a );
          MODULATE( dst.b, dst.a );
     }

     /*
      * XOR comes right before blending, after load, modulate and premultiply.
      */
     if (state->blittingflags & DSBLIT_XOR) {
          src.a ^= dst.a;
          src.r ^= dst.r;
          src.g ^= dst.g;
          src.b ^= dst.b;
     }

     /*
      * Blending stage
      */

     /* Initialize scratch with source values, modify the copy according to the source blend function.
        Could be done better by writing to the scratch only once after the calculation. */
     x = src;

     /* Blend scratch (source copy) and destination values accordingly. */
     if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA)) {
          /* Apply the source blend function to the scratch. */
          switch (state->src_blend) {
               /* Sargb *= 0.0 */
               case DSBF_ZERO:
                    x.a = x.r = x.g = x.b = 0;
                    break;

               /* Sargb *= 1.0 */
               case DSBF_ONE:
                    break;

               /* Sargb *= Sargb */
               case DSBF_SRCCOLOR:
                    MODULATE( x.a, src.a );
                    MODULATE( x.r, src.r );
                    MODULATE( x.g, src.g );
                    MODULATE( x.b, src.b );
                    break;

               /* Sargb *= 1.0 - Sargb */
               case DSBF_INVSRCCOLOR:
                    MODULATE( x.a, src.a ^ 0xff );
                    MODULATE( x.r, src.r ^ 0xff );
                    MODULATE( x.g, src.g ^ 0xff );
                    MODULATE( x.b, src.b ^ 0xff );
                    break;

               /* Sargb *= Saaaa */
               case DSBF_SRCALPHA:
                    MODULATE( x.a, src.a );
                    MODULATE( x.r, src.a );
                    MODULATE( x.g, src.a );
                    MODULATE( x.b, src.a );
                    break;

               /* Sargb *= 1.0 - Saaaa */
               case DSBF_INVSRCALPHA:
                    MODULATE( x.a, src.a ^ 0xff );
                    MODULATE( x.r, src.a ^ 0xff );
                    MODULATE( x.g, src.a ^ 0xff );
                    MODULATE( x.b, src.a ^ 0xff );
                    break;

               /* Sargb *= Daaaa */
               case DSBF_DESTALPHA:
                    MODULATE( x.a, dst.a );
                    MODULATE( x.r, dst.a );
                    MODULATE( x.g, dst.a );
                    MODULATE( x.b, dst.a );
                    break;

               /* Sargb *= 1.0 - Daaaa */
               case DSBF_INVDESTALPHA:
                    MODULATE( x.a, dst.a ^ 0xff );
                    MODULATE( x.r, dst.a ^ 0xff );
                    MODULATE( x.g, dst.a ^ 0xff );
                    MODULATE( x.b, dst.a ^ 0xff );
                    break;

               /* Sargb *= Dargb */
               case DSBF_DESTCOLOR:
                    MODULATE( x.a, dst.a );
                    MODULATE( x.r, dst.r );
                    MODULATE( x.g, dst.g );
                    MODULATE( x.b, dst.b );
                    break;

               /* Sargb *= 1.0 - Dargb */
               case DSBF_INVDESTCOLOR:
                    MODULATE( x.a, dst.a ^ 0xff );
                    MODULATE( x.r, dst.r ^ 0xff );
                    MODULATE( x.g, dst.g ^ 0xff );
                    MODULATE( x.b, dst.b ^ 0xff );
                    break;

               /* ??? */
               case DSBF_SRCALPHASAT:
                    D_UNIMPLEMENTED();
                    break;

               default:
                    D_BUG( "unknown blend function %d", state->src_blend );
          }

          /* Apply the destination blend function. */
          switch (state->dst_blend) {
               /* Dargb *= 0.0 */
               case DSBF_ZERO:
                    dst.a = dst.r = dst.g = dst.b = 0;
                    break;

               /* Dargb *= 1.0 */
               case DSBF_ONE:
                    break;

               /* Dargb *= Sargb */
               case DSBF_SRCCOLOR:
                    MODULATE( dst.a, src.a );
                    MODULATE( dst.r, src.r );
                    MODULATE( dst.g, src.g );
                    MODULATE( dst.b, src.b );
                    break;

               /* Dargb *= 1.0 - Sargb */
               case DSBF_INVSRCCOLOR:
                    MODULATE( dst.a, src.a ^ 0xff );
                    MODULATE( dst.r, src.r ^ 0xff );
                    MODULATE( dst.g, src.g ^ 0xff );
                    MODULATE( dst.b, src.b ^ 0xff );
                    break;

               /* Dargb *= Saaaa */
               case DSBF_SRCALPHA:
                    MODULATE( dst.a, src.a );
                    MODULATE( dst.r, src.a );
                    MODULATE( dst.g, src.a );
                    MODULATE( dst.b, src.a );
                    break;

               /* Dargb *= 1.0 - Saaaa */
               case DSBF_INVSRCALPHA:
                    MODULATE( dst.a, src.a ^ 0xff );
                    MODULATE( dst.r, src.a ^ 0xff );
                    MODULATE( dst.g, src.a ^ 0xff );
                    MODULATE( dst.b, src.a ^ 0xff );
                    break;

               /* Dargb *= Daaaa */
               case DSBF_DESTALPHA:
                    MODULATE( dst.r, dst.a );
                    MODULATE( dst.g, dst.a );
                    MODULATE( dst.b, dst.a );
                    MODULATE( dst.a, dst.a ); //
                    break;

               /* Dargb *= 1.0 - Daaaa */
               case DSBF_INVDESTALPHA:
                    MODULATE( dst.r, dst.a ^ 0xff );
                    MODULATE( dst.g, dst.a ^ 0xff );
                    MODULATE( dst.b, dst.a ^ 0xff );
                    MODULATE( dst.a, dst.a ^ 0xff ); //
                    break;

               /* Dargb *= Dargb */
               case DSBF_DESTCOLOR:
                    MODULATE( dst.r, dst.r );
                    MODULATE( dst.g, dst.g );
                    MODULATE( dst.b, dst.b );
                    MODULATE( dst.a, dst.a ); //
                    break;

               /* Dargb *= 1.0 - Dargb */
               case DSBF_INVDESTCOLOR:
                    MODULATE( dst.r, dst.r ^ 0xff );
                    MODULATE( dst.g, dst.g ^ 0xff );
                    MODULATE( dst.b, dst.b ^ 0xff );
                    MODULATE( dst.a, dst.a ^ 0xff ); //
                    break;

               /* ??? */
               case DSBF_SRCALPHASAT:
                    D_UNIMPLEMENTED();
                    break;

               default:
                    D_BUG( "unknown blend function %d", state->dst_blend );
          }

          /*
           * Add blended destination values to the scratch.
           */
          x.a += dst.a;
          x.r += dst.r;
          x.g += dst.g;
          x.b += dst.b;
     }

     /* Better not use the conversion from premultiplied to non-premultiplied! */
     if (state->blittingflags & DSBLIT_DEMULTIPLY) {
          x.r = ((int)x.r << 8) / ((int)x.a + 1);
          x.g = ((int)x.g << 8) / ((int)x.a + 1);
          x.b = ((int)x.b << 8) / ((int)x.a + 1);
     }

     /*
      * Output
      */
     return x;
}

/**********************************************************************************************************************/

static const char *
blend_to_string( DFBSurfaceBlendFunction func )
{
     int i;

     for (i=0; i<D_ARRAY_SIZE(m_bfuncs); i++) {
          if (m_bfuncs[i].function == func)
               return m_bfuncs[i].name;
     }

     return "<unknown>";
}

/**********************************************************************************************************************/

static void
parse_flags( const char *arg, DFBSurfaceBlittingFlags *ret_flags )
{
     int i;

     *ret_flags = DSBLIT_NOFX;

     for (i=0; i<D_ARRAY_SIZE(m_bflags); i++) {
          if (strcasestr( arg, m_bflags[i].name ))
               *ret_flags |= m_bflags[i].flag;
     }
}

static bool
parse_color( const char *arg, DFBColor *ret_color )
{
     char *error;
     u32   argb;

     if (arg[0] == '#')
          arg++;

     if (arg[0] == '0' && arg[1] == 'x')
          arg+=2;

     argb = strtoul( arg, &error, 16 );

     if (*error) {
          fprintf( stderr, "Invalid characters in color string: '%s'\n", error );
          return false;
     }

     ret_color->a =  argb >> 24;
     ret_color->r = (argb & 0xFF0000) >> 16;
     ret_color->g = (argb & 0xFF00)   >> 8;
     ret_color->b =  argb & 0xFF;

     return true;
}

static bool
parse_blend_func( const char *arg, DFBSurfaceBlendFunction *ret_func )
{
     int i;

     for (i=0; i<D_ARRAY_SIZE(m_bfuncs); i++) {
          if (!strcasecmp( arg, m_bfuncs[i].name )) {
               *ret_func = m_bfuncs[i].function;
               return true;
          }
     }

     fprintf( stderr, "Unknown blend function: '%s'\n", arg );

     return false;
}

static void
print_usage (const char *prg_name)
{
     int i;

     fprintf( stderr, "\n"
                      "DirectFB Blitting FX Demonstrator (version %s)\n"
                      "\n"
                      "Usage: %s [options]\n"
                      "\n"
                      "Options:\n"
                      "   -b, --blittingflags   <flag>[,<flag>]     Set blitting flags\n"
                      "   -D, --destination     <0xAARRGGBB>        Set destination value (ARGB32 in hex)\n"
                      "   -S, --source          <0xAARRGGBB>        Set source value (ARGB32 in hex)\n"
                      "   -c, --color           <0xAARRGGBB>        Set color (ARGB32 in hex)\n"
                      "   -s, --srcblend        <func>              Set source blend function\n"
                      "   -d, --dstblend        <func>              Set destination blend function\n"
                      "\n"
                      "Blitting flags:\n", DIRECTFB_VERSION, prg_name );

     for (i=0; i<D_ARRAY_SIZE(m_bflags)-1; i++) {
          fprintf( stderr, "  %-20s", m_bflags[i].name );

          if (i % 4 == 3)
               fprintf( stderr, "\n" );
     }

     fprintf( stderr, "(any other value means NOFX)\n"
                      "\n"
                      "Blend functions:" );

     for (i=0; i<D_ARRAY_SIZE(m_bfuncs)-1; i++) {
          if (i % 4 == 0)
               fprintf( stderr, "\n" );

          fprintf( stderr, "  %-20s", m_bfuncs[i].name );
     }

     fprintf( stderr, "\n" );
}

static DFBBoolean
parse_command_line( int argc, char *argv[], CardState *state, DFBColor *dest, DFBColor *source )
{
     int i;

     /* Parse command line arguments. */
     for (i=1; i<argc; i++) {
          const char *arg = argv[i];

          if (strcmp (arg, "-h") == 0 || strcmp (arg, "--help") == 0) {
               print_usage (argv[0]);
               return DFB_FALSE;
          }

          if (strcmp (arg, "-v") == 0 || strcmp (arg, "--version") == 0) {
               fprintf (stderr, "dfbfx version %s\n", DIRECTFB_VERSION);
               return DFB_FALSE;
          }

          if (strcmp (arg, "-b") == 0 || strcmp (arg, "--blittingflags") == 0) {
               if (++i == argc) {
                    print_usage (argv[0]);
                    return DFB_FALSE;
               }

               parse_flags( argv[i], &state->blittingflags );

               continue;
          }

          if (strcmp (arg, "-D") == 0 || strcmp (arg, "--destination") == 0) {
               if (++i == argc) {
                    print_usage (argv[0]);
                    return DFB_FALSE;
               }

               if (parse_color( argv[i], dest ))
                    continue;
          }

          if (strcmp (arg, "-S") == 0 || strcmp (arg, "--source") == 0) {
               if (++i == argc) {
                    print_usage (argv[0]);
                    return DFB_FALSE;
               }

               if (parse_color( argv[i], source ))
                    continue;
          }

          if (strcmp (arg, "-c") == 0 || strcmp (arg, "--color") == 0) {
               if (++i == argc) {
                    print_usage (argv[0]);
                    return DFB_FALSE;
               }

               if (parse_color( argv[i], &state->color ))
                    continue;
          }

          if (strcmp (arg, "-s") == 0 || strcmp (arg, "--srcblend") == 0) {
               if (++i == argc) {
                    print_usage (argv[0]);
                    return DFB_FALSE;
               }

               if (parse_blend_func( argv[i], &state->src_blend ))
                    continue;
          }

          if (strcmp (arg, "-d") == 0 || strcmp (arg, "--dstblend") == 0) {
               if (++i == argc) {
                    print_usage (argv[0]);
                    return DFB_FALSE;
               }

               if (parse_blend_func( argv[i], &state->dst_blend ))
                    continue;
          }

          print_usage (argv[0]);

          return DFB_FALSE;
     }

     return DFB_TRUE;
}

/**********************************************************************************************************************/

int
main( int argc, char *argv[] )
{
     int       i;
     CardState state = {0};
     DFBColor  result;

     /* Initialize sample source and destination values. */
     //DFBColor  src = { 0x93, 0x93, 0x93, 0x93 };
     DFBColor  dst = { 0xf0, 0xe0, 0xe0, 0xe0 };

#define DRAWSTRING_PREMULT_FONT 1

     /* Initialize default rendering state. */
#if DRAWSTRING_PREMULT_FONT
     /* Initialize sample source and destination values. */
     DFBColor  src = { 0x93, 0x93, 0x93, 0x93 };

     state.blittingflags = DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_COLORIZE;
     state.src_blend     = DSBF_ONE;
     state.dst_blend     = DSBF_INVSRCALPHA;
     state.color.a       = 0x81;
     state.color.r       = 0xff;
     state.color.g       = 0x80;
     state.color.b       = 0x23;
#elif DRAWSTRING_NONPREMULT_ALPHADROP
     /* Initialize sample source and destination values. */
     DFBColor  src = { 0x93, 0xff, 0xff, 0xff };

     state.blittingflags = DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_COLORIZE;
     state.src_blend     = DSBF_SRCALPHA;
     state.dst_blend     = DSBF_INVSRCALPHA;
     state.color.a       = 0x81;
     state.color.r       = 0xff;
     state.color.g       = 0x80;
     state.color.b       = 0x23;
#endif

     /* Startup the blitting FX demonstrator. */
     printf( "\ndfbfx v" DIRECTFB_VERSION "\n\n" );

     if (!parse_command_line( argc, argv, &state, &dst, &src ))
          return -1;

     /* Show blitting flags being used. */
     printf( "  blit_flags: " );
     for (i=0; i<D_ARRAY_SIZE(m_bflags); i++) {
          if (D_FLAGS_IS_SET( state.blittingflags, m_bflags[i].flag ))
               printf( "%s ", m_bflags[i].name );
     }
     printf( "\n" );

     /* Blending needs source and destination blend function. */
     if (state.blittingflags & (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA)) {
          printf( "  src_blend:  %s\n", blend_to_string( state.src_blend ) );
          printf( "  dst_blend:  %s\n", blend_to_string( state.dst_blend ) );
     }
     
     /* These require one or more global values via the color (for modulation). */
     if (state.blittingflags & (DSBLIT_BLEND_COLORALPHA | DSBLIT_SRC_PREMULTCOLOR | DSBLIT_COLORIZE))
          printf( "  color:      %02x %02x %02x %02x\n",
                  state.color.a, state.color.r, state.color.g, state.color.b );

     /* Show original source values. */
     printf( "  src:        %02x %02x %02x %02x\n", src.a, src.r, src.g, src.b );

     /* Show source color key. */
     if (state.blittingflags & DSBLIT_SRC_COLORKEY)
          printf( "  src_key:       %02x %02x %02x\n",
                  state.src_colorkey >> 16, (state.src_colorkey >> 8) & 0xff, state.src_colorkey & 0xff );

     /* Show original destination values. */
     printf( "  dst:        %02x %02x %02x %02x\n", dst.a, dst.r, dst.g, dst.b );

     /* Show destination color key. */
     if (state.blittingflags & DSBLIT_DST_COLORKEY)
          printf( "  dst_key:       %02x %02x %02x\n",
                  state.dst_colorkey >> 16, (state.dst_colorkey >> 8) & 0xff, state.dst_colorkey & 0xff );

     /* Do magic... */
     result = blit_pixel( &state, src, dst );

     /* Show resulting values. */
     printf( "  result:     %02x %02x %02x %02x\n", result.a, result.r, result.g, result.b );

     return 0;
}

