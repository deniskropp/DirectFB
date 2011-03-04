/*
   (c) Copyright 2009  Denis Oliver Kropp <dok@directfb.org>

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

#include <direct/messages.h>

#include <core/core.h>
#include <core/gfxcard.h>
#include <core/state.h>
#include <core/surface.h>

#include <directfb.h>


int
main( int argc, char *argv[] )
{
     DFBResult     ret;
     IDirectFB    *dfb;
     CoreDFB      *core;
     CoreSurface  *dst;
     CoreSurface  *src;
     CoreSurface  *src2;
     CardState     state;
     DFBColor      red   = { 0xC0, 0xc0, 0x00, 0x00 }; // premultiplied
     DFBColor      blue  = { 0xC0, 0x00, 0x00, 0xc0 }; // "
     DFBColor      white = { 0xFF, 0xff, 0xff, 0xff }; // "
     DFBRegion     clip  = { 0, 0,  99,  99 };
     DFBRegion     c150  = { 0, 0, 149, 149 };
     DFBRectangle  rect  = { 0, 0, 100, 100 };
     DFBRectangle  r150  = { 0, 0, 150, 150 };
     DFBPoint      p_dst = { 0, 0 };
     DFBPoint      p_src = { 50, 50 };

     /* Initialize DirectFB. */
     ret = DirectFBInit( &argc, &argv );
     if (ret) {
          D_DERROR( ret, "CoreTest/Blit2: DirectFBInit() failed!\n" );
          return ret;
     }


     /* Create super interface. */
     ret = DirectFBCreate( &dfb );
     if (ret) {
          D_DERROR( ret, "CoreTest/Blit2: DirectFBCreate() failed!\n" );
          return ret;
     }

     dfb_core_create( &core );


     /*
      * This test creates three surfaces: 
      *  
      *  dst  .--------.    src  .--------.   src2  .-------------.
      *       |        |         |        |         |        |    |
      *       |        |         |  red   |         | blue   |    |
      *       |        |         |        |         |        |    |
      *       '--------'         '--------'         |--------'    |
      *                                             |      white  |
      *         100x100            100x100          '-------------'
      *  
      *                                                   150x150
      */

     ret = dfb_surface_create_simple( core, 100, 100, DSPF_ARGB, DSCS_RGB, DSCAPS_NONE, CSTF_NONE, 0, NULL, &dst );
     if (ret) {
          D_DERROR( ret, "CoreTest/Blit2: dfb_surface_create_simple(dst) failed!\n" );
          goto error_dst;
     }

     ret = dfb_surface_create_simple( core, 100, 100, DSPF_ARGB, DSCS_RGB, DSCAPS_NONE, CSTF_NONE, 0, NULL, &src );
     if (ret) {
          D_DERROR( ret, "CoreTest/Blit2: dfb_surface_create_simple(src) failed!\n" );
          goto error_src;
     }

     ret = dfb_surface_create_simple( core, 150, 150, DSPF_ARGB, DSCS_RGB, DSCAPS_NONE, CSTF_NONE, 0, NULL, &src2 );
     if (ret) {
          D_DERROR( ret, "CoreTest/Blit2: dfb_surface_create_simple(src2) failed!\n" );
          goto error_src2;
     }


     /* Initialize state */
     dfb_state_init( &state, core );


     /* Fill source with red */
     dfb_state_set_clip( &state, &clip );
     dfb_state_set_destination( &state, src );
     dfb_state_set_color( &state, &red );

     dfb_gfxcard_fillrectangles( &rect, 1, &state );

     dfb_surface_dump_buffer( src, CSBR_FRONT, "/", "coretest_blit2_src" );


     /* Fill source2 with blue/white */
     dfb_state_set_clip( &state, &c150 );
     dfb_state_set_destination( &state, src2 );

     dfb_state_set_color( &state, &white );
     dfb_gfxcard_fillrectangles( &r150, 1, &state );

     dfb_state_set_color( &state, &blue );
     dfb_gfxcard_fillrectangles( &rect, 1, &state );

     dfb_surface_dump_buffer( src2, CSBR_FRONT, "/", "coretest_blit2_src2" );


     /*
      * Setup dual source blit
      */
     dfb_state_set_clip( &state, &clip );
     dfb_state_set_destination( &state, dst );
     dfb_state_set_source( &state, src );
     dfb_state_set_source2( &state, src2 );

     dfb_state_set_blitting_flags( &state, DSBLIT_BLEND_ALPHACHANNEL );

     dfb_state_set_src_blend( &state, DSBF_ONE );
     dfb_state_set_dst_blend( &state, DSBF_INVSRCALPHA );

     /*
      * Perform dual source blit 
      *  
      *   100x100  at 0,0
      *  
      *    reading src  at  0, 0
      *    reading src2 at 50,50
      *  
      *  
      * Result should be: 
      *  
      *  .-------------.
      *  | red- |      |
      *  | blue |      |
      *  |      |      |
      *  |------'      |
      *  |             |
      *  |   red-      |
      *  |      white  |
      *  '-------------'
      *  
      *        100x100
      */
     dfb_gfxcard_batchblit2( &rect, &p_dst, &p_src, 1, &state );

     dfb_surface_dump_buffer( dst, CSBR_BACK, "/", "coretest_blit2_dst" );


     /* Shutdown state */
     dfb_state_set_destination( &state, NULL );
     dfb_state_set_source( &state, NULL );
     dfb_state_set_source2( &state, NULL );

     dfb_state_destroy( &state );



     dfb_surface_unref( src2 );

error_src2:
     dfb_surface_unref( src );

error_src:
     dfb_surface_unref( dst );

error_dst:
     dfb_core_destroy( core, false );

     /* Shutdown DirectFB. */
     dfb->Release( dfb );

     return ret;
}

