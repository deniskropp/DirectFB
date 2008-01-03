/* getblk.c, DCT block decoding                                             */

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

#include "global.h"


/* defined in getvlc.h */
typedef struct {
     char run, level, len;
} DCTtab;

extern const DCTtab DCTtabfirst[],DCTtabnext[],DCTtab0[],DCTtab1[];
extern const DCTtab DCTtab2[],DCTtab3[],DCTtab4[],DCTtab5[],DCTtab6[];
extern const DCTtab DCTtab0a[],DCTtab1a[];


/* decode one intra coded MPEG-1 block */

void
MPEG2_Decode_MPEG1_Intra_Block(MPEG2_Decoder *dec, int comp, int dc_dct_pred[])
{
     int val, i, j, sign;
     unsigned int code;
     const DCTtab *tab;
     short *bp;

     bp = dec->block[comp];

     /* ISO/IEC 11172-2 section 2.4.3.7: Block layer. */
     /* decode DC coefficients */
     if (comp<4)
          bp[0] = (dc_dct_pred[0]+=MPEG2_Get_Luma_DC_dct_diff(dec)) << 3;
     else if (comp==4)
          bp[0] = (dc_dct_pred[1]+=MPEG2_Get_Chroma_DC_dct_diff(dec)) << 3;
     else
          bp[0] = (dc_dct_pred[2]+=MPEG2_Get_Chroma_DC_dct_diff(dec)) << 3;

     if (dec->Fault_Flag)
          return;

     /* D-pictures do not contain AC coefficients */
     if (dec->picture_coding_type == D_TYPE)
          return;

     /* decode AC coefficients */
     for (i=1; ; i++) {
          code = MPEG2_Show_Bits(dec, 16);
          if (code>=16384)
               tab = &DCTtabnext[(code>>12)-4];
          else if (code>=1024)
               tab = &DCTtab0[(code>>8)-4];
          else if (code>=512)
               tab = &DCTtab1[(code>>6)-8];
          else if (code>=256)
               tab = &DCTtab2[(code>>4)-16];
          else if (code>=128)
               tab = &DCTtab3[(code>>3)-16];
          else if (code>=64)
               tab = &DCTtab4[(code>>2)-16];
          else if (code>=32)
               tab = &DCTtab5[(code>>1)-16];
          else if (code>=16)
               tab = &DCTtab6[code-16];
          else {
               if (!MPEG2_Quiet_Flag)
                    printf("invalid Huffman code in MPEG2_Decode_MPEG1_Intra_Block()\n");
               dec->Fault_Flag = 1;
               return;
          }

          MPEG2_Flush_Buffer(dec, tab->len);

          if (tab->run==64) /* end_of_block */
               return;

          if (tab->run==65) { /* escape */
               i+= MPEG2_Get_Bits(dec, 6);

               val = MPEG2_Get_Bits(dec, 8);
               if (val==0)
                    val = MPEG2_Get_Bits(dec, 8);
               else if (val==128)
                    val = MPEG2_Get_Bits(dec, 8) - 256;
               else if (val>128)
                    val -= 256;

               if ((sign = (val<0)))
                    val = -val;
          }
          else {
               i+= tab->run;
               val = tab->level;
               sign = MPEG2_Get_Bits(dec, 1);
          }

          if (i>=64) {
               if (!MPEG2_Quiet_Flag)
                    fprintf(stderr,"DCT coeff index (i) out of bounds (intra)\n");
               dec->Fault_Flag = 1;
               return;
          }

          j = MPEG2_scan[ZIG_ZAG][i];
          val = (val*dec->quantizer_scale*dec->intra_quantizer_matrix[j]) >> 3;

          /* mismatch control ('oddification') */
          if (val!=0) /* should always be true, but it's not guaranteed */
               val = (val-1) | 1; /* equivalent to: if ((val&1)==0) val = val - 1; */

          /* saturation */
          if (!sign)
               bp[j] = (val>2047) ?  2047 :  val; /* positive */
          else
               bp[j] = (val>2048) ? -2048 : -val; /* negative */
     }
}


/* decode one non-intra coded MPEG-1 block */

void
MPEG2_Decode_MPEG1_Non_Intra_Block(MPEG2_Decoder *dec, int comp)
{
     int val, i, j, sign;
     unsigned int code;
     const DCTtab *tab;
     short *bp;

     bp = dec->block[comp];

     /* decode AC coefficients */
     for (i=0; ; i++) {
          code = MPEG2_Show_Bits(dec, 16);
          if (code>=16384) {
               if (i==0)
                    tab = &DCTtabfirst[(code>>12)-4];
               else
                    tab = &DCTtabnext[(code>>12)-4];
          }
          else if (code>=1024)
               tab = &DCTtab0[(code>>8)-4];
          else if (code>=512)
               tab = &DCTtab1[(code>>6)-8];
          else if (code>=256)
               tab = &DCTtab2[(code>>4)-16];
          else if (code>=128)
               tab = &DCTtab3[(code>>3)-16];
          else if (code>=64)
               tab = &DCTtab4[(code>>2)-16];
          else if (code>=32)
               tab = &DCTtab5[(code>>1)-16];
          else if (code>=16)
               tab = &DCTtab6[code-16];
          else {
               if (!MPEG2_Quiet_Flag)
                    printf("invalid Huffman code in MPEG2_Decode_MPEG1_Non_Intra_Block()\n");
               dec->Fault_Flag = 1;
               return;
          }

          MPEG2_Flush_Buffer(dec, tab->len);

          if (tab->run==64) /* end_of_block */
               return;

          if (tab->run==65) { /* escape */
               i+= MPEG2_Get_Bits(dec, 6);

               val = MPEG2_Get_Bits(dec, 8);
               if (val==0)
                    val = MPEG2_Get_Bits(dec, 8);
               else if (val==128)
                    val = MPEG2_Get_Bits(dec, 8) - 256;
               else if (val>128)
                    val -= 256;

               if ((sign = (val<0)))
                    val = -val;
          }
          else {
               i+= tab->run;
               val = tab->level;
               sign = MPEG2_Get_Bits(dec, 1);
          }

          if (i>=64) {
               if (!MPEG2_Quiet_Flag)
                    fprintf(stderr,"DCT coeff index (i) out of bounds (inter)\n");
               dec->Fault_Flag = 1;
               return;
          }

          j = MPEG2_scan[ZIG_ZAG][i];
          val = (((val<<1)+1)*dec->quantizer_scale*dec->non_intra_quantizer_matrix[j]) >> 4;

          /* mismatch control ('oddification') */
          if (val!=0) /* should always be true, but it's not guaranteed */
               val = (val-1) | 1; /* equivalent to: if ((val&1)==0) val = val - 1; */

          /* saturation */
          if (!sign)
               bp[j] = (val>2047) ?  2047 :  val; /* positive */
          else
               bp[j] = (val>2048) ? -2048 : -val; /* negative */
     }
}


/* decode one intra coded MPEG-2 block */

void
MPEG2_Decode_MPEG2_Intra_Block(MPEG2_Decoder *dec, int comp, int dc_dct_pred[])
{
     int val, i, j, sign, nc, cc, run;
     unsigned int code;
     const DCTtab *tab;
     short *bp;
     int *qmat;

     bp = dec->block[comp];

     cc = (comp<4) ? 0 : (comp&1)+1;

     qmat = (comp<4 || dec->chroma_format==CHROMA420)
            ? dec->intra_quantizer_matrix
            : dec->chroma_intra_quantizer_matrix;

     /* ISO/IEC 13818-2 section 7.2.1: decode DC coefficients */
     if (cc==0)
          val = (dc_dct_pred[0]+= MPEG2_Get_Luma_DC_dct_diff(dec));
     else if (cc==1)
          val = (dc_dct_pred[1]+= MPEG2_Get_Chroma_DC_dct_diff(dec));
     else
          val = (dc_dct_pred[2]+= MPEG2_Get_Chroma_DC_dct_diff(dec));

     if (dec->Fault_Flag)
          return;

     bp[0] = val << (3-dec->intra_dc_precision);

     nc=0;

     /* decode AC coefficients */
     for (i=1; ; i++) {
          code = MPEG2_Show_Bits(dec, 16);
          if (code>=16384 && !dec->intra_vlc_format)
               tab = &DCTtabnext[(code>>12)-4];
          else if (code>=1024) {
               if (dec->intra_vlc_format)
                    tab = &DCTtab0a[(code>>8)-4];
               else
                    tab = &DCTtab0[(code>>8)-4];
          }
          else if (code>=512) {
               if (dec->intra_vlc_format)
                    tab = &DCTtab1a[(code>>6)-8];
               else
                    tab = &DCTtab1[(code>>6)-8];
          }
          else if (code>=256)
               tab = &DCTtab2[(code>>4)-16];
          else if (code>=128)
               tab = &DCTtab3[(code>>3)-16];
          else if (code>=64)
               tab = &DCTtab4[(code>>2)-16];
          else if (code>=32)
               tab = &DCTtab5[(code>>1)-16];
          else if (code>=16)
               tab = &DCTtab6[code-16];
          else {
               if (!MPEG2_Quiet_Flag)
                    printf("invalid Huffman code in MPEG2_Decode_MPEG2_Intra_Block()\n");
               dec->Fault_Flag = 1;
               return;
          }

          MPEG2_Flush_Buffer(dec, tab->len);

          if (tab->run==64) { /* end_of_block */
               return;
          }

          if (tab->run==65) { /* escape */
               i+= run = MPEG2_Get_Bits(dec, 6);

               val = MPEG2_Get_Bits(dec, 12);
               if ((val&2047)==0) {
                    if (!MPEG2_Quiet_Flag)
                         printf("invalid escape in MPEG2_Decode_MPEG2_Intra_Block()\n");
                    dec->Fault_Flag = 1;
                    return;
               }
               if ((sign = (val>=2048)))
                    val = 4096 - val;
          }
          else {
               i+= run = tab->run;
               val = tab->level;
               sign = MPEG2_Get_Bits(dec, 1);
          }

          if (i>=64) {
               if (!MPEG2_Quiet_Flag)
                    fprintf(stderr,"DCT coeff index (i) out of bounds (intra2)\n");
               dec->Fault_Flag = 1;
               return;
          }

          j = MPEG2_scan[dec->alternate_scan][i];
          val = (val * dec->quantizer_scale * qmat[j]) >> 4;
          bp[j] = sign ? -val : val;
          nc++;
     }
}


/* decode one non-intra coded MPEG-2 block */

void
MPEG2_Decode_MPEG2_Non_Intra_Block(MPEG2_Decoder *dec, int comp)
{
     int val, i, j, sign, nc, run;
     unsigned int code;
     const DCTtab *tab;
     short *bp;
     int *qmat;

     bp = dec->block[comp];

     qmat = (comp<4 || dec->chroma_format==CHROMA420)
            ? dec->non_intra_quantizer_matrix
            : dec->chroma_non_intra_quantizer_matrix;

     nc = 0;

     /* decode AC coefficients */
     for (i=0; ; i++) {
          code = MPEG2_Show_Bits(dec, 16);
          if (code>=16384) {
               if (i==0)
                    tab = &DCTtabfirst[(code>>12)-4];
               else
                    tab = &DCTtabnext[(code>>12)-4];
          }
          else if (code>=1024)
               tab = &DCTtab0[(code>>8)-4];
          else if (code>=512)
               tab = &DCTtab1[(code>>6)-8];
          else if (code>=256)
               tab = &DCTtab2[(code>>4)-16];
          else if (code>=128)
               tab = &DCTtab3[(code>>3)-16];
          else if (code>=64)
               tab = &DCTtab4[(code>>2)-16];
          else if (code>=32)
               tab = &DCTtab5[(code>>1)-16];
          else if (code>=16)
               tab = &DCTtab6[code-16];
          else {
               if (!MPEG2_Quiet_Flag)
                    printf("invalid Huffman code in MPEG2_Decode_MPEG2_Non_Intra_Block()\n");
               dec->Fault_Flag = 1;
               return;
          }

          MPEG2_Flush_Buffer(dec, tab->len);

          if (tab->run==64) { /* end_of_block */
               return;
          }

          if (tab->run==65) { /* escape */
               i+= run = MPEG2_Get_Bits(dec, 6);

               val = MPEG2_Get_Bits(dec, 12);
               if ((val&2047)==0) {
                    if (!MPEG2_Quiet_Flag)
                         printf("invalid escape in MPEG2_Decode_MPEG2_Intra_Block()\n");
                    dec->Fault_Flag = 1;
                    return;
               }
               if ((sign = (val>=2048)))
                    val = 4096 - val;
          }
          else {
               i+= run = tab->run;
               val = tab->level;
               sign = MPEG2_Get_Bits(dec, 1);
          }

          if (i>=64) {
               if (!MPEG2_Quiet_Flag)
                    fprintf(stderr,"DCT coeff index (i) out of bounds (inter2)\n");
               dec->Fault_Flag = 1;
               return;
          }

          j = MPEG2_scan[dec->alternate_scan][i];
          val = (((val<<1)+1) * dec->quantizer_scale * qmat[j]) >> 5;
          bp[j] = sign ? -val : val;
          nc++;
     }
}
