
/* mpeg2dec.c, main(), initialization, option processing                    */

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
#include <ctype.h>
#include <fcntl.h>

#include <sys/types.h>
#include <unistd.h>

#include "global.h"

/* private prototypes */
static int  video_sequence (MPEG2_Decoder *dec, int *framenum);
static void Initialize_Sequence (MPEG2_Decoder *dec);
static void Initialize_Decoder (MPEG2_Decoder *dec);
static void Deinitialize_Sequence (MPEG2_Decoder *dec);

/* decoder operation control flags */
int MPEG2_Quiet_Flag = 0;
int MPEG2_Reference_IDCT_Flag = 0;

/* zig-zag and alternate MPEG2_scan patterns */
const unsigned char MPEG2_scan[2][64] = {
     { /* Zig-Zag MPEG2_scan pattern  */
          0,1,8,16,9,2,3,10,17,24,32,25,18,11,4,5,
          12,19,26,33,40,48,41,34,27,20,13,6,7,14,21,28,
          35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,
          58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63
     },
     { /* Alternate MPEG2_scan pattern */
          0,8,16,24,1,9,2,10,17,25,32,40,48,56,57,49,
          41,33,26,18,3,11,4,12,19,27,34,42,50,58,35,43,
          51,59,20,28,5,13,6,14,21,29,36,44,52,60,37,45,
          53,61,22,30,7,15,23,31,38,46,54,62,39,47,55,63
     }
};

/* non-linear quantization coefficient table */
const unsigned char MPEG2_Non_Linear_quantizer_scale[32] = {
     0, 1, 2, 3, 4, 5, 6, 7,
     8,10,12,14,16,18,20,22,
     24,28,32,36,40,44,48,52,
     56,64,72,80,88,96,104,112
};

MPEG2_Decoder *
MPEG2_Init (MPEG2_Read mpeg2_read, void *read_ctx, int *width, int *height)
{
     MPEG2_Decoder *dec;

     dec = calloc( 1, sizeof(MPEG2_Decoder) );
     if (!dec)
          return NULL;

     dec->mpeg2_read = mpeg2_read;
     dec->mpeg2_read_ctx = read_ctx;

     MPEG2_Initialize_Buffer(dec);

     Initialize_Decoder(dec);

     if (MPEG2_Get_Hdr(dec) != 1) {
          free(dec);
          return NULL;
     }

     if (width)
          *width = dec->horizontal_size;

     if (height)
          *height = dec->vertical_size;

     return dec;
}

int
MPEG2_Decode(MPEG2_Decoder *dec, MPEG2_Write mpeg2_write, void *write_ctx)
{
     int Bitstream_Framenum = 0;

     dec->mpeg2_write = mpeg2_write;
     dec->mpeg2_write_ctx = write_ctx;

     return video_sequence(dec, &Bitstream_Framenum);
}

void
MPEG2_Close(MPEG2_Decoder *dec)
{
}



/* IMPLEMENTAION specific rouintes */
static void
Initialize_Decoder(MPEG2_Decoder *dec)
{
     int i;

     dec->Clip = dec->_Clip + 384;

     for (i=-384; i<640; i++)
          dec->Clip[i] = (i<0) ? 0 : ((i>255) ? 255 : i);

     /* IDCT */
     if (MPEG2_Reference_IDCT_Flag)
          MPEG2_Initialize_Reference_IDCT(dec);
     else
          MPEG2_Initialize_Fast_IDCT(dec);

}

/* mostly IMPLEMENTAION specific rouintes */
static void
Initialize_Sequence(MPEG2_Decoder *dec)
{
     int cc, size;
     static int Table_6_20[3] = {6,8,12};

     /* force MPEG-1 parameters for proper decoder behavior */
     /* see ISO/IEC 13818-2 section D.9.14 */
     if (!dec->MPEG2_Flag) {
          dec->progressive_sequence = 1;
          dec->progressive_frame = 1;
          dec->picture_structure = FRAME_PICTURE;
          dec->frame_pred_frame_dct = 1;
          dec->chroma_format = CHROMA420;
          dec->matrix_coefficients = 5;
     }

     /* round to nearest multiple of coded macroblocks */
     /* ISO/IEC 13818-2 section 6.3.3 sequence_header() */
     dec->mb_width = (dec->horizontal_size+15)/16;
     dec->mb_height = (dec->MPEG2_Flag && !dec->progressive_sequence) ? 2*((dec->vertical_size+31)/32)
                      : (dec->vertical_size+15)/16;

     dec->Coded_Picture_Width = 16*dec->mb_width;
     dec->Coded_Picture_Height = 16*dec->mb_height;

     /* ISO/IEC 13818-2 sections 6.1.1.8, 6.1.1.9, and 6.1.1.10 */
     dec->Chroma_Width = (dec->chroma_format==CHROMA444) ? dec->Coded_Picture_Width
                         : dec->Coded_Picture_Width>>1;
     dec->Chroma_Height = (dec->chroma_format!=CHROMA420) ? dec->Coded_Picture_Height
                          : dec->Coded_Picture_Height>>1;

     /* derived based on Table 6-20 in ISO/IEC 13818-2 section 6.3.17 */
     dec->block_count = Table_6_20[dec->chroma_format-1];

     for (cc=0; cc<3; cc++) {
          if (cc==0)
               size = dec->Coded_Picture_Width*dec->Coded_Picture_Height;
          else
               size = dec->Chroma_Width*dec->Chroma_Height;

          if (!(dec->backward_reference_frame[cc] = (unsigned char *)malloc(size)))
               MPEG2_Error(dec, "backward_reference_frame[] malloc failed\n");

          if (!(dec->forward_reference_frame[cc] = (unsigned char *)malloc(size)))
               MPEG2_Error(dec, "forward_reference_frame[] malloc failed\n");

          if (!(dec->auxframe[cc] = (unsigned char *)malloc(size)))
               MPEG2_Error(dec, "auxframe[] malloc failed\n");
     }
}

void
MPEG2_Error(MPEG2_Decoder *dec, char *text)
{
     fprintf(stderr,text);
     //  exit(1);
}

static void
Deinitialize_Sequence(MPEG2_Decoder *dec)
{
     int i;

     /* clear flags */
     dec->MPEG2_Flag=0;

     for (i=0;i<3;i++) {
          free(dec->backward_reference_frame[i]);
          free(dec->forward_reference_frame[i]);
          free(dec->auxframe[i]);
     }
}


static int
video_sequence(MPEG2_Decoder *dec, int *Bitstream_Framenumber)
{
     int Bitstream_Framenum;
     int Sequence_Framenum;
     int Return_Value;

     Bitstream_Framenum = *Bitstream_Framenumber;
     Sequence_Framenum=0;

     Initialize_Sequence(dec);

     /* decode picture whose header has already been parsed in 
        Decode_Bitstream() */


     MPEG2_Decode_Picture(dec, Bitstream_Framenum, Sequence_Framenum);

     /* update picture numbers */
     if (!dec->Second_Field) {
          Bitstream_Framenum++;
          Sequence_Framenum++;
     }

     /* loop through the rest of the pictures in the sequence */
     while ((Return_Value=MPEG2_Get_Hdr(dec))) {
          MPEG2_Decode_Picture(dec, Bitstream_Framenum, Sequence_Framenum);

          if (!dec->Second_Field) {
               Bitstream_Framenum++;
               Sequence_Framenum++;
          }
     }

     /* put last frame */
     if (Sequence_Framenum!=0) {
          MPEG2_Output_Last_Frame_of_Sequence(dec, Bitstream_Framenum);
     }

     Deinitialize_Sequence(dec);

     *Bitstream_Framenumber = Bitstream_Framenum;

     return(Return_Value);
}

