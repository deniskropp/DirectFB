#ifndef SOURCE_LOOKUP
#define SOURCE_LOOKUP(x) (x)
#define SOURCE_LOOKUP_AUTO
#endif

#ifndef SOURCE_TYPE
#define SOURCE_TYPE u16
#define SOURCE_TYPE_AUTO
#endif

#if 0
#define HVX_DEBUG(x...)  direct_log_printf( NULL, x )
#else
#define HVX_DEBUG(x...)  do {} while (0)
#endif


/*** OPTIMIZE by doing two pixels at once (vertically) with a 32 bit line buffer ***/

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
     long  x, y   = 0;
     long  cw     = clip->x2 - clip->x1 + 1;
     long  ch     = clip->y2 - clip->y1 + 1;
     long  hfraq  = ((long)(width  - MINUS_1) << 18) / (long)(dst_width);
     long  vfraq  = ((long)(height - MINUS_1) << 18) / (long)(dst_height);
     long  point0 = POINT_0 + clip->x1 * hfraq;
     long  point  = point0;
     long  line   = LINE_0 + clip->y1 * vfraq;
     long  ratios[cw];
     u16  *dst16;

     u16   _lbT[cw+16];
     u16   _lbB[cw+16];

     u16  *lbX;
     u16  *lbT = (u16*)((((ulong)(&_lbT[0])) + 31) & ~31);
     u16  *lbB = (u16*)((((ulong)(&_lbB[0])) + 31) & ~31);

     long  lineT = -2000;

     for (x=0; x<cw; x++) {
          ratios[x] = POINT_TO_RATIO( point, hfraq );

          point += hfraq;
     }

     dst += clip->x1 * 2 + clip->y1 * dpitch;

     dst16 = dst;

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
               u16 L, R;
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

                         lbT[x] = (((((R & 0x00ff)-(L & 0x00ff))*ratios[x] + ((L & 0x00ff)<<8)) & 0x00ff00) + 
                                   ((((R & 0xff00)-(L & 0xff00))*ratios[x] + ((L & 0xff00)<<8)) & 0xff0000)) >> 8;

                         L = SOURCE_LOOKUP(srcB[pl]);
                         R = SOURCE_LOOKUP(srcB[pl+1]);

                         lbB[x] = (((((R & 0x00ff)-(L & 0x00ff))*ratios[x] + ((L & 0x00ff)<<8)) & 0x00ff00) + 
                                   ((((R & 0xff00)-(L & 0xff00))*ratios[x] + ((L & 0xff00)<<8)) & 0xff0000)) >> 8;
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

                         lbB[x] = (((((R & 0x00ff)-(L & 0x00ff))*ratios[x] + ((L & 0x00ff)<<8)) & 0x00ff00) + 
                                   ((((R & 0xff00)-(L & 0xff00))*ratios[x] + ((L & 0xff00)<<8)) & 0xff0000)) >> 8;
                    }
               }

               lineT = nlT;
          }

          /*
           * Vertical interpolation
           */
          long X = LINE_TO_RATIO( line, vfraq );

          for (x=0; x<cw; x++)
               dst16[x] = (((((lbB[x] & 0x00ff)-(lbT[x] & 0x00ff))*X + ((lbT[x] & 0x00ff)<<8)) & 0x00ff00) + 
                           ((((lbB[x] & 0xff00)-(lbT[x] & 0xff00))*X + ((lbT[x] & 0xff00)<<8)) & 0xff0000)) >> 8;

          dst16 += dpitch / 2;
          line  += vfraq;
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

