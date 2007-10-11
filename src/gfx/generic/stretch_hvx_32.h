#ifndef SOURCE_LOOKUP
#define SOURCE_LOOKUP(x) (x)
#define SOURCE_LOOKUP_AUTO
#endif

#ifndef SOURCE_TYPE
#define SOURCE_TYPE u32
#define SOURCE_TYPE_AUTO
#endif

#if 0
#define HVX_DEBUG(x...)  direct_log_printf( NULL, x )
#else
#define HVX_DEBUG(x...)  do {} while (0)
#endif

/* stretch_hvx_up/down_32_KPI */( void             *dst,
                                  int               dpitch,
                                  const void       *src,
                                  int               spitch,
                                  int               width,
                                  int               height,
                                  int               dst_width,
                                  int               dst_height,
                                  const StretchCtx *ctx )
{
     long  x, y   = 0;
     long  cw     = ctx->clip.x2 - ctx->clip.x1 + 1;
     long  ch     = ctx->clip.y2 - ctx->clip.y1 + 1;
     long  hfraq  = ((long)(width  - MINUS_1) << 18) / (long)(dst_width);
     long  vfraq  = ((long)(height - MINUS_1) << 18) / (long)(dst_height);
     long  dp4    = dpitch / 4;
     long  point0 = POINT_0 + ctx->clip.x1 * hfraq;
     long  point  = point0;
     long  line   = LINE_0 + ctx->clip.y1 * vfraq;
     long  ratios[cw];
     u32  *dst32;

#if defined (COLOR_KEY) || defined (KEY_PROTECT)
     u32   dt;
#endif

     u32   _lbT[cw+8];
     u32   _lbB[cw+8];

     u32  *lbX;
     u32  *lbT = (u32*)((((ulong)(&_lbT[0])) + 31) & ~31);
     u32  *lbB = (u32*)((((ulong)(&_lbB[0])) + 31) & ~31);

     long  lineT = -2000;

     for (x=0; x<cw; x++) {
          ratios[x] = POINT_TO_RATIO( point, hfraq );

          point += hfraq;
     }

     HVX_DEBUG("%dx%d -> %dx%d  (0x%x, 0x%x)\n", width, height, dst_width, dst_height, hfraq, vfraq );

     dst += ctx->clip.x1 * 4 + ctx->clip.y1 * dpitch;

     dst32 = dst;

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
               u32 L, R;
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

                         lbT[x]= ((((((R & X_00FF00FF) - (L & X_00FF00FF))*ratios[x]) >> SHIFT_R8) + (L & X_00FF00FF)) & X_00FF00FF) +
                                 ((((((R>>SHIFT_R8) & X_00FF00FF) - ((L>>SHIFT_R8) & X_00FF00FF))*ratios[x]) + (L & X_FF00FF00)) & X_FF00FF00);

                         L = SOURCE_LOOKUP(srcB[pl]);
                         R = SOURCE_LOOKUP(srcB[pl+1]);

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
                         long pl = POINT_L( point, hfraq );
                         HVX_DEBUG("%ld,%ld %lu  (%lu/%lu)   0x%x  0x%x\n", x, y, pl,
                                   POINT_L( point, hfraq ), POINT_R( point, hfraq ), point, ratios[r] );
                         D_ASSERT( pl >= 0 );
                         D_ASSERT( pl < width-1 );

                         L = SOURCE_LOOKUP(srcB[pl]);
                         R = SOURCE_LOOKUP(srcB[pl+1]);

                         lbB[x] = ((((((R & X_00FF00FF) - (L & X_00FF00FF))*ratios[x]) >> SHIFT_R8) + (L & X_00FF00FF)) & X_00FF00FF) +
                                  ((((((R>>SHIFT_R8) & X_00FF00FF) - ((L>>SHIFT_R8) & X_00FF00FF))*ratios[x]) + (L & X_FF00FF00)) & X_FF00FF00);
                    }
               }

               lineT = nlT;
          }

          /*
           * Vertical interpolation
           */
          long X = LINE_TO_RATIO( line, vfraq );

          for (x=0; x<cw; x++) {
#if defined (COLOR_KEY) || defined (KEY_PROTECT)
               dt = ((((((lbB[x] & X_00FF00FF) - (lbT[x] & X_00FF00FF))*X) >> SHIFT_R8) + (lbT[x] & X_00FF00FF)) & X_00FF00FF) +
                    ((((((lbB[x]>>SHIFT_R8) & X_00FF00FF) - ((lbT[x]>>SHIFT_R8) & X_00FF00FF))*X) + (lbT[x] & X_FF00FF00)) & X_FF00FF00);
#ifdef COLOR_KEY
               if (dt != (COLOR_KEY))
#endif
#ifdef KEY_PROTECT
                    /* Write to destination with color key protection */
                    dst32[x] = ((dt & MASK_RGB) == KEY_PROTECT) ? dt^1 : dt;
#else
                    /* Write to destination without color key protection */
                    dst32[x] = dt;
#endif
#else
               /* Write to destination without color key protection */
               dst32[x] = ((((((lbB[x] & X_00FF00FF) - (lbT[x] & X_00FF00FF))*X) >> SHIFT_R8) + (lbT[x] & X_00FF00FF)) & X_00FF00FF) +
                          ((((((lbB[x]>>SHIFT_R8) & X_00FF00FF) - ((lbT[x]>>SHIFT_R8) & X_00FF00FF))*X) + (lbT[x] & X_FF00FF00)) & X_FF00FF00);
#endif
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

