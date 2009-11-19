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

#include <directfb.h>


int
main( int argc, char *argv[] )
{
     DFBResult              ret;
     IDirectFB             *dfb;
     DFBSurfaceDescription  desc;
     IDirectFBSurface      *dst;
     IDirectFBSurface      *src;
     IDirectFBSurface      *src2;
     DFBColor               red   = { 0xC0, 0xc0, 0x00, 0x00 }; // premultiplied
     DFBColor               blue  = { 0xC0, 0x00, 0x00, 0xc0 }; // "
     DFBColor               white = { 0xFF, 0xff, 0xff, 0xff }; // "
     DFBRectangle           rect  = { 0, 0, 100, 100 };
     DFBPoint               p_dst = { 0, 0 };
     DFBPoint               p_src = { 50, 50 };

     /* Initialize DirectFB. */
     ret = DirectFBInit( &argc, &argv );
     if (ret) {
          D_DERROR( ret, "DFBTest/Blit2: DirectFBInit() failed!\n" );
          return ret;
     }


     /* Create super interface. */
     ret = DirectFBCreate( &dfb );
     if (ret) {
          D_DERROR( ret, "DFBTest/Blit2: DirectFBCreate() failed!\n" );
          return ret;
     }


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
     desc.flags       = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
     desc.width       = 100;
     desc.height      = 100;
     desc.pixelformat = DSPF_ARGB;

     ret = dfb->CreateSurface( dfb, &desc, &dst );
     if (ret) {
          D_DERROR( ret, "DFBTest/Blit2: CreateSurface(dst) failed!\n" );
          goto error_dst;
     }

     dst->Clear( dst, 0, 0, 0, 0 );

     ret = dfb->CreateSurface( dfb, &desc, &src );
     if (ret) {
          D_DERROR( ret, "DFBTest/Blit2: CreateSurface(src) failed!\n" );
          goto error_src;
     }

     desc.width  = 150;
     desc.height = 150;

     ret = dfb->CreateSurface( dfb, &desc, &src2 );
     if (ret) {
          D_DERROR( ret, "DFBTest/Blit2: CreateSurface(src2) failed!\n" );
          goto error_src2;
     }


     /* Fill source with red */
     src->Clear( src, red.r, red.g, red.b, red.a );

     src->Dump( src, "/", "dfbtest_blit2_src" );


     /* Fill source2 with blue/white */
     src2->Clear( src2, white.r, white.g, white.b, white.a );

     src2->SetColor( src2, blue.r, blue.g, blue.b, blue.a );
     src2->FillRectangles( src2, &rect, 1 );

     src2->Dump( src2, "/", "dfbtest_blit2_src2" );


     /*
      * Setup dual source blit
      */
     dst->SetBlittingFlags( dst, DSBLIT_BLEND_ALPHACHANNEL );

     dst->SetPorterDuff( dst, DSPD_SRC_OVER );

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
     dst->BatchBlit2( dst, src, src2, &rect, &p_dst, &p_src, 1 );

     dst->Dump( dst, "/", "dfbtest_blit2_dst" );



     src2->Release( src2 );

error_src2:
     src->Release( src );

error_src:
     dst->Release( dst );

error_dst:
     /* Shutdown DirectFB. */
     dfb->Release( dfb );

     return ret;
}

