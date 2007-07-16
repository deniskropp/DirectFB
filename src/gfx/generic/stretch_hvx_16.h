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
     int  x, y, r = 0;
     int  head    = ((((unsigned long) dst) & 2) >> 1) ^ (clip->x1 & 1);
     int  cw      = clip->x2 - clip->x1 + 1;
     int  ch      = clip->y2 - clip->y1 + 1;
     int  tail    = (cw - head) & 1;
     int  w2      = (cw - head) / 2;
     u32  hfraq   = ((u32)width  << 18) / (u32)dst_width;
     u32  vfraq   = ((u32)height << 18) / (u32)dst_height;
     int  dp4     = dpitch / 4;
     u32  point0  = POINT_0 + clip->x1 * hfraq;
     u32  point   = point0;
     u32  line    = LINE_0 + clip->y1 * vfraq;
     u32 *dst32;
     u32  ratios[cw];

     u32  _lbT[w2+8];
     u32  _lbB[w2+8];

     u32 *lbX;
     u32 *lbT = (u32*)((((ulong)(&_lbT[0])) + 31) & ~31);
     u32 *lbB = (u32*)((((ulong)(&_lbB[0])) + 31) & ~31);

     int  lineT = -2000;

     for (x=0; x<cw; x++) {
          ratios[x] = POINT_TO_RATIO( point, hfraq );

          point += hfraq;
     }

     dst += clip->x1 * 2 + clip->y1 * dpitch;

     dst32 = dst;

     if (head) {
          u32 dpT, dpB, L, R;

          u16 *dst16 = dst;

          point = point0;

          for (y=0; y<ch; y++) {
               u32 X = LINE_TO_RATIO( line, vfraq );

               const SOURCE_TYPE *srcT = src + spitch * LINE_T( line, vfraq );
               const SOURCE_TYPE *srcB = src + spitch * LINE_B( line, vfraq );

               /*
                * Horizontal interpolation
                */
               L = SOURCE_LOOKUP(srcT[POINT_L( point, hfraq )]);
               R = SOURCE_LOOKUP(srcT[POINT_R( point, hfraq )]);

               dpT = (((((R & X_F81F)-(L & X_F81F))*ratios[r] + ((L & X_F81F)<<SHIFT_L6)) & X_003E07C0) + 
                      ((((R & X_07E0)-(L & X_07E0))*ratios[r] + ((L & X_07E0)<<SHIFT_L6)) & X_0001F800)) >> SHIFT_R6;

               L = SOURCE_LOOKUP(srcB[POINT_L( point, hfraq )]);
               R = SOURCE_LOOKUP(srcB[POINT_R( point, hfraq )]);

               dpB = (((((R & X_F81F)-(L & X_F81F))*ratios[r] + ((L & X_F81F)<<SHIFT_L6)) & X_003E07C0) + 
                      ((((R & X_07E0)-(L & X_07E0))*ratios[r] + ((L & X_07E0)<<SHIFT_L6)) & X_0001F800)) >> SHIFT_R6;

               /*
                * Vertical interpolation
                */
               dst16[0] = ((((((dpB & X_F81F) - (dpT & X_F81F))*X) >> SHIFT_R5) + (dpT & X_F81F)) & X_F81F) +
                          ((((((dpB>>SHIFT_R5) & X_003F) - ((dpT>>SHIFT_R5) & X_003F))*X) + (dpT & X_07E0)) & X_07E0);

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
          int nlT = LINE_T( line, vfraq );

          /*
           * Fill line buffer(s) ?
           */
          if (nlT != lineT) {
               u32 L, R, dpT, dpB;
               const SOURCE_TYPE *srcT = src + spitch * nlT;
               const SOURCE_TYPE *srcB = src + spitch * (nlT + 1);
               int                diff = nlT - lineT;

               if (diff > 1) {
                    /*
                     * Two output pixels per step.
                     */
                    for (x=0, r=head, point=point0; x<w2; x++) {
                         /*
                          * Horizontal interpolation
                          */
                         L = SOURCE_LOOKUP(srcT[POINT_L( point, hfraq )]);
                         R = SOURCE_LOOKUP(srcT[POINT_R( point, hfraq )]);

                         dpT = (((((R & X_F81F)-(L & X_F81F))*ratios[r] + ((L & X_F81F)<<SHIFT_L6)) & X_003E07C0) + 
                                ((((R & X_07E0)-(L & X_07E0))*ratios[r] + ((L & X_07E0)<<SHIFT_L6)) & X_0001F800))
#ifdef WORDS_BIGENDIAN
                              << SHIFT_L10;
#else
                              >> SHIFT_R6;
#endif

                         L = SOURCE_LOOKUP(srcB[POINT_L( point, hfraq )]);
                         R = SOURCE_LOOKUP(srcB[POINT_R( point, hfraq )]);

                         dpB = (((((R & X_F81F)-(L & X_F81F))*ratios[r] + ((L & X_F81F)<<SHIFT_L6)) & X_003E07C0) + 
                                ((((R & X_07E0)-(L & X_07E0))*ratios[r] + ((L & X_07E0)<<SHIFT_L6)) & X_0001F800))
#ifdef WORDS_BIGENDIAN
                              << SHIFT_L10;
#else
                              >> SHIFT_R6;
#endif

                         point += hfraq;
                         r++;


                         L = SOURCE_LOOKUP(srcT[POINT_L( point, hfraq )]);
                         R = SOURCE_LOOKUP(srcT[POINT_R( point, hfraq )]);

                         dpT |= (((((R & X_F81F)-(L & X_F81F))*ratios[r] + ((L & X_F81F)<<SHIFT_L6)) & X_003E07C0) + 
                                 ((((R & X_07E0)-(L & X_07E0))*ratios[r] + ((L & X_07E0)<<SHIFT_L6)) & X_0001F800))
#ifdef WORDS_BIGENDIAN
                              >> SHIFT_R6;
#else
                              << SHIFT_L10;
#endif

                         L = SOURCE_LOOKUP(srcB[POINT_L( point, hfraq )]);
                         R = SOURCE_LOOKUP(srcB[POINT_R( point, hfraq )]);

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
                         L = SOURCE_LOOKUP(srcB[POINT_L( point, hfraq )]);
                         R = SOURCE_LOOKUP(srcB[POINT_R( point, hfraq )]);

                         dpB = (((((R & X_F81F)-(L & X_F81F))*ratios[r] + ((L & X_F81F)<<SHIFT_L6)) & X_003E07C0) + 
                                ((((R & X_07E0)-(L & X_07E0))*ratios[r] + ((L & X_07E0)<<SHIFT_L6)) & X_0001F800))
#ifdef WORDS_BIGENDIAN
                              << SHIFT_L10;
#else
                              >> SHIFT_R6;
#endif

                         point += hfraq;
                         r++;


                         L = SOURCE_LOOKUP(srcB[POINT_L( point, hfraq )]);
                         R = SOURCE_LOOKUP(srcB[POINT_R( point, hfraq )]);

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
          u32 X = LINE_TO_RATIO( line, vfraq );

          for (x=0; x<w2; x++) {
#ifdef HAS_ALPHA
               dst32[x] = ((((((lbB[x] & X_F81FF81F) - (lbT[x] & X_F81FF81F))*X) >> SHIFT_R5) + (lbT[x] & X_F81FF81F)) & X_F81FF81F) +
                          ((((((lbB[x]>>SHIFT_R5) & X_F81FF81F) - ((lbT[x]>>SHIFT_R5) & X_F81FF81F))*X) + (lbT[x] & X_07E007E0)) & X_07E007E0);
#else
               dst32[x] = ((((((lbB[x] & X_07E0F81F) - (lbT[x] & X_07E0F81F))*X) >> SHIFT_R5) + (lbT[x] & X_07E0F81F)) & X_07E0F81F) +
                          ((((((lbB[x]>>SHIFT_R5) & X_07C0F83F) - ((lbT[x]>>SHIFT_R5) & X_07C0F83F))*X) + (lbT[x] & X_F81F07E0)) & X_F81F07E0);
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
               u32 X = LINE_TO_RATIO( line, vfraq );

               const SOURCE_TYPE *srcT = src + spitch * LINE_T( line, vfraq );
               const SOURCE_TYPE *srcB = src + spitch * LINE_B( line, vfraq );

               /*
                * Horizontal interpolation
                */
               L = SOURCE_LOOKUP(srcT[POINT_L( point, hfraq )]);
               R = SOURCE_LOOKUP(srcT[POINT_R( point, hfraq )]);

               dpT = (((((R & X_F81F)-(L & X_F81F))*ratios[r] + ((L & X_F81F)<<SHIFT_L6)) & X_003E07C0) + 
                      ((((R & X_07E0)-(L & X_07E0))*ratios[r] + ((L & X_07E0)<<SHIFT_L6)) & X_0001F800)) >> SHIFT_R6;

               L = SOURCE_LOOKUP(srcB[POINT_L( point, hfraq )]);
               R = SOURCE_LOOKUP(srcB[POINT_R( point, hfraq )]);

               dpB = (((((R & X_F81F)-(L & X_F81F))*ratios[r] + ((L & X_F81F)<<SHIFT_L6)) & X_003E07C0) + 
                      ((((R & X_07E0)-(L & X_07E0))*ratios[r] + ((L & X_07E0)<<SHIFT_L6)) & X_0001F800)) >> SHIFT_R6;

               /*
                * Vertical interpolation
                */
               dst16[0] = ((((((dpB & X_F81F) - (dpT & X_F81F))*X) >> SHIFT_R5) + (dpT & X_F81F)) & X_F81F) +
                          ((((((dpB>>SHIFT_R5) & X_003F) - ((dpT>>SHIFT_R5) & X_003F))*X) + (dpT & X_07E0)) & X_07E0);

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

