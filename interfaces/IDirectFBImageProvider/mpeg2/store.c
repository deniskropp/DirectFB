/* store.c, picture output routines                                         */

/* Copyright (C) 1996, MPEG Software Simulation Group. All Rights Reserved. */

/*
 * Disclaimer of Warranty
 *
 * These software programs are available to the user without any license fee or
 * royalty on an "as is" basis.  The MPEG Software Simulation Group disclaims
 * any and all warranties, whether express, implied, or statuary, including any
 * implied warranties or merchantability or of fitness for a particular
 * purpose.  In no event shall the copyright-holder be liable for any
 * incidental, punitive, or consequential damages of any kind whatsoever
 * arising from the use of these programs.
 *
 * This disclaimer of warranty extends to the user of these programs and user's
 * customers, employees, agents, transferees, successors, and assigns.
 *
 * The MPEG Software Simulation Group does not represent or warrant that the
 * programs furnished hereunder are free of infringement of any third-party
 * patents.
 *
 * Commercial implementations of MPEG-1 and MPEG-2 video, including shareware,
 * are subject to royalty fees to patent holders.  Many of these patents are
 * general enough such that they are unavoidable regardless of implementation
 * design.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include <direct/mem.h>

#include "global.h"

/* private prototypes */
static void store_argb (MPEG2_Decoder *dec, unsigned char *src[],
                        int offset, int incr, int height);

static void conv422to444 (MPEG2_Decoder *dec, unsigned char *src, unsigned char *dst);
static void conv420to422 (MPEG2_Decoder *dec, unsigned char *src, unsigned char *dst);

/* color space conversion coefficients
 * for YCbCr -> RGB mapping
 *
 * entries are {crv,cbu,cgu,cgv}
 *
 * crv=(255/224)*65536*(1-cr)/0.5
 * cbu=(255/224)*65536*(1-cb)/0.5
 * cgu=(255/224)*65536*(cb/cg)*(1-cb)/0.5
 * cgv=(255/224)*65536*(cr/cg)*(1-cr)/0.5
 *
 * where Y=cr*R+cg*G+cb*B (cr+cg+cb=1)
 */

/* ISO/IEC 13818-2 section 6.3.6 sequence_display_extension() */

static const int Inverse_Table_6_9[8][4] = {
     {117504, 138453, 13954, 34903}, /* no sequence_display_extension */
     {117504, 138453, 13954, 34903}, /* ITU-R Rec. 709 (1990) */
     {104597, 132201, 25675, 53279}, /* unspecified */
     {104597, 132201, 25675, 53279}, /* reserved */
     {104448, 132798, 24759, 53109}, /* FCC */
     {104597, 132201, 25675, 53279}, /* ITU-R Rec. 624-4 System B, G */
     {104597, 132201, 25675, 53279}, /* SMPTE 170M */
     {117579, 136230, 16907, 35559}  /* SMPTE 240M (1987) */
};


/*
 * store a picture as either one frame or two fields
 */
void MPEG2_Write_Frame(MPEG2_Decoder *dec, unsigned char *src[], int frame)
{
     store_argb (dec, src, 0, dec->Coded_Picture_Width, dec->vertical_size);
}

/*
 * store as ARGB
 */
static void store_argb(MPEG2_Decoder *dec,
unsigned char *src[],
int offset, int incr, int height)
{
     int i, j;
     int y, u, v, r, g, b;
     int crv, cbu, cgu, cgv;
     unsigned char *py, *pu, *pv;
     unsigned char *u422 = NULL, *v422 = NULL, *u444 = NULL, *v444 = NULL;

     if (dec->chroma_format==CHROMA444) {
          u444 = src[1];
          v444 = src[2];
     }
     else {
          if (dec->chroma_format==CHROMA420) {
               if (!(u422 = (unsigned char *)D_MALLOC((dec->Coded_Picture_Width>>1)
                                                      *dec->Coded_Picture_Height)))
                    MPEG2_Error(dec, "malloc failed");
               if (!(v422 = (unsigned char *)D_MALLOC((dec->Coded_Picture_Width>>1)
                                                      *dec->Coded_Picture_Height)))
                    MPEG2_Error(dec, "malloc failed");
          }

          if (!(u444 = (unsigned char *)D_MALLOC(dec->Coded_Picture_Width
                                                 *dec->Coded_Picture_Height)))
               MPEG2_Error(dec, "malloc failed");

          if (!(v444 = (unsigned char *)D_MALLOC(dec->Coded_Picture_Width
                                                 *dec->Coded_Picture_Height)))
               MPEG2_Error(dec, "malloc failed");

          if (dec->chroma_format==CHROMA420) {
               conv420to422(dec, src[1],u422);
               conv420to422(dec, src[2],v422);
               conv422to444(dec, u422,u444);
               conv422to444(dec, v422,v444);
          }
          else {
               conv422to444(dec, src[1],u444);
               conv422to444(dec, src[2],v444);
          }
     }

     /* matrix coefficients */
     crv = Inverse_Table_6_9[dec->matrix_coefficients][0];
     cbu = Inverse_Table_6_9[dec->matrix_coefficients][1];
     cgu = Inverse_Table_6_9[dec->matrix_coefficients][2];
     cgv = Inverse_Table_6_9[dec->matrix_coefficients][3];

     for (i=0; i<height; i++) {
          py = src[0] + offset + incr*i;
          pu = u444 + offset + incr*i;
          pv = v444 + offset + incr*i;

          for (j=0; j<dec->horizontal_size; j++) {
               u = *pu++ - 128;
               v = *pv++ - 128;
               y = 76309 * (*py++ - 16); /* (255/219)*65536 */
               r = dec->Clip[(y + crv*v + 32768)>>16];
               g = dec->Clip[(y - cgu*u - cgv*v + 32768)>>16];
               b = dec->Clip[(y + cbu*u + 32786)>>16];

               dec->mpeg2_write (j, i,
                                 0xff000000 | (r << 16) | (g << 8) | b,
                                 dec->mpeg2_write_ctx);
          }
     }

     if (dec->chroma_format!=CHROMA444) {
          if (u422)
               D_FREE( u422 );

          if (v422)
               D_FREE( v422 );

          if (u444)
               D_FREE( u444 );

          if (v444)
               D_FREE( v444 );
     }
}

/* horizontal 1:2 interpolation filter */
static void conv422to444(MPEG2_Decoder *dec, unsigned char *src, unsigned char *dst)
{
     int i, i2, w, j, im3, im2, im1, ip1, ip2, ip3;

     w = dec->Coded_Picture_Width>>1;

     if (dec->MPEG2_Flag) {
          for (j=0; j<dec->Coded_Picture_Height; j++) {
               for (i=0; i<w; i++) {
                    i2 = i<<1;
                    im2 = (i<2) ? 0 : i-2;
                    im1 = (i<1) ? 0 : i-1;
                    ip1 = (i<w-1) ? i+1 : w-1;
                    ip2 = (i<w-2) ? i+2 : w-1;
                    ip3 = (i<w-3) ? i+3 : w-1;

                    /* FIR filter coefficients (*256): 21 0 -52 0 159 256 159 0 -52 0 21 */
                    /* even samples (0 0 256 0 0) */
                    dst[i2] = src[i];

                    /* odd samples (21 -52 159 159 -52 21) */
                    dst[i2+1] = dec->Clip[(int)(21*(src[im2]+src[ip3])
                                                 -52*(src[im1]+src[ip2])
                                                 +159*(src[i]+src[ip1])+128)>>8];
               }
               src+= w;
               dst+= dec->Coded_Picture_Width;
          }
     }
     else {
          for (j=0; j<dec->Coded_Picture_Height; j++) {
               for (i=0; i<w; i++) {

                    i2 = i<<1;
                    im3 = (i<3) ? 0 : i-3;
                    im2 = (i<2) ? 0 : i-2;
                    im1 = (i<1) ? 0 : i-1;
                    ip1 = (i<w-1) ? i+1 : w-1;
                    ip2 = (i<w-2) ? i+2 : w-1;
                    ip3 = (i<w-3) ? i+3 : w-1;

                    /* FIR filter coefficients (*256): 5 -21 70 228 -37 11 */
                    dst[i2] =   dec->Clip[(int)(  5*src[im3]
                                                   -21*src[im2]
                                                   +70*src[im1]
                                                   +228*src[i]
                                                   -37*src[ip1]
                                                   +11*src[ip2]+128)>>8];

                    dst[i2+1] = dec->Clip[(int)(  5*src[ip3]
                                                   -21*src[ip2]
                                                   +70*src[ip1]
                                                   +228*src[i]
                                                   -37*src[im1]
                                                   +11*src[im2]+128)>>8];
               }
               src+= w;
               dst+= dec->Coded_Picture_Width;
          }
     }
}

/* vertical 1:2 interpolation filter */
static void conv420to422(MPEG2_Decoder *dec, unsigned char *src, unsigned char *dst)
{
     int w, h, i, j, j2;
     int jm6, jm5, jm4, jm3, jm2, jm1, jp1, jp2, jp3, jp4, jp5, jp6, jp7;

     w = dec->Coded_Picture_Width>>1;
     h = dec->Coded_Picture_Height>>1;

     if (dec->progressive_frame) {
          /* intra frame */
          for (i=0; i<w; i++) {
               for (j=0; j<h; j++) {
                    j2 = j<<1;
                    jm3 = (j<3) ? 0 : j-3;
                    jm2 = (j<2) ? 0 : j-2;
                    jm1 = (j<1) ? 0 : j-1;
                    jp1 = (j<h-1) ? j+1 : h-1;
                    jp2 = (j<h-2) ? j+2 : h-1;
                    jp3 = (j<h-3) ? j+3 : h-1;

                    /* FIR filter coefficients (*256): 5 -21 70 228 -37 11 */
                    /* New FIR filter coefficients (*256): 3 -16 67 227 -32 7 */
                    dst[w*j2] =     dec->Clip[(int)(  3*src[w*jm3]
                                                       -16*src[w*jm2]
                                                       +67*src[w*jm1]
                                                       +227*src[w*j]
                                                       -32*src[w*jp1]
                                                       +7*src[w*jp2]+128)>>8];

                    dst[w*(j2+1)] = dec->Clip[(int)(  3*src[w*jp3]
                                                       -16*src[w*jp2]
                                                       +67*src[w*jp1]
                                                       +227*src[w*j]
                                                       -32*src[w*jm1]
                                                       +7*src[w*jm2]+128)>>8];
               }
               src++;
               dst++;
          }
     }
     else {
          /* intra field */
          for (i=0; i<w; i++) {
               for (j=0; j<h; j+=2) {
                    j2 = j<<1;

                    /* top field */
                    jm6 = (j<6) ? 0 : j-6;
                    jm4 = (j<4) ? 0 : j-4;
                    jm2 = (j<2) ? 0 : j-2;
                    jp2 = (j<h-2) ? j+2 : h-2;
                    jp4 = (j<h-4) ? j+4 : h-2;
                    jp6 = (j<h-6) ? j+6 : h-2;

                    /* Polyphase FIR filter coefficients (*256): 2 -10 35 242 -18 5 */
                    /* New polyphase FIR filter coefficients (*256): 1 -7 30 248 -21 5 */
                    dst[w*j2] = dec->Clip[(int)(  1*src[w*jm6]
                                                   -7*src[w*jm4]
                                                   +30*src[w*jm2]
                                                   +248*src[w*j]
                                                   -21*src[w*jp2]
                                                   +5*src[w*jp4]+128)>>8];

                    /* Polyphase FIR filter coefficients (*256): 11 -38 192 113 -30 8 */
                    /* New polyphase FIR filter coefficients (*256):7 -35 194 110 -24 4 */
                    dst[w*(j2+2)] = dec->Clip[(int)( 7*src[w*jm4]
                                                      -35*src[w*jm2]
                                                      +194*src[w*j]
                                                      +110*src[w*jp2]
                                                      -24*src[w*jp4]
                                                      +4*src[w*jp6]+128)>>8];

                    /* bottom field */
                    jm5 = (j<5) ? 1 : j-5;
                    jm3 = (j<3) ? 1 : j-3;
                    jm1 = (j<1) ? 1 : j-1;
                    jp1 = (j<h-1) ? j+1 : h-1;
                    jp3 = (j<h-3) ? j+3 : h-1;
                    jp5 = (j<h-5) ? j+5 : h-1;
                    jp7 = (j<h-7) ? j+7 : h-1;

                    /* Polyphase FIR filter coefficients (*256): 11 -38 192 113 -30 8 */
                    /* New polyphase FIR filter coefficients (*256):7 -35 194 110 -24 4 */
                    dst[w*(j2+1)] = dec->Clip[(int)( 7*src[w*jp5]
                                                      -35*src[w*jp3]
                                                      +194*src[w*jp1]
                                                      +110*src[w*jm1]
                                                      -24*src[w*jm3]
                                                      +4*src[w*jm5]+128)>>8];

                    dst[w*(j2+3)] = dec->Clip[(int)(  1*src[w*jp7]
                                                       -7*src[w*jp5]
                                                       +30*src[w*jp3]
                                                       +248*src[w*jp1]
                                                       -21*src[w*jm1]
                                                       +5*src[w*jm3]+128)>>8];
               }
               src++;
               dst++;
          }
     }
}
