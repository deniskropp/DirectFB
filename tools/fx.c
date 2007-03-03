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

#include <core/state.h>

#include <gfx/convert.h>


#define MODULATE(a,b)    do { (a) = (((int)(a) * ((int)(b) + 1)) >> 8); } while (0)

static DFBColor
blit_pixel( CardState *state, DFBColor src, DFBColor dst )
{
     DFBColor x;

     if (!state->blittingflags)
          return src;


     if (state->blittingflags & DSBLIT_DST_COLORKEY) {
          if (PIXEL_RGB32(dst.r,dst.g,dst.b) != state->dst_colorkey) {
               return dst;
          }
     }

     if (state->blittingflags & DSBLIT_SRC_COLORKEY) {
          if (PIXEL_RGB32(src.r,src.g,src.b) == state->src_colorkey) {
               return dst;
          }
     }



     if (state->blittingflags & DSBLIT_BLEND_COLORALPHA) {
          if (state->blittingflags & DSBLIT_BLEND_ALPHACHANNEL)
               MODULATE( src.a, state->color.a );
          else
               src.a = state->color.a;
     }


     if (state->blittingflags & DSBLIT_SRC_PREMULTIPLY) {
          MODULATE( src.r, src.a );
          MODULATE( src.g, src.a );
          MODULATE( src.b, src.a );
     }

     if (state->blittingflags & DSBLIT_SRC_PREMULTCOLOR) {
          MODULATE( src.r, state->color.a );
          MODULATE( src.g, state->color.a );
          MODULATE( src.b, state->color.a );
     }


     if (state->blittingflags & DSBLIT_COLORIZE) {
          MODULATE( src.r, state->color.r );
          MODULATE( src.g, state->color.g );
          MODULATE( src.b, state->color.b );
     }


     if (state->blittingflags & DSBLIT_DST_PREMULTIPLY) {
          MODULATE( dst.r, dst.a );
          MODULATE( dst.g, dst.a );
          MODULATE( dst.b, dst.a );
     }


     if (state->blittingflags & DSBLIT_XOR) {
          src.a ^= dst.a;
          src.r ^= dst.r;
          src.g ^= dst.g;
          src.b ^= dst.b;
     }

     x = src;

     if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA)) {
          switch (state->src_blend) {
               case DSBF_ZERO:
                    x.a = x.r = x.g = x.b = 0;
                    break;

               case DSBF_ONE:
                    break;

               case DSBF_SRCCOLOR:
                    MODULATE( x.a, src.a );
                    MODULATE( x.r, src.r );
                    MODULATE( x.g, src.g );
                    MODULATE( x.b, src.b );
                    break;

               case DSBF_INVSRCCOLOR:
                    MODULATE( x.a, src.a ^ 0xff );
                    MODULATE( x.r, src.r ^ 0xff );
                    MODULATE( x.g, src.g ^ 0xff );
                    MODULATE( x.b, src.b ^ 0xff );
                    break;

               case DSBF_SRCALPHA:
                    MODULATE( x.a, src.a );
                    MODULATE( x.r, src.a );
                    MODULATE( x.g, src.a );
                    MODULATE( x.b, src.a );
                    break;

               case DSBF_INVSRCALPHA:
                    MODULATE( x.a, src.a ^ 0xff );
                    MODULATE( x.r, src.a ^ 0xff );
                    MODULATE( x.g, src.a ^ 0xff );
                    MODULATE( x.b, src.a ^ 0xff );
                    break;

               case DSBF_DESTALPHA:
                    MODULATE( x.a, dst.a );
                    MODULATE( x.r, dst.a );
                    MODULATE( x.g, dst.a );
                    MODULATE( x.b, dst.a );
                    break;

               case DSBF_INVDESTALPHA:
                    MODULATE( x.a, dst.a ^ 0xff );
                    MODULATE( x.r, dst.a ^ 0xff );
                    MODULATE( x.g, dst.a ^ 0xff );
                    MODULATE( x.b, dst.a ^ 0xff );
                    break;

               case DSBF_DESTCOLOR:
                    MODULATE( x.a, dst.a );
                    MODULATE( x.r, dst.r );
                    MODULATE( x.g, dst.g );
                    MODULATE( x.b, dst.b );
                    break;

               case DSBF_INVDESTCOLOR:
                    MODULATE( x.a, dst.a ^ 0xff );
                    MODULATE( x.r, dst.r ^ 0xff );
                    MODULATE( x.g, dst.g ^ 0xff );
                    MODULATE( x.b, dst.b ^ 0xff );
                    break;

               case DSBF_SRCALPHASAT:
                    D_UNIMPLEMENTED();
                    break;

               default:
                    D_BUG( "unknown blend function %d", state->src_blend );
          }

          switch (state->dst_blend) {
               case DSBF_ZERO:
                    dst.a = dst.r = dst.g = dst.b = 0;
                    break;

               case DSBF_ONE:
                    break;

               case DSBF_SRCCOLOR:
                    MODULATE( dst.a, src.a );
                    MODULATE( dst.r, src.r );
                    MODULATE( dst.g, src.g );
                    MODULATE( dst.b, src.b );
                    break;

               case DSBF_INVSRCCOLOR:
                    MODULATE( dst.a, src.a ^ 0xff );
                    MODULATE( dst.r, src.r ^ 0xff );
                    MODULATE( dst.g, src.g ^ 0xff );
                    MODULATE( dst.b, src.b ^ 0xff );
                    break;

               case DSBF_SRCALPHA:
                    MODULATE( dst.a, src.a );
                    MODULATE( dst.r, src.a );
                    MODULATE( dst.g, src.a );
                    MODULATE( dst.b, src.a );
                    break;

               case DSBF_INVSRCALPHA:
                    MODULATE( dst.a, src.a ^ 0xff );
                    MODULATE( dst.r, src.a ^ 0xff );
                    MODULATE( dst.g, src.a ^ 0xff );
                    MODULATE( dst.b, src.a ^ 0xff );
                    break;

               case DSBF_DESTALPHA:
                    MODULATE( dst.r, dst.a );
                    MODULATE( dst.g, dst.a );
                    MODULATE( dst.b, dst.a );
                    MODULATE( dst.a, dst.a ); //
                    break;

               case DSBF_INVDESTALPHA:
                    MODULATE( dst.r, dst.a ^ 0xff );
                    MODULATE( dst.g, dst.a ^ 0xff );
                    MODULATE( dst.b, dst.a ^ 0xff );
                    MODULATE( dst.a, dst.a ^ 0xff ); //
                    break;

               case DSBF_DESTCOLOR:
                    MODULATE( dst.r, dst.r );
                    MODULATE( dst.g, dst.g );
                    MODULATE( dst.b, dst.b );
                    MODULATE( dst.a, dst.a ); //
                    break;

               case DSBF_INVDESTCOLOR:
                    MODULATE( dst.r, dst.r ^ 0xff );
                    MODULATE( dst.g, dst.g ^ 0xff );
                    MODULATE( dst.b, dst.b ^ 0xff );
                    MODULATE( dst.a, dst.a ^ 0xff ); //
                    break;

               case DSBF_SRCALPHASAT:
                    D_UNIMPLEMENTED();
                    break;

               default:
                    D_BUG( "unknown blend function %d", state->dst_blend );
          }

          x.a += dst.a;
          x.r += dst.r;
          x.g += dst.g;
          x.b += dst.b;
     }

     if (state->blittingflags & DSBLIT_DEMULTIPLY) {
          x.r = ((int)x.r << 8) / ((int)x.a + 1);
          x.g = ((int)x.g << 8) / ((int)x.a + 1);
          x.b = ((int)x.b << 8) / ((int)x.a + 1);
     }

     return x;
}

int
main()
{
     DFBColor  src = { 0xc0, 0x80, 0x20, 0xff };
     DFBColor  dst = { 0xd0, 0x20, 0x20, 0x00 };
     CardState state;
     DFBColor  result;

     state.blittingflags = DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_SRC_PREMULTIPLY;
     state.src_blend     = DSBF_ONE;
     state.dst_blend     = DSBF_INVSRCALPHA;
     
     result = blit_pixel( &state, src, dst );

     printf( "src:    %02x %02x %02x %02x\n", src.a, src.r, src.g, src.b );
     printf( "dst:    %02x %02x %02x %02x\n", dst.a, dst.r, dst.g, dst.b );
     printf( "result: %02x %02x %02x %02x\n", result.a, result.r, result.g, result.b );

     return 0;
}
