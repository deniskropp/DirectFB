#ifndef SOURCE_LOOKUP
#define SOURCE_LOOKUP(x) (x)
#define SOURCE_LOOKUP_AUTO
#endif

#ifndef SOURCE_TYPE
#define SOURCE_TYPE u8
#define SOURCE_TYPE_AUTO
#endif


/*** OPTIMIZE by doing four pixels at once (vertically) with a 32 bit line buffer ***/

/*static void STRETCH_HVX_NV16( void       *dst,
                               int         dpitch,
                               const void *src,
                               int         spitch,
                               int         width,
                               int         height,
                               int         dst_width,
                               int         dst_height,
                               DFBRegion  *clip )*/
{
     int  x, y   = 0;
     int  cw     = clip->x2 - clip->x1 + 1;
     int  ch     = clip->y2 - clip->y1 + 1;
     u32  hfraq  = ((u32)width  << 18) / (u32)dst_width;
     u32  vfraq  = ((u32)height << 18) / (u32)dst_height;
     u32  point0 = POINT_0 + clip->x1 * hfraq;
     u32  point  = point0;
     u32  line   = LINE_0 + clip->y1 * vfraq;
     u8  *dst8;
     u32  ratios[cw];

     u8   _lbT[cw+32];
     u8   _lbB[cw+32];

     u8  *lbX;
     u8  *lbT = (u8*)((((ulong)(&_lbT[0])) + 31) & ~31);
     u8  *lbB = (u8*)((((ulong)(&_lbB[0])) + 31) & ~31);

     int  lineT = -2000;

     for (x=0; x<cw; x++) {
          ratios[x] = POINT_TO_RATIO( point, hfraq );

          point += hfraq;
     }

     dst += clip->x1 + clip->y1 * dpitch;

     dst8 = dst;

     /*
      * Scale line by line.
      */
     for (y=0; y<ch; y++) {
          int nlT = LINE_T( line, vfraq );

          /*
           * Fill line buffer(s) ?
           */
          if (nlT != lineT) {
               u8 L, R;
               const SOURCE_TYPE *srcT = src + spitch * nlT;
               const SOURCE_TYPE *srcB = src + spitch * (nlT + 1);
               int                diff = nlT - lineT;

               if (diff > 1) {
                    /*
                     * Horizontal interpolation
                     */
                    for (x=0, point=point0; x<cw; x++, point += hfraq) {
                         L = SOURCE_LOOKUP(srcT[POINT_L( point, hfraq )]);
                         R = SOURCE_LOOKUP(srcT[POINT_R( point, hfraq )]);

                         lbT[x] = (((R - L) * ratios[x]) >> 8) + L;

                         L = SOURCE_LOOKUP(srcB[POINT_L( point, hfraq )]);
                         R = SOURCE_LOOKUP(srcB[POINT_R( point, hfraq )]);

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
                         L = SOURCE_LOOKUP(srcB[POINT_L( point, hfraq )]);
                         R = SOURCE_LOOKUP(srcB[POINT_R( point, hfraq )]);

                         lbB[x] = (((R - L) * ratios[x]) >> 8) + L;
                    }
               }

               lineT = nlT;
          }

          /*
           * Vertical interpolation
           */
          u32 X = LINE_TO_RATIO( line, vfraq );

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

