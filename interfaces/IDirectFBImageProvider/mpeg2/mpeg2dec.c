
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

#define GLOBAL
#include "global.h"

/* private prototypes */
static int  video_sequence (int *framenum);
#if 0
static int Decode_Bitstream (void);
#endif
static int  Headers (void);
static void Initialize_Sequence (void);
static void Initialize_Decoder (void);
static void Deinitialize_Sequence (void);


static void Clear_Options();


int
MPEG2_Init (MPEG2_Read mpeg2_read, void *read_ctx, int *width, int *height)
{
  int ret;

  Clear_Options();

  ld = &base; /* select base layer context */

  ld->mpeg2_read = mpeg2_read;
  ld->mpeg2_read_ctx = read_ctx;

  MPEG2_Initialize_Buffer(); 

  Initialize_Decoder();

  ret = Headers();
  if (ret != 1)
    return -1;

  if (width)
    *width = horizontal_size;

  if (height)
    *height = vertical_size;

  return 0;
}

int
MPEG2_Decode(MPEG2_Write mpeg2_write, void *write_ctx)
{
  int Bitstream_Framenum = 0;

  ld->mpeg2_write = mpeg2_write;
  ld->mpeg2_write_ctx = write_ctx;

  return video_sequence(&Bitstream_Framenum);
}

void
MPEG2_Close()
{
  free( MPEG2_Clip - 384 );
  MPEG2_Clip = NULL;
}



/* IMPLEMENTAION specific rouintes */
static void Initialize_Decoder()
{
  int i;

  /* MPEG2_Clip table */
  if (!(MPEG2_Clip=(unsigned char *)malloc(1024)))
    MPEG2_Error("MPEG2_Clip[] malloc failed\n");

  MPEG2_Clip += 384;

  for (i=-384; i<640; i++)
    MPEG2_Clip[i] = (i<0) ? 0 : ((i>255) ? 255 : i);

  /* IDCT */
  if (MPEG2_Reference_IDCT_Flag)
    MPEG2_Initialize_Reference_IDCT();
  else
    MPEG2_Initialize_Fast_IDCT();

}

/* mostly IMPLEMENTAION specific rouintes */
static void Initialize_Sequence()
{
  int cc, size;
  static int Table_6_20[3] = {6,8,12};

  /* force MPEG-1 parameters for proper decoder behavior */
  /* see ISO/IEC 13818-2 section D.9.14 */
  if (!base.MPEG2_Flag)
  {
    progressive_sequence = 1;
    progressive_frame = 1;
    picture_structure = FRAME_PICTURE;
    frame_pred_frame_dct = 1;
    chroma_format = CHROMA420;
    matrix_coefficients = 5;
  }

  /* round to nearest multiple of coded macroblocks */
  /* ISO/IEC 13818-2 section 6.3.3 sequence_header() */
  mb_width = (horizontal_size+15)/16;
  mb_height = (base.MPEG2_Flag && !progressive_sequence) ? 2*((vertical_size+31)/32)
                                        : (vertical_size+15)/16;

  Coded_Picture_Width = 16*mb_width;
  Coded_Picture_Height = 16*mb_height;

  /* ISO/IEC 13818-2 sections 6.1.1.8, 6.1.1.9, and 6.1.1.10 */
  Chroma_Width = (chroma_format==CHROMA444) ? Coded_Picture_Width
                                           : Coded_Picture_Width>>1;
  Chroma_Height = (chroma_format!=CHROMA420) ? Coded_Picture_Height
                                            : Coded_Picture_Height>>1;
  
  /* derived based on Table 6-20 in ISO/IEC 13818-2 section 6.3.17 */
  block_count = Table_6_20[chroma_format-1];

  for (cc=0; cc<3; cc++)
  {
    if (cc==0)
      size = Coded_Picture_Width*Coded_Picture_Height;
    else
      size = Chroma_Width*Chroma_Height;

    if (!(backward_reference_frame[cc] = (unsigned char *)malloc(size)))
      MPEG2_Error("backward_reference_frame[] malloc failed\n");

    if (!(forward_reference_frame[cc] = (unsigned char *)malloc(size)))
      MPEG2_Error("forward_reference_frame[] malloc failed\n");

    if (!(auxframe[cc] = (unsigned char *)malloc(size)))
      MPEG2_Error("auxframe[] malloc failed\n");
  }
}

void MPEG2_Error(text)
char *text;
{
  fprintf(stderr,text);
  //  exit(1);
}

static int Headers()
{
  int ret;

  ld = &base;
  

  /* return when end of sequence (0) or picture
     header has been parsed (1) */

  ret = MPEG2_Get_Hdr();

  return ret;
}

#if 0
static int Decode_Bitstream()
{
  int ret;
  int Bitstream_Framenum;

  Bitstream_Framenum = 0;

  for(;;)
  {
    ret = Headers();
    
    if(ret==1)
    {
      ret = video_sequence(&Bitstream_Framenum);
    }
    else
      return(ret);
  }

}
#endif

static void Deinitialize_Sequence()
{
  int i;

  /* clear flags */
  base.MPEG2_Flag=0;

  for(i=0;i<3;i++)
  {
    free(backward_reference_frame[i]);
    free(forward_reference_frame[i]);
    free(auxframe[i]);
  }
}


static int video_sequence(Bitstream_Framenumber)
int *Bitstream_Framenumber;
{
  int Bitstream_Framenum;
  int Sequence_Framenum;
  int Return_Value;

  Bitstream_Framenum = *Bitstream_Framenumber;
  Sequence_Framenum=0;

  Initialize_Sequence();

  /* decode picture whose header has already been parsed in 
     Decode_Bitstream() */


  MPEG2_Decode_Picture(Bitstream_Framenum, Sequence_Framenum);

  /* update picture numbers */
  if (!Second_Field)
  {
    Bitstream_Framenum++;
    Sequence_Framenum++;
  }

  /* loop through the rest of the pictures in the sequence */
  while ((Return_Value=Headers()))
  {
    MPEG2_Decode_Picture(Bitstream_Framenum, Sequence_Framenum);

    if (!Second_Field)
    {
      Bitstream_Framenum++;
      Sequence_Framenum++;
    }
  }

  /* put last frame */
  if (Sequence_Framenum!=0)
  {
    MPEG2_Output_Last_Frame_of_Sequence(Bitstream_Framenum);
  }

  Deinitialize_Sequence();

  *Bitstream_Framenumber = Bitstream_Framenum;

  return(Return_Value);
}



static void Clear_Options()
{
  MPEG2_Reference_IDCT_Flag = 0;
  MPEG2_Quiet_Flag = 0;
}
