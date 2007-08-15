#ifndef SOURCE_LOOKUP
#define SOURCE_LOOKUP(x) (x)
#define SOURCE_LOOKUP_AUTO
#endif

#ifndef SOURCE_TYPE
#define SOURCE_TYPE u8
#define SOURCE_TYPE_AUTO
#endif

#if 0
#define HVX_DEBUG(x...)  direct_log_printf( NULL, x )
#else
#define HVX_DEBUG(x...)  do {} while (0)
#endif

/*** OPTIMIZE by doing four pixels at once (vertically) with a 32 bit line buffer ***/

/*static void STRETCH_HVX_YV12( void       *dst,
                               int         dpitch,
                               const void *src,
                               int         spitch,
                               int         width,
                               int         height,
                               int         dst_width,
                               int         dst_height,
                               DFBRegion  *clip )*/
{
     long  x, y   = 0;
     long  cw     = clip->x2 - clip->x1 + 1;
     long  ch     = clip->y2 - clip->y1 + 1;
     long  hfraq  = ((long)(width  - MINUS_1) << 18) / (long)(dst_width);
     long  vfraq  = ((long)(height - MINUS_1) << 18) / (long)(dst_height);
     long  point0 = POINT_0 + clip->x1 * hfraq;
     long  point  = point0;
     long  line   = LINE_0 + clip->y1 * vfraq;
     long  ratios[cw];
     u8   *dst8;

     u8    _lbT[cw+32];
     u8    _lbB[cw+32];

     u8   *lbX;
     u8   *lbT = (u8*)((((ulong)(&_lbT[0])) + 31) & ~31);
     u8   *lbB = (u8*)((((ulong)(&_lbB[0])) + 31) & ~31);

     long  lineT = -2000;

     for (x=0; x<cw; x++) {
          ratios[x] = POINT_TO_RATIO( point, hfraq );

          point += hfraq;
     }

     HVX_DEBUG("%dx%d -> %dx%d  (0x%x, 0x%x)\n", width, height, dst_width, dst_height, hfraq, vfraq );

     dst += clip->x1 + clip->y1 * dpitch;

     dst8 = dst;

     /*
      * Scale line by line.
      */
     for (y=0; y<ch; y++) {
          long nlT = LINE_T( line, vfraq );

          D_ASSERT( nlT >= 0 );
          D_ASSERT( nlT < height-1 );

          /*
           * Fill line buffer(s) ?
           */
          if (nlT != lineT) {
               u8 L, R;
               const SOURCE_TYPE *srcT = src + spitch * nlT;
               const SOURCE_TYPE *srcB = src + spitch * (nlT + 1);
               long               diff = nlT - lineT;

               if (diff > 1) {
                    /*
                     * Horizontal interpolation
                     */
                    for (x=0, point=point0; x<cw; x++, point += hfraq) {
                         long pl = POINT_L( point, hfraq );
                         HVX_DEBUG("%ld,%ld %lu  (%lu/%lu)   0x%x  0x%x\n", x, y, pl,
                                   POINT_L( point, hfraq ), POINT_R( point, hfraq ), point, ratios[r] );
                         D_ASSERT( pl >= 0 );
                         D_ASSERT( pl < width-1 );

                         L = SOURCE_LOOKUP(srcT[pl]);
                         R = SOURCE_LOOKUP(srcT[pl+1]);

                         lbT[x] = (((R - L) * ratios[x]) >> 8) + L;

                         L = SOURCE_LOOKUP(srcB[pl]);
                         R = SOURCE_LOOKUP(srcB[pl+1]);

                         lbB[x] = (((R - L) * ratios[x]) >> 8) + L;
                    }
               }
               else {
                    /* Swap */
                    lbX = lbT;
                    lbT = lbB;
                    lbB = lbX;

                    /*
                     * Horizontal interpolation
                     */
                    for (x=0, point=point0; x<cw; x++, point += hfraq) {
                         long pl = POINT_L( point, hfraq );
                         HVX_DEBUG("%ld,%ld %lu  (%lu/%lu)   0x%x  0x%x\n", x, y, pl,
                                   POINT_L( point, hfraq ), POINT_R( point, hfraq ), point, ratios[r] );
                         D_ASSERT( pl >= 0 );
                         D_ASSERT( pl < width-1 );

                         L = SOURCE_LOOKUP(srcB[pl]);
                         R = SOURCE_LOOKUP(srcB[pl+1]);

                         lbB[x] = (((R - L) * ratios[x]) >> 8) + L;
                    }
               }

               lineT = nlT;
          }

          /*
           * Vertical interpolation
           */
          long X = LINE_TO_RATIO( line, vfraq );

          for (x=0; x<cw; x++)
               dst8[x] = (((lbB[x] - lbT[x]) * X) >> 8) + lbT[x];

          dst8 += dpitch;
          line += vfraq;
     }
}

#ifdef SOURCE_LOOKUP_AUTO
#undef SOURCE_LOOKUP_AUTO
#undef SOURCE_LOOKUP
#endif

#ifdef SOURCE_TYPE_AUTO
#undef SOURCE_TYPE_AUTO
#undef SOURCE_TYPE
#endif

