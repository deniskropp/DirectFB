#ifndef SOURCE_LOOKUP
#define SOURCE_LOOKUP(x) (x)
#define SOURCE_LOOKUP_AUTO
#endif

#ifndef SOURCE_TYPE
#define SOURCE_TYPE u32
#define SOURCE_TYPE_AUTO
#endif

/*static void STRETCH_HVX_ARGB32( void       *dst,
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
     int  dp4    = dpitch / 4;
     u32  point0 = POINT_0 + clip->x1 * hfraq;
     u32  point  = point0;
     u32  line   = LINE_0 + clip->y1 * vfraq;
     u32 *dst32;
     u32  ratios[cw];

     u32  _lbT[cw+8];
     u32  _lbB[cw+8];

     u32 *lbX;
     u32 *lbT = (u32*)((((ulong)(&_lbT[0])) + 31) & ~31);
     u32 *lbB = (u32*)((((ulong)(&_lbB[0])) + 31) & ~31);

     int  lineT = -2000;

     for (x=0; x<cw; x++) {
          ratios[x] = POINT_TO_RATIO( point, hfraq );

          point += hfraq;
     }

     dst += clip->x1 * 4 + clip->y1 * dpitch;

     dst32 = dst;

     /*
      * Scale line by line.
      */
     for (y=0; y<ch; y++) {
          int nlT = LINE_T( line, vfraq );

          /*
           * Fill line buffer(s) ?
           */
          if (nlT != lineT) {
               u32 L, R;
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

                         lbT[x]= ((((((R & X_00FF00FF) - (L & X_00FF00FF))*ratios[x]) >> SHIFT_R8) + (L & X_00FF00FF)) & X_00FF00FF) +
                                 ((((((R>>SHIFT_R8) & X_00FF00FF) - ((L>>SHIFT_R8) & X_00FF00FF))*ratios[x]) + (L & X_FF00FF00)) & X_FF00FF00);

                         L = SOURCE_LOOKUP(srcB[POINT_L( point, hfraq )]);
                         R = SOURCE_LOOKUP(srcB[POINT_R( point, hfraq )]);

                         lbB[x] = ((((((R & X_00FF00FF) - (L & X_00FF00FF))*ratios[x]) >> SHIFT_R8) + (L & X_00FF00FF)) & X_00FF00FF) +
                                  ((((((R>>SHIFT_R8) & X_00FF00FF) - ((L>>SHIFT_R8) & X_00FF00FF))*ratios[x]) + (L & X_FF00FF00)) & X_FF00FF00);
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

                         lbB[x] = ((((((R & X_00FF00FF) - (L & X_00FF00FF))*ratios[x]) >> SHIFT_R8) + (L & X_00FF00FF)) & X_00FF00FF) +
                                  ((((((R>>SHIFT_R8) & X_00FF00FF) - ((L>>SHIFT_R8) & X_00FF00FF))*ratios[x]) + (L & X_FF00FF00)) & X_FF00FF00);
                    }
               }

               lineT = nlT;
          }

          /*
           * Vertical interpolation
           */
          u32 X = LINE_TO_RATIO( line, vfraq );

          for (x=0; x<cw; x++) {
               dst32[x] = ((((((lbB[x] & X_00FF00FF) - (lbT[x] & X_00FF00FF))*X) >> SHIFT_R8) + (lbT[x] & X_00FF00FF)) & X_00FF00FF) +
                          ((((((lbB[x]>>SHIFT_R8) & X_00FF00FF) - ((lbT[x]>>SHIFT_R8) & X_00FF00FF))*X) + (lbT[x] & X_FF00FF00)) & X_FF00FF00);
          }

          dst32 += dp4;
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

