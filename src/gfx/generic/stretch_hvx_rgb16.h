static void STRETCH_HVX_RGB16( void       *dst,
                               int         dpitch,
                               const void *src,
                               int         spitch,
                               int         width,
                               int         height,
                               int         dst_width,
                               int         dst_height,
                               DFBRegion  *clip )
{
     int        x, y, r = 0;
     int        head    = ((((unsigned long) dst) & 2) >> 1) ^ (clip->x1 & 1);
     int        cw      = clip->x2 - clip->x1 + 1;
     int        ch      = clip->y2 - clip->y1 + 1;
     int        tail    = (cw - head) & 1;
     int        w2      = (cw - head) / 2;
     u32        hfraq   = ((u32)width  << 18) / (u32)dst_width;
     u32        vfraq   = ((u32)height << 18) / (u32)dst_height;
     int        dp4     = dpitch / 4;
     u32        point0  = POINT_0 + clip->x1 * hfraq;
     u32        point   = point0;
     u32        line    = LINE_0 + clip->y1 * vfraq;
     u32       *dst32;
     u32        ratios[cw];

     u32        _lbT[w2+8];
     u32        _lbB[w2+8];

     u32       *lbX;
     u32       *lbT = (u32*)((((u32)(&_lbT[0])) + 31) & ~31);
     u32       *lbB = (u32*)((((u32)(&_lbB[0])) + 31) & ~31);

     int        lineT = -2000;

//     direct_log_printf( NULL, "     %p [%d] -> %p [%d]\n", src, spitch, dst, dpitch );

     for (x=0; x<cw; x++) {
          ratios[x] = POINT_TO_RATIO( point, hfraq );
         // printf("(%3d) %6x (%6x) <- %3d %3d\n",x,ratios[x], point, POINT_L( point, hfraq ), POINT_R( point, hfraq ));

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

               const u16 *srcT = src + spitch * LINE_T( line, vfraq );
               const u16 *srcB = src + spitch * LINE_B( line, vfraq );


               /*
                * Horizontal interpolation
                */

               L = srcT[POINT_L( point, hfraq )];
               R = srcT[POINT_R( point, hfraq )];

               dpT = (((((R & 0xf81f)-(L & 0xf81f))*ratios[r] + ((L & 0xf81f)<<6)) & 0x003e07c0) + 
                      ((((R & 0x07e0)-(L & 0x07e0))*ratios[r] + ((L & 0x07e0)<<6)) & 0x0001f800)) >> 6;

               L = srcB[POINT_L( point, hfraq )];
               R = srcB[POINT_R( point, hfraq )];

               dpB = (((((R & 0xf81f)-(L & 0xf81f))*ratios[r] + ((L & 0xf81f)<<6)) & 0x003e07c0) + 
                      ((((R & 0x07e0)-(L & 0x07e0))*ratios[r] + ((L & 0x07e0)<<6)) & 0x0001f800)) >> 6;

               /*
                * Vertical interpolation
                */

               dst16[0] = ((((((dpB & 0xf81f) - (dpT & 0xf81f))*X) >> 5) + (dpT & 0xf81f)) & 0xf81f) +
                          ((((((dpB>>5) & 0x3f) - ((dpT>>5) & 0x3f))*X) + (dpT & 0x07e0)) & 0x07e0);

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
               const u16 *srcT = src + spitch * nlT;
               const u16 *srcB = src + spitch * (nlT + 1);
               int        diff = nlT - lineT;

               if (diff > 1) {
                    /*
                     * Two output pixels per step.
                     */
                    for (x=0, r=head, point=point0; x<w2; x++) {
                         /*
                          * Horizontal interpolation
                          */
                         L = srcT[POINT_L( point, hfraq )];
                         R = srcT[POINT_R( point, hfraq )];

                         dpT = (((((R & 0xf81f)-(L & 0xf81f))*ratios[r] + ((L & 0xf81f)<<6)) & 0x003e07c0) + 
                                ((((R & 0x07e0)-(L & 0x07e0))*ratios[r] + ((L & 0x07e0)<<6)) & 0x0001f800)) >> 6;

                         L = srcB[POINT_L( point, hfraq )];
                         R = srcB[POINT_R( point, hfraq )];

                         dpB = (((((R & 0xf81f)-(L & 0xf81f))*ratios[r] + ((L & 0xf81f)<<6)) & 0x003e07c0) + 
                                ((((R & 0x07e0)-(L & 0x07e0))*ratios[r] + ((L & 0x07e0)<<6)) & 0x0001f800)) >> 6;

                         point += hfraq;
                         r++;


                         L = srcT[POINT_L( point, hfraq )];
                         R = srcT[POINT_R( point, hfraq )];

                         dpT |= (((((R & 0xf81f)-(L & 0xf81f))*ratios[r] + ((L & 0xf81f)<<6)) & 0x003e07c0) + 
                                 ((((R & 0x07e0)-(L & 0x07e0))*ratios[r] + ((L & 0x07e0)<<6)) & 0x0001f800)) << 10;

                         L = srcB[POINT_L( point, hfraq )];
                         R = srcB[POINT_R( point, hfraq )];

                         dpB |= (((((R & 0xf81f)-(L & 0xf81f))*ratios[r] + ((L & 0xf81f)<<6)) & 0x003e07c0) + 
                                 ((((R & 0x07e0)-(L & 0x07e0))*ratios[r] + ((L & 0x07e0)<<6)) & 0x0001f800)) << 10;

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
                         L = srcB[POINT_L( point, hfraq )];
                         R = srcB[POINT_R( point, hfraq )];

                         dpB = (((((R & 0xf81f)-(L & 0xf81f))*ratios[r] + ((L & 0xf81f)<<6)) & 0x003e07c0) + 
                                ((((R & 0x07e0)-(L & 0x07e0))*ratios[r] + ((L & 0x07e0)<<6)) & 0x0001f800)) >> 6;

                         point += hfraq;
                         r++;


                         L = srcB[POINT_L( point, hfraq )];
                         R = srcB[POINT_R( point, hfraq )];

                         dpB |= (((((R & 0xf81f)-(L & 0xf81f))*ratios[r] + ((L & 0xf81f)<<6)) & 0x003e07c0) + 
                                 ((((R & 0x07e0)-(L & 0x07e0))*ratios[r] + ((L & 0x07e0)<<6)) & 0x0001f800)) << 10;

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
               dst32[x] = ((((((lbB[x] & 0x07e0f81f) - (lbT[x] & 0x07e0f81f))*X) >> 5) + (lbT[x] & 0x07e0f81f)) & 0x07e0f81f) +
                          ((((((lbB[x]>>5) & 0x07c0f83f) - ((lbT[x]>>5) & 0x07c0f83f))*X) + (lbT[x] & 0xf81f07e0)) & 0xf81f07e0);
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

               const u16 *srcT = src + spitch * LINE_T( line, vfraq );
               const u16 *srcB = src + spitch * LINE_B( line, vfraq );


               /*
                * Horizontal interpolation
                */

               L = srcT[POINT_L( point, hfraq )];
               R = srcT[POINT_R( point, hfraq )];

               dpT = (((((R & 0xf81f)-(L & 0xf81f))*ratios[r] + ((L & 0xf81f)<<6)) & 0x003e07c0) + 
                      ((((R & 0x07e0)-(L & 0x07e0))*ratios[r] + ((L & 0x07e0)<<6)) & 0x0001f800)) >> 6;

               L = srcB[POINT_L( point, hfraq )];
               R = srcB[POINT_R( point, hfraq )];

               dpB = (((((R & 0xf81f)-(L & 0xf81f))*ratios[r] + ((L & 0xf81f)<<6)) & 0x003e07c0) + 
                      ((((R & 0x07e0)-(L & 0x07e0))*ratios[r] + ((L & 0x07e0)<<6)) & 0x0001f800)) >> 6;

               /*
                * Vertical interpolation
                */

               dst16[0] = ((((((dpB & 0xf81f) - (dpT & 0xf81f))*X) >> 5) + (dpT & 0xf81f)) & 0xf81f) +
                          ((((((dpB>>5) & 0x3f) - ((dpT>>5) & 0x3f))*X) + (dpT & 0x07e0)) & 0x07e0);

               dst16 += dpitch / 2;
               line  += vfraq;
          }
     }
}

