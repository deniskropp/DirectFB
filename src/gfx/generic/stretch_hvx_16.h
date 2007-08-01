#ifndef SOURCE_LOOKUP
#define SOURCE_LOOKUP(x) (x)
#define SOURCE_LOOKUP_AUTO
#endif

#ifndef SOURCE_TYPE
#define SOURCE_TYPE u16
#define SOURCE_TYPE_AUTO
#endif


#define SHIFT_L5    SHIFT_R5
#define SHIFT_L6    SHIFT_R6
#define SHIFT_L10   (16-SHIFT_L6)

#define X_003F      (X_07E0>>SHIFT_R5)

#define X_003E07C0  (X_F81F<<SHIFT_L6)
#define X_0001F800  (X_07E0<<SHIFT_L6)

#define X_07E0F81F  ((X_07E0<<16) | X_F81F)
#define X_F81F07E0  ((X_F81F<<16) | X_07E0)

#define X_F81FF81F  ((X_F81F<<16) | X_F81F)
#define X_07E007E0  ((X_07E0<<16) | X_07E0)

#define X_07C0F83F  (X_F81F07E0>>SHIFT_R5)

#if 0
#define HVX_DEBUG(x...)  direct_log_printf( NULL, x )
#else
#define HVX_DEBUG(x...)  do {} while (0)
#endif

/*static void STRETCH_HVX_RGB16( void       *dst,
                               int         dpitch,
                               const void *src,
                               int         spitch,
                               int         width,
                               int         height,
                               int         dst_width,
                               int         dst_height,
                               DFBRegion  *clip )*/
{
     long  x, y, r = 0;
     long  head    = ((((ulong) dst) & 2) >> 1) ^ (clip->x1 & 1);
     long  cw      = clip->x2 - clip->x1 + 1;
     long  ch      = clip->y2 - clip->y1 + 1;
     long  tail    = (cw - head) & 1;
     long  w2      = (cw - head) / 2;
     long  hfraq   = ((long)(width  - (POINT_0 ? 0 : 1) ) << 18) / (long)(dst_width);
     long  vfraq   = ((long)(height - (LINE_0  ? 0 : 1) ) << 18) / (long)(dst_height);
     long  dp4     = dpitch / 4;
     long  point0  = POINT_0 + clip->x1 * hfraq;
     long  point   = point0;
     long  line    = LINE_0 + clip->y1 * vfraq;
     long  ratios[cw];
     u32  *dst32;

#if defined (COLOR_KEY) || defined (COLOR_KEY_PROTECT)
     u32   dt;
     u16   l_, h_;
#endif

     u32   _lbT[w2+8];
     u32   _lbB[w2+8];

     u32  *lbX;
     u32  *lbT = (u32*)((((ulong)(&_lbT[0])) + 31) & ~31);
     u32  *lbB = (u32*)((((ulong)(&_lbB[0])) + 31) & ~31);

     long  lineT = -2000;

     for (x=0; x<cw; x++) {
          ratios[x] = POINT_TO_RATIO( point, hfraq );

          point += hfraq;
     }

     HVX_DEBUG("%dx%d -> %dx%d  (0x%x, 0x%x)\n", width, height, dst_width, dst_height, hfraq, vfraq );

     dst += clip->x1 * 2 + clip->y1 * dpitch;

     dst32 = dst;

     if (head) {
          u32 dpT, dpB, L, R;

          u16 *dst16 = dst;

          point = point0;

          for (y=0; y<ch; y++) {
               long X = LINE_TO_RATIO( line, vfraq );

               const SOURCE_TYPE *srcT = src + spitch * LINE_T( line, vfraq );
               const SOURCE_TYPE *srcB = src + spitch * LINE_B( line, vfraq );

               /*
                * Horizontal interpolation
                */
               long pl = POINT_L( point, hfraq );
               HVX_DEBUG("h,%ld %lu  (%lu/%lu)   0x%x  0x%x\n", y, pl,
                         POINT_L( point, hfraq ), POINT_R( point, hfraq ), point, ratios[r] );
               D_ASSERT( pl >= 0 );
               D_ASSERT( pl < width-1 );
               L = SOURCE_LOOKUP(srcT[pl]);
               R = SOURCE_LOOKUP(srcT[pl+1]);

               dpT = (((((R & X_F81F)-(L & X_F81F))*ratios[r] + ((L & X_F81F)<<SHIFT_L6)) & X_003E07C0) + 
                      ((((R & X_07E0)-(L & X_07E0))*ratios[r] + ((L & X_07E0)<<SHIFT_L6)) & X_0001F800)) >> SHIFT_R6;

               L = SOURCE_LOOKUP(srcB[pl]);
               R = SOURCE_LOOKUP(srcB[pl+1]);

               dpB = (((((R & X_F81F)-(L & X_F81F))*ratios[r] + ((L & X_F81F)<<SHIFT_L6)) & X_003E07C0) + 
                      ((((R & X_07E0)-(L & X_07E0))*ratios[r] + ((L & X_07E0)<<SHIFT_L6)) & X_0001F800)) >> SHIFT_R6;

               /*
                * Vertical interpolation
                */
#if defined (COLOR_KEY) || defined (COLOR_KEY_PROTECT)
               l_ = ((((((dpB & X_F81F) - (dpT & X_F81F))*X) >> SHIFT_R5) + (dpT & X_F81F)) & X_F81F) +
                    ((((((dpB>>SHIFT_R5) & X_003F) - ((dpT>>SHIFT_R5) & X_003F))*X) + (dpT & X_07E0)) & X_07E0);
#ifdef COLOR_KEY
               if (l_ != (COLOR_KEY))
#endif
#ifdef COLOR_KEY_PROTECT
                    /* Write to destination with color key protection */
                    dst16[0] = (l_ == KEY_PROTECT) ? KEY_REPLACE : l_;
#else
                    /* Write to destination without color key protection */
                    dst16[0] = l_;
#endif
#else
               /* Write to destination without color key protection */
               dst16[0] = ((((((dpB & X_F81F) - (dpT & X_F81F))*X) >> SHIFT_R5) + (dpT & X_F81F)) & X_F81F) +
                          ((((((dpB>>SHIFT_R5) & X_003F) - ((dpT>>SHIFT_R5) & X_003F))*X) + (dpT & X_07E0)) & X_07E0);
#endif

               dst16 += dpitch / 2;
               line  += vfraq;
          }

          /* Adjust */
          point0 += hfraq;
          dst32   = dst + 2;

          /* Reset */
          line = LINE_0 + clip->y1 * vfraq;
     }

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
               u32 L, R, dpT, dpB;
               const SOURCE_TYPE *srcT = src + spitch * nlT;
               const SOURCE_TYPE *srcB = src + spitch * (nlT + 1);
               long               diff = nlT - lineT;

               if (diff > 1) {
                    /*
                     * Two output pixels per step.
                     */
                    for (x=0, r=head, point=point0; x<w2; x++) {
                         /*
                          * Horizontal interpolation
                          */
                         long pl = POINT_L( point, hfraq );
                         HVX_DEBUG("%ld,%ld %lu  (%lu/%lu)   0x%x  0x%x\n", x, y, pl,
                                   POINT_L( point, hfraq ), POINT_R( point, hfraq ), point, ratios[r] );
                         D_ASSERT( pl >= 0 );
                         D_ASSERT( pl < width-1 );
                         L = SOURCE_LOOKUP(srcT[pl]);
                         R = SOURCE_LOOKUP(srcT[pl+1]);

                         dpT = (((((R & X_F81F)-(L & X_F81F))*ratios[r] + ((L & X_F81F)<<SHIFT_L6)) & X_003E07C0) + 
                                ((((R & X_07E0)-(L & X_07E0))*ratios[r] + ((L & X_07E0)<<SHIFT_L6)) & X_0001F800))
#ifdef WORDS_BIGENDIAN
                              << SHIFT_L10;
#else
                              >> SHIFT_R6;
#endif

                         L = SOURCE_LOOKUP(srcB[pl]);
                         R = SOURCE_LOOKUP(srcB[pl+1]);

                         dpB = (((((R & X_F81F)-(L & X_F81F))*ratios[r] + ((L & X_F81F)<<SHIFT_L6)) & X_003E07C0) + 
                                ((((R & X_07E0)-(L & X_07E0))*ratios[r] + ((L & X_07E0)<<SHIFT_L6)) & X_0001F800))
#ifdef WORDS_BIGENDIAN
                              << SHIFT_L10;
#else
                              >> SHIFT_R6;
#endif

                         point += hfraq;
                         r++;


                         pl = POINT_L( point, hfraq );
                         HVX_DEBUG("%ld,%ld %lu  (%lu/%lu)   0x%x  0x%x\n", x, y, pl,
                                   POINT_L( point, hfraq ), POINT_R( point, hfraq ), point, ratios[r] );
                         D_ASSERT( pl >= 0 );
                         D_ASSERT( pl < width-1 );

                         L = SOURCE_LOOKUP(srcT[pl]);
                         R = SOURCE_LOOKUP(srcT[pl+1]);

                         dpT |= (((((R & X_F81F)-(L & X_F81F))*ratios[r] + ((L & X_F81F)<<SHIFT_L6)) & X_003E07C0) + 
                                 ((((R & X_07E0)-(L & X_07E0))*ratios[r] + ((L & X_07E0)<<SHIFT_L6)) & X_0001F800))
#ifdef WORDS_BIGENDIAN
                              >> SHIFT_R6;
#else
                              << SHIFT_L10;
#endif

                         L = SOURCE_LOOKUP(srcB[pl]);
                         R = SOURCE_LOOKUP(srcB[pl+1]);

                         dpB |= (((((R & X_F81F)-(L & X_F81F))*ratios[r] + ((L & X_F81F)<<SHIFT_L6)) & X_003E07C0) + 
                                 ((((R & X_07E0)-(L & X_07E0))*ratios[r] + ((L & X_07E0)<<SHIFT_L6)) & X_0001F800))
#ifdef WORDS_BIGENDIAN
                              >> SHIFT_R6;
#else
                              << SHIFT_L10;
#endif

                         point += hfraq;
                         r++;

                         /* Store */
                         lbT[x] = dpT;
                         lbB[x] = dpB;
                    }
               }
               else {
                    /* Swap */
                    lbX = lbT;
                    lbT = lbB;
                    lbB = lbX;

                    /*
                     * Two output pixels per step.
                     */
                    for (x=0, r=head, point=point0; x<w2; x++) {
                         /*
                          * Horizontal interpolation
                          */
                         long pl = POINT_L( point, hfraq );
                         HVX_DEBUG("%ld,%ld %lu  (%lu/%lu)   0x%x  0x%x\n", x, y, pl,
                                   POINT_L( point, hfraq ), POINT_R( point, hfraq ), point, ratios[r] );
                         D_ASSERT( pl >= 0 );
                         D_ASSERT( pl < width-1 );
                         L = SOURCE_LOOKUP(srcB[pl]);
                         R = SOURCE_LOOKUP(srcB[pl+1]);

                         dpB = (((((R & X_F81F)-(L & X_F81F))*ratios[r] + ((L & X_F81F)<<SHIFT_L6)) & X_003E07C0) + 
                                ((((R & X_07E0)-(L & X_07E0))*ratios[r] + ((L & X_07E0)<<SHIFT_L6)) & X_0001F800))
#ifdef WORDS_BIGENDIAN
                              << SHIFT_L10;
#else
                              >> SHIFT_R6;
#endif

                         point += hfraq;
                         r++;


                         pl = POINT_L( point, hfraq );
                         HVX_DEBUG("%ld,%ld %lu  (%lu/%lu)   0x%x  0x%x\n", x, y, pl,
                                   POINT_L( point, hfraq ), POINT_R( point, hfraq ), point, ratios[r] );
                         D_ASSERT( pl >= 0 );
                         D_ASSERT( pl < width-1 );

                         L = SOURCE_LOOKUP(srcB[pl]);
                         R = SOURCE_LOOKUP(srcB[pl+1]);

                         dpB |= (((((R & X_F81F)-(L & X_F81F))*ratios[r] + ((L & X_F81F)<<SHIFT_L6)) & X_003E07C0) + 
                                 ((((R & X_07E0)-(L & X_07E0))*ratios[r] + ((L & X_07E0)<<SHIFT_L6)) & X_0001F800))
#ifdef WORDS_BIGENDIAN
                              >> SHIFT_R6;
#else
                              << SHIFT_L10;
#endif

                         point += hfraq;
                         r++;

                         /* Store */
                         lbB[x] = dpB;
                    }
               }

               lineT = nlT;
          }

          /*
           * Vertical interpolation
           */
          long X = LINE_TO_RATIO( line, vfraq );

          for (x=0; x<w2; x++) {
#if defined (COLOR_KEY) || defined (COLOR_KEY_PROTECT)
#ifdef HAS_ALPHA
               dt = ((((((lbB[x] & X_F81FF81F) - (lbT[x] & X_F81FF81F))*X) >> SHIFT_R5) + (lbT[x] & X_F81FF81F)) & X_F81FF81F) +
                    ((((((lbB[x]>>SHIFT_R5) & X_F81FF81F) - ((lbT[x]>>SHIFT_R5) & X_F81FF81F))*X) + (lbT[x] & X_07E007E0)) & X_07E007E0);
#else
               dt = ((((((lbB[x] & X_07E0F81F) - (lbT[x] & X_07E0F81F))*X) >> SHIFT_R5) + (lbT[x] & X_07E0F81F)) & X_07E0F81F) +
                    ((((((lbB[x]>>SHIFT_R5) & X_07C0F83F) - ((lbT[x]>>SHIFT_R5) & X_07C0F83F))*X) + (lbT[x] & X_F81F07E0)) & X_F81F07E0);
#endif

               /* Get two new pixels. */
               l_ = dt;
               h_ = dt >> 16;

#ifdef COLOR_KEY
               if (l_ != (COLOR_KEY)) {
                    if (h_ != (COLOR_KEY)) {
#ifdef COLOR_KEY_PROTECT
                         /* Write to destination with color key protection */
                         dst32[x] = (((h_ == KEY_PROTECT) ? KEY_REPLACE : h_) << 16) | ((l_ == KEY_PROTECT) ? KEY_REPLACE : l_);
#else
                         /* Write to destination without color key protection */
                         dst32[x] = dt;
#endif
                    }
                    else {
                         u16 *_dst16 = (u16*) &dst32[x];

#ifdef COLOR_KEY_PROTECT
                         /* Write to destination with color key protection */
                         *_dst16 = ((l_ == KEY_PROTECT) ? KEY_REPLACE : l_);
#else
                         /* Write to destination without color key protection */
                         *_dst16 = l_;
#endif
                    }
               }
               else if (h_ != (COLOR_KEY)) {
                    u16 *_dst16 = ((u16*) &dst32[x]) + 1;

#ifdef COLOR_KEY_PROTECT
                    /* Write to destination with color key protection */
                    *_dst16 = ((h_ == KEY_PROTECT) ? KEY_REPLACE : h_);
#else
                    /* Write to destination without color key protection */
                    *_dst16 = h_;
#endif
               }
#else
               /* Write to destination with color key protection */
               dst32[x] = (((h_ == KEY_PROTECT) ? KEY_REPLACE : h_) << 16) | ((l_ == KEY_PROTECT) ? KEY_REPLACE : l_);
#endif
#else
#ifdef HAS_ALPHA
               dst32[x] = ((((((lbB[x] & X_F81FF81F) - (lbT[x] & X_F81FF81F))*X) >> SHIFT_R5) + (lbT[x] & X_F81FF81F)) & X_F81FF81F) +
                          ((((((lbB[x]>>SHIFT_R5) & X_F81FF81F) - ((lbT[x]>>SHIFT_R5) & X_F81FF81F))*X) + (lbT[x] & X_07E007E0)) & X_07E007E0);
#else
               dst32[x] = ((((((lbB[x] & X_07E0F81F) - (lbT[x] & X_07E0F81F))*X) >> SHIFT_R5) + (lbT[x] & X_07E0F81F)) & X_07E0F81F) +
                          ((((((lbB[x]>>SHIFT_R5) & X_07C0F83F) - ((lbT[x]>>SHIFT_R5) & X_07C0F83F))*X) + (lbT[x] & X_F81F07E0)) & X_F81F07E0);
#endif
#endif
          }

          dst32 += dp4;
          line  += vfraq;
     }

     if (tail) {
          u32 dpT, dpB, L, R;

          u16 *dst16 = dst + cw * 2 - 2;

          /* Reset */
          line = LINE_0 + clip->y1 * vfraq;

          for (y=0; y<ch; y++) {
               long X = LINE_TO_RATIO( line, vfraq );

               const SOURCE_TYPE *srcT = src + spitch * LINE_T( line, vfraq );
               const SOURCE_TYPE *srcB = src + spitch * LINE_B( line, vfraq );

               /*
                * Horizontal interpolation
                */
               long pl = POINT_L( point, hfraq );
               HVX_DEBUG("t,%ld %lu  (%lu/%lu)   0x%x  0x%x\n", y, pl,
                         POINT_L( point, hfraq ), POINT_R( point, hfraq ), point, ratios[r] );
               D_ASSERT( pl >= 0 );
               D_ASSERT( pl < width-1 );
               L = SOURCE_LOOKUP(srcT[pl]);
               R = SOURCE_LOOKUP(srcT[pl+1]);

               dpT = (((((R & X_F81F)-(L & X_F81F))*ratios[r] + ((L & X_F81F)<<SHIFT_L6)) & X_003E07C0) + 
                      ((((R & X_07E0)-(L & X_07E0))*ratios[r] + ((L & X_07E0)<<SHIFT_L6)) & X_0001F800)) >> SHIFT_R6;

               L = SOURCE_LOOKUP(srcB[pl]);
               R = SOURCE_LOOKUP(srcB[pl+1]);

               dpB = (((((R & X_F81F)-(L & X_F81F))*ratios[r] + ((L & X_F81F)<<SHIFT_L6)) & X_003E07C0) + 
                      ((((R & X_07E0)-(L & X_07E0))*ratios[r] + ((L & X_07E0)<<SHIFT_L6)) & X_0001F800)) >> SHIFT_R6;

               /*
                * Vertical interpolation
                */
#if defined (COLOR_KEY) || defined (COLOR_KEY_PROTECT)
               l_ = ((((((dpB & X_F81F) - (dpT & X_F81F))*X) >> SHIFT_R5) + (dpT & X_F81F)) & X_F81F) +
                    ((((((dpB>>SHIFT_R5) & X_003F) - ((dpT>>SHIFT_R5) & X_003F))*X) + (dpT & X_07E0)) & X_07E0);

#ifdef COLOR_KEY
               if (l_ != (COLOR_KEY))
#endif
#ifdef COLOR_KEY_PROTECT
                    /* Write to destination with color key protection */
                    dst16[0] = (l_ == KEY_PROTECT) ? KEY_REPLACE : l_;
#else
                    /* Write to destination without color key protection */
                    dst16[0] = l_;
#endif
#else
               /* Write to destination without color key protection */
               dst16[0] = ((((((dpB & X_F81F) - (dpT & X_F81F))*X) >> SHIFT_R5) + (dpT & X_F81F)) & X_F81F) +
                          ((((((dpB>>SHIFT_R5) & X_003F) - ((dpT>>SHIFT_R5) & X_003F))*X) + (dpT & X_07E0)) & X_07E0);
#endif

               dst16 += dpitch / 2;
               line  += vfraq;
          }
     }
}


#undef SHIFT_L6
#undef SHIFT_L10

#undef X_003F

#undef X_003E07C0
#undef X_0001F800

#undef X_07E0F81F
#undef X_F81F07E0

#undef X_07C0F83F



#ifdef SOURCE_LOOKUP_AUTO
#undef SOURCE_LOOKUP_AUTO
#undef SOURCE_LOOKUP
#endif

#ifdef SOURCE_TYPE_AUTO
#undef SOURCE_TYPE_AUTO
#undef SOURCE_TYPE
#endif

