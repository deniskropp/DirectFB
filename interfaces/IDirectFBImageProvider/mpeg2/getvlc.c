/* getvlc.c, variable length decoding                                       */

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
#include "getvlc.h"

/* private prototypes */
/* generic picture macroblock type processing functions */
static int Get_I_macroblock_type (MPEG2_Decoder *dec);
static int Get_P_macroblock_type (MPEG2_Decoder *dec);
static int Get_B_macroblock_type (MPEG2_Decoder *dec);
static int Get_D_macroblock_type (MPEG2_Decoder *dec);


int
MPEG2_Get_macroblock_type(MPEG2_Decoder *dec)
{
     int macroblock_type = 0;

     switch (dec->picture_coding_type) {
          case I_TYPE:
               macroblock_type = Get_I_macroblock_type(dec);
               break;
          case P_TYPE:
               macroblock_type = Get_P_macroblock_type(dec);
               break;
          case B_TYPE:
               macroblock_type = Get_B_macroblock_type(dec);
               break;
          case D_TYPE:
               macroblock_type = Get_D_macroblock_type(dec);
               break;
          default:
               printf("MPEG2_Get_macroblock_type(): unrecognized picture coding type\n");
               break;
     }

     return macroblock_type;
}

static int
Get_I_macroblock_type(MPEG2_Decoder *dec)
{
     if (MPEG2_Get_Bits1(dec)) {
          return 1;
     }

     if (!MPEG2_Get_Bits1(dec)) {
          if (!MPEG2_Quiet_Flag)
               printf("Invalid macroblock_type code\n");
          dec->Fault_Flag = 1;
     }

     return 17;
}

static int
Get_P_macroblock_type(MPEG2_Decoder *dec)
{
     int code;

     if ((code = MPEG2_Show_Bits(dec, 6))>=8) {
          code >>= 3;
          MPEG2_Flush_Buffer(dec, PMBtab0[code].len);
          return PMBtab0[code].val;
     }

     if (code==0) {
          if (!MPEG2_Quiet_Flag)
               printf("Invalid macroblock_type code\n");
          dec->Fault_Flag = 1;
          return 0;
     }

     MPEG2_Flush_Buffer(dec, PMBtab1[code].len);

     return PMBtab1[code].val;
}

static int
Get_B_macroblock_type(MPEG2_Decoder *dec)
{
     int code;

     if ((code = MPEG2_Show_Bits(dec, 6))>=8) {
          code >>= 2;
          MPEG2_Flush_Buffer(dec, BMBtab0[code].len);

          return BMBtab0[code].val;
     }

     if (code==0) {
          if (!MPEG2_Quiet_Flag)
               printf("Invalid macroblock_type code\n");
          dec->Fault_Flag = 1;
          return 0;
     }

     MPEG2_Flush_Buffer(dec, BMBtab1[code].len);

     return BMBtab1[code].val;
}

static int
Get_D_macroblock_type(MPEG2_Decoder *dec)
{
     if (!MPEG2_Get_Bits1(dec)) {
          if (!MPEG2_Quiet_Flag)
               printf("Invalid macroblock_type code\n");
          dec->Fault_Flag=1;
     }

     return 1;
}

int
MPEG2_Get_motion_code(MPEG2_Decoder *dec)
{
     int code;

     if (MPEG2_Get_Bits1(dec)) {
          return 0;
     }

     if ((code = MPEG2_Show_Bits(dec, 9))>=64) {
          code >>= 6;
          MPEG2_Flush_Buffer(dec, MVtab0[code].len);

          return MPEG2_Get_Bits1(dec)?-MVtab0[code].val:MVtab0[code].val;
     }

     if (code>=24) {
          code >>= 3;
          MPEG2_Flush_Buffer(dec, MVtab1[code].len);

          return MPEG2_Get_Bits1(dec)?-MVtab1[code].val:MVtab1[code].val;
     }

     if ((code-=12)<0) {
          if (!MPEG2_Quiet_Flag)
/* HACK */
               printf("Invalid MPEG2_motion_vector code (MBA %d, pic %d)\n", dec->global_MBA, dec->global_pic);
          dec->Fault_Flag=1;
          return 0;
     }

     MPEG2_Flush_Buffer(dec, MVtab2[code].len);

     return MPEG2_Get_Bits1(dec) ? -MVtab2[code].val : MVtab2[code].val;
}

/* get differential motion vector (for dual prime prediction) */
int
MPEG2_Get_dmvector(MPEG2_Decoder *dec)
{
     if (MPEG2_Get_Bits(dec, 1)) {
          return MPEG2_Get_Bits(dec, 1) ? -1 : 1;
     }
     else {
          return 0;
     }
}

int
MPEG2_Get_coded_block_pattern(MPEG2_Decoder *dec)
{
     int code;

     if ((code = MPEG2_Show_Bits(dec, 9))>=128) {
          code >>= 4;
          MPEG2_Flush_Buffer(dec, CBPtab0[code].len);

          return CBPtab0[code].val;
     }

     if (code>=8) {
          code >>= 1;
          MPEG2_Flush_Buffer(dec, CBPtab1[code].len);

          return CBPtab1[code].val;
     }

     if (code<1) {
          if (!MPEG2_Quiet_Flag)
               printf("Invalid coded_block_pattern code\n");
          dec->Fault_Flag = 1;
          return 0;
     }

     MPEG2_Flush_Buffer(dec, CBPtab2[code].len);

     return CBPtab2[code].val;
}

int
MPEG2_Get_macroblock_address_increment(MPEG2_Decoder *dec)
{
     int code, val;

     val = 0;

     while ((code = MPEG2_Show_Bits(dec, 11))<24) {
          if (code!=15) { /* if not macroblock_stuffing */
               if (code==8) { /* if macroblock_escape */
                    val+= 33;
               }
               else {
                    if (!MPEG2_Quiet_Flag)
                         printf("Invalid macroblock_address_increment code\n");

                    dec->Fault_Flag = 1;
                    return 1;
               }
          }

          MPEG2_Flush_Buffer(dec, 11);
     }

     /* macroblock_address_increment == 1 */
     /* ('1' is in the MSB position of the lookahead) */
     if (code>=1024) {
          MPEG2_Flush_Buffer(dec, 1);
          return val + 1;
     }

     /* codes 00010 ... 011xx */
     if (code>=128) {
          /* remove leading zeros */
          code >>= 6;
          MPEG2_Flush_Buffer(dec, MBAtab1[code].len);


          return val + MBAtab1[code].val;
     }

     /* codes 00000011000 ... 0000111xxxx */
     code-= 24; /* remove common base */
     MPEG2_Flush_Buffer(dec, MBAtab2[code].len);

     return val + MBAtab2[code].val;
}

/* combined MPEG-1 and MPEG-2 stage. parse VLC and 
   perform dct_diff arithmetic.

   MPEG-1:  ISO/IEC 11172-2 section
   MPEG-2:  ISO/IEC 13818-2 section 7.2.1 
   
   Note: the arithmetic here is presented more elegantly than
   the spec, yet the results, dct_diff, are the same.
*/

int
MPEG2_Get_Luma_DC_dct_diff(MPEG2_Decoder *dec)
{
     int code, size, dct_diff;

     /* decode length */
     code = MPEG2_Show_Bits(dec, 5);

     if (code<31) {
          size = DClumtab0[code].val;
          MPEG2_Flush_Buffer(dec, DClumtab0[code].len);
     }
     else {
          code = MPEG2_Show_Bits(dec, 9) - 0x1f0;
          size = DClumtab1[code].val;
          MPEG2_Flush_Buffer(dec, DClumtab1[code].len);
     }

     if (size==0)
          dct_diff = 0;
     else {
          dct_diff = MPEG2_Get_Bits(dec, size);
          if ((dct_diff & (1<<(size-1)))==0)
               dct_diff-= (1<<size) - 1;
     }

     return dct_diff;
}


int
MPEG2_Get_Chroma_DC_dct_diff(MPEG2_Decoder *dec)
{
     int code, size, dct_diff;

     /* decode length */
     code = MPEG2_Show_Bits(dec, 5);

     if (code<31) {
          size = DCchromtab0[code].val;
          MPEG2_Flush_Buffer(dec, DCchromtab0[code].len);
     }
     else {
          code = MPEG2_Show_Bits(dec, 10) - 0x3e0;
          size = DCchromtab1[code].val;
          MPEG2_Flush_Buffer(dec, DCchromtab1[code].len);
     }

     if (size==0)
          dct_diff = 0;
     else {
          dct_diff = MPEG2_Get_Bits(dec, size);
          if ((dct_diff & (1<<(size-1)))==0)
               dct_diff-= (1<<size) - 1;
     }

     return dct_diff;
}
