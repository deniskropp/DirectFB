/* getpic.c, picture decoding                                               */

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

/* private prototypes*/
static void picture_data (MPEG2_Decoder *dec, int framenum);
static void macroblock_modes (MPEG2_Decoder *dec, int *pmacroblock_type, int *pstwtype,
                              int *pstwclass, int *pmotion_type, int *pmotion_vector_count, int *pmv_format, int *pdmv,
                              int *pmvscale, int *pdct_type);
static void Clear_Block (MPEG2_Decoder *dec, int comp);
static void Add_Block (MPEG2_Decoder *dec, int comp, int bx, int by,
                       int dct_type, int addflag);
static void Update_Picture_Buffers (MPEG2_Decoder *dec);
static void frame_reorder (MPEG2_Decoder *dec, int bitstream_framenum, 
                           int sequence_framenum);

static void motion_compensation (MPEG2_Decoder *dec, int MBA, int macroblock_type, 
                                 int motion_type, int PMV[2][2][2], int motion_vertical_field_select[2][2], 
                                 int dmvector[2], int stwtype, int dct_type);

static void skipped_macroblock (MPEG2_Decoder *dec, int dc_dct_pred[3], 
                                int PMV[2][2][2], int *motion_type, int motion_vertical_field_select[2][2],
                                int *stwtype, int *macroblock_type);

static int slice (MPEG2_Decoder *dec, int framenum, int MBAmax);

static int start_of_slice (MPEG2_Decoder *dec, int MBAmax, int *MBA,
                           int *MBAinc, int dc_dct_pred[3], int PMV[2][2][2]);

static int decode_macroblock (MPEG2_Decoder *dec, int *macroblock_type, 
                              int *stwtype, int *stwclass, int *motion_type, int *dct_type,
                              int PMV[2][2][2], int dc_dct_pred[3], 
                              int motion_vertical_field_select[2][2], int dmvector[2]);


/* decode one frame or field picture */
void
MPEG2_Decode_Picture(MPEG2_Decoder *dec,
                     int bitstream_framenum, int sequence_framenum)
{

     if (dec->picture_structure==FRAME_PICTURE && dec->Second_Field) {
          /* recover from illegal number of field pictures */
          printf("odd number of field pictures\n");
          dec->Second_Field = 0;
     }

     /* IMPLEMENTATION: update picture buffer pointers */
     Update_Picture_Buffers(dec);

     /* decode picture data ISO/IEC 13818-2 section 6.2.3.7 */
     picture_data(dec, bitstream_framenum);

     /* write or display current or previously decoded reference frame */
     /* ISO/IEC 13818-2 section 6.1.1.11: Frame reordering */
     frame_reorder(dec, bitstream_framenum, sequence_framenum);

     if (dec->picture_structure!=FRAME_PICTURE)
          dec->Second_Field = !dec->Second_Field;
}


/* decode all macroblocks of the current picture */
/* stages described in ISO/IEC 13818-2 section 7 */
static void
picture_data(MPEG2_Decoder *dec, int framenum)
{
     int MBAmax;
     int ret;

     /* number of macroblocks per picture */
     MBAmax = dec->mb_width*dec->mb_height;

     if (dec->picture_structure!=FRAME_PICTURE)
          MBAmax>>=1; /* field picture has half as mnay macroblocks as frame */

     for (;;) {
          if ((ret=slice(dec, framenum, MBAmax))<0)
               return;
     }

}



/* decode all macroblocks of the current picture */
/* ISO/IEC 13818-2 section 6.3.16 */
static int
slice(MPEG2_Decoder *dec, int framenum, int MBAmax)
{
     int MBA; 
     int MBAinc, macroblock_type = 0, motion_type = 0, dct_type = 0;
     int dc_dct_pred[3];
     int PMV[2][2][2], motion_vertical_field_select[2][2];
     int dmvector[2];
     int stwtype = 0, stwclass = 0;
     int ret;

     MBA = 0; /* macroblock address */
     MBAinc = 0;

     if ((ret=start_of_slice(dec, MBAmax, &MBA, &MBAinc, dc_dct_pred, PMV))!=1)
          return(ret);

     dec->Fault_Flag=0;

     for (;;) {

          /* this is how we properly exit out of picture */
          if (MBA>=MBAmax)
               return(-1); /* all macroblocks decoded */

          if (MBAinc==0) {
               if (!MPEG2_Show_Bits(dec, 23) || dec->Fault_Flag) { /* MPEG2_next_start_code or fault */
                    resync: /* if dec->Fault_Flag: resynchronize to next MPEG2_next_start_code */
                    dec->Fault_Flag = 0;
                    return(0);     /* trigger: go to next slice */
               }
               else { /* neither MPEG2_next_start_code nor dec->Fault_Flag */
                    /* decode macroblock address increment */
                    MBAinc = MPEG2_Get_macroblock_address_increment(dec);

                    if (dec->Fault_Flag) goto resync;
               }
          }

          if (MBA>=MBAmax) {
               /* MBAinc points beyond picture dimensions */
               if (!MPEG2_Quiet_Flag)
                    printf("Too many macroblocks in picture\n");
               return(-1);
          }

          if (MBAinc==1) { /* not skipped */
               ret = decode_macroblock(dec, &macroblock_type, &stwtype, &stwclass,
                                       &motion_type, &dct_type, PMV, dc_dct_pred, 
                                       motion_vertical_field_select, dmvector);

               if (ret==-1)
                    return(-1);

               if (ret==0)
                    goto resync;

          }
          else { /* MBAinc!=1: skipped macroblock */
               /* ISO/IEC 13818-2 section 7.6.6 */
               skipped_macroblock(dec, dc_dct_pred, PMV, &motion_type, 
                                  motion_vertical_field_select, &stwtype, &macroblock_type);
          }

          /* ISO/IEC 13818-2 section 7.6 */
          motion_compensation(dec, MBA, macroblock_type, motion_type, PMV, 
                              motion_vertical_field_select, dmvector, stwtype, dct_type);


          /* advance to next macroblock */
          MBA++;
          MBAinc--;

          if (MBA>=MBAmax)
               return(-1); /* all macroblocks decoded */
     }
}


/* ISO/IEC 13818-2 section 6.3.17.1: Macroblock modes */
static void
macroblock_modes(MPEG2_Decoder *dec, int *pmacroblock_type, int *pstwtype, int *pstwclass,
                 int *pmotion_type,int *pmotion_vector_count,int *pmv_format,int *pdmv,int *pmvscale,int *pdct_type)
{
     int macroblock_type;
     int stwtype, stwclass;
     int motion_type = 0;
     int motion_vector_count, mv_format, dmv, mvscale;
     int dct_type;
     static unsigned char stwclass_table[9]
     = {0, 1, 2, 1, 1, 2, 3, 3, 4};

     /* get macroblock_type */
     macroblock_type = MPEG2_Get_macroblock_type(dec);

     if (dec->Fault_Flag) return;

     /* get spatial_temporal_weight_code */
     if (macroblock_type & MB_WEIGHT) {
          stwtype = 4;
     }
     else
          stwtype = (macroblock_type & MB_CLASS4) ? 8 : 0;

     /* SCALABILITY: derive spatial_temporal_weight_class (Table 7-18) */
     stwclass = stwclass_table[stwtype];

     /* get frame/field motion type */
     if (macroblock_type & (MACROBLOCK_MOTION_FORWARD|MACROBLOCK_MOTION_BACKWARD)) {
          if (dec->picture_structure==FRAME_PICTURE) { /* frame_motion_type */
               motion_type = dec->frame_pred_frame_dct ? MC_FRAME : MPEG2_Get_Bits(dec, 2);
          }
          else { /* field_motion_type */
               motion_type = MPEG2_Get_Bits(dec, 2);
          }
     }
     else if ((macroblock_type & MACROBLOCK_INTRA) && dec->concealment_motion_vectors) {
          /* concealment motion vectors */
          motion_type = (dec->picture_structure==FRAME_PICTURE) ? MC_FRAME : MC_FIELD;
     }
#if 0
     else {
          printf("maroblock_modes(): unknown macroblock type\n");
          motion_type = -1;
     }
#endif

     /* derive motion_vector_count, mv_format and dmv, (table 6-17, 6-18) */
     if (dec->picture_structure==FRAME_PICTURE) {
          motion_vector_count = (motion_type==MC_FIELD && stwclass<2) ? 2 : 1;
          mv_format = (motion_type==MC_FRAME) ? MV_FRAME : MV_FIELD;
     }
     else {
          motion_vector_count = (motion_type==MC_16X8) ? 2 : 1;
          mv_format = MV_FIELD;
     }

     dmv = (motion_type==MC_DMV); /* dual prime */

     /* field mv predictions in frame pictures have to be scaled
      * ISO/IEC 13818-2 section 7.6.3.1 Decoding the motion vectors
      * IMPLEMENTATION: mvscale is derived for later use in MPEG2_motion_vectors()
      * it displaces the stage:
      *
      *    if((mv_format=="field")&&(t==1)&&(picture_structure=="Frame picture"))
      *      prediction = PMV[r][s][t] DIV 2;
      */

     mvscale = ((mv_format==MV_FIELD) && (dec->picture_structure==FRAME_PICTURE));

     /* get dct_type (frame DCT / field DCT) */
     dct_type = (dec->picture_structure==FRAME_PICTURE)
                && (!dec->frame_pred_frame_dct)
                && (macroblock_type & (MACROBLOCK_PATTERN|MACROBLOCK_INTRA))
                ? MPEG2_Get_Bits(dec, 1)
                : 0;

     /* return values */
     *pmacroblock_type = macroblock_type;
     *pstwtype = stwtype;
     *pstwclass = stwclass;
     *pmotion_type = motion_type;
     *pmotion_vector_count = motion_vector_count;
     *pmv_format = mv_format;
     *pdmv = dmv;
     *pmvscale = mvscale;
     *pdct_type = dct_type;
}


/* move/add 8x8-Block from block[comp] to backward_reference_frame */
/* copy reconstructed 8x8 block from block[comp] to current_frame[]
 * ISO/IEC 13818-2 section 7.6.8: Adding prediction and coefficient data
 * This stage also embodies some of the operations implied by:
 *   - ISO/IEC 13818-2 section 7.6.7: Combining predictions
 *   - ISO/IEC 13818-2 section 6.1.3: Macroblock
*/
static void
Add_Block(MPEG2_Decoder *dec, int comp,int bx,int by,int dct_type,int addflag)
{
     int cc,i, j, iincr;
     unsigned char *rfp;
     short *bp;


     /* derive color component index */
     /* equivalent to ISO/IEC 13818-2 Table 7-1 */
     cc = (comp<4) ? 0 : (comp&1)+1; /* color component index */

     if (cc==0) {
          /* luminance */

          if (dec->picture_structure==FRAME_PICTURE)
               if (dct_type) {
                    /* field DCT coding */
                    rfp = dec->current_frame[0]
                          + dec->Coded_Picture_Width*(by+((comp&2)>>1)) + bx + ((comp&1)<<3);
                    iincr = (dec->Coded_Picture_Width<<1) - 8;
               }
               else {
                    /* frame DCT coding */
                    rfp = dec->current_frame[0]
                          + dec->Coded_Picture_Width*(by+((comp&2)<<2)) + bx + ((comp&1)<<3);
                    iincr = dec->Coded_Picture_Width - 8;
               }
          else {
               /* field picture */
               rfp = dec->current_frame[0]
                     + (dec->Coded_Picture_Width<<1)*(by+((comp&2)<<2)) + bx + ((comp&1)<<3);
               iincr = (dec->Coded_Picture_Width<<1) - 8;
          }
     }
     else {
          /* chrominance */

          /* scale coordinates */
          if (dec->chroma_format!=CHROMA444)
               bx >>= 1;
          if (dec->chroma_format==CHROMA420)
               by >>= 1;
          if (dec->picture_structure==FRAME_PICTURE) {
               if (dct_type && (dec->chroma_format!=CHROMA420)) {
                    /* field DCT coding */
                    rfp = dec->current_frame[cc]
                          + dec->Chroma_Width*(by+((comp&2)>>1)) + bx + (comp&8);
                    iincr = (dec->Chroma_Width<<1) - 8;
               }
               else {
                    /* frame DCT coding */
                    rfp = dec->current_frame[cc]
                          + dec->Chroma_Width*(by+((comp&2)<<2)) + bx + (comp&8);
                    iincr = dec->Chroma_Width - 8;
               }
          }
          else {
               /* field picture */
               rfp = dec->current_frame[cc]
                     + (dec->Chroma_Width<<1)*(by+((comp&2)<<2)) + bx + (comp&8);
               iincr = (dec->Chroma_Width<<1) - 8;
          }
     }

     bp = dec->block[comp];

     if (addflag) {
          for (i=0; i<8; i++) {
               for (j=0; j<8; j++) {
                    *rfp = dec->Clip[*bp++ + *rfp];
                    rfp++;
               }

               rfp+= iincr;
          }
     }
     else {
          for (i=0; i<8; i++) {
               for (j=0; j<8; j++)
                    *rfp++ = dec->Clip[*bp++ + 128];

               rfp+= iincr;
          }
     }
}


/* IMPLEMENTATION: set scratch pad macroblock to zero */
static void
Clear_Block(MPEG2_Decoder *dec, int comp)
{
     short *Block_Ptr;
     int i;

     Block_Ptr = dec->block[comp];

     for (i=0; i<64; i++)
          *Block_Ptr++ = 0;
}


/* reuse old picture buffers as soon as they are no longer needed 
   based on life-time axioms of MPEG */
static void
Update_Picture_Buffers(MPEG2_Decoder *dec)
{                           
     int cc;              /* color component index */
     unsigned char *tmp;  /* temporary swap pointer */

     for (cc=0; cc<3; cc++) {
          /* B pictures do not need to be save for future reference */
          if (dec->picture_coding_type==B_TYPE) {
               dec->current_frame[cc] = dec->auxframe[cc];
          }
          else {
               /* only update at the beginning of the coded frame */
               if (!dec->Second_Field) {
                    tmp = dec->forward_reference_frame[cc];

                    /* the previously decoded reference frame is stored
                       coincident with the location where the backward 
                       reference frame is stored (backwards prediction is not
                       needed in P pictures) */
                    dec->forward_reference_frame[cc] = dec->backward_reference_frame[cc];

                    /* update pointer for potential future B pictures */
                    dec->backward_reference_frame[cc] = tmp;
               }

               /* can erase over old backward reference frame since it is not used
                  in a P picture, and since any subsequent B pictures will use the 
                  previously decoded I or P frame as the backward_reference_frame */
               dec->current_frame[cc] = dec->backward_reference_frame[cc];
          }

          /* IMPLEMENTATION:
             one-time folding of a line offset into the pointer which stores the
             memory address of the current frame saves offsets and conditional 
             branches throughout the remainder of the picture processing loop */
          if (dec->picture_structure==BOTTOM_FIELD)
               dec->current_frame[cc]+= (cc==0) ? dec->Coded_Picture_Width : dec->Chroma_Width;
     }
}


/* store last frame */

void
MPEG2_Output_Last_Frame_of_Sequence(MPEG2_Decoder *dec, int Framenum)
{
     if (dec->Second_Field)
          printf("last frame incomplete, not stored\n");
     else
          MPEG2_Write_Frame(dec, dec->backward_reference_frame,Framenum-1);
}



static void
frame_reorder(MPEG2_Decoder *dec, int Bitstream_Framenum, int Sequence_Framenum)
{
     /* tracking variables to insure proper output in spatial scalability */
     static int Oldref_progressive_frame, Newref_progressive_frame;

     if (Sequence_Framenum!=0) {
          if (dec->picture_structure==FRAME_PICTURE || dec->Second_Field) {
               if (dec->picture_coding_type==B_TYPE)
                    MPEG2_Write_Frame(dec, dec->auxframe,Bitstream_Framenum-1);
               else {
                    Newref_progressive_frame = dec->progressive_frame;
                    dec->progressive_frame = Oldref_progressive_frame;

                    MPEG2_Write_Frame(dec, dec->forward_reference_frame,Bitstream_Framenum-1);

                    Oldref_progressive_frame = dec->progressive_frame = Newref_progressive_frame;
               }
          }
     }
     else
          Oldref_progressive_frame = dec->progressive_frame;
}


/* ISO/IEC 13818-2 section 7.6 */
static void
motion_compensation(MPEG2_Decoder *dec,
int MBA,
int macroblock_type,
int motion_type,
int PMV[2][2][2],
int motion_vertical_field_select[2][2],
int dmvector[2],
int stwtype,
int dct_type)
{
     int bx, by;
     int comp;

     /* derive current macroblock position within picture */
     /* ISO/IEC 13818-2 section 6.3.1.6 and 6.3.1.7 */
     bx = 16*(MBA%dec->mb_width);
     by = 16*(MBA/dec->mb_width);

     /* motion compensation */
     if (!(macroblock_type & MACROBLOCK_INTRA))
          MPEG2_form_predictions(dec, bx,by,macroblock_type,motion_type,PMV,
                                 motion_vertical_field_select,dmvector,stwtype);

     /* copy or add block data into picture */
     for (comp=0; comp<dec->block_count; comp++) {
          /* ISO/IEC 13818-2 section Annex A: inverse DCT */
          if (MPEG2_Reference_IDCT_Flag)
               MPEG2_Reference_IDCT(dec, dec->block[comp]);
          else
               MPEG2_Fast_IDCT(dec, dec->block[comp]);

          /* ISO/IEC 13818-2 section 7.6.8: Adding prediction and coefficient data */
          Add_Block(dec, comp,bx,by,dct_type,(macroblock_type & MACROBLOCK_INTRA)==0);
     }
}

/* ISO/IEC 13818-2 section 7.6.6 */
static void skipped_macroblock(MPEG2_Decoder *dec,
int dc_dct_pred[3],
int PMV[2][2][2],
int *motion_type,
int motion_vertical_field_select[2][2],
int *stwtype,
int *macroblock_type)
{
     int comp;

     for (comp=0; comp<dec->block_count; comp++)
          Clear_Block(dec, comp);

     /* reset intra_dc predictors */
     /* ISO/IEC 13818-2 section 7.2.1: DC coefficients in intra blocks */
     dc_dct_pred[0]=dc_dct_pred[1]=dc_dct_pred[2]=0;

     /* reset motion vector predictors */
     /* ISO/IEC 13818-2 section 7.6.3.4: Resetting motion vector predictors */
     if (dec->picture_coding_type==P_TYPE)
          PMV[0][0][0]=PMV[0][0][1]=PMV[1][0][0]=PMV[1][0][1]=0;

     /* derive motion_type */
     if (dec->picture_structure==FRAME_PICTURE)
          *motion_type = MC_FRAME;
     else {
          *motion_type = MC_FIELD;

          /* predict from field of same parity */
          /* ISO/IEC 13818-2 section 7.6.6.1 and 7.6.6.3: P field picture and B field
             picture */
          motion_vertical_field_select[0][0]=motion_vertical_field_select[0][1] = 
                                             (dec->picture_structure==BOTTOM_FIELD);
     }

     /* skipped I are spatial-only predicted, */
     /* skipped P and B are temporal-only predicted */
     /* ISO/IEC 13818-2 section 7.7.6: Skipped macroblocks */
     *stwtype = (dec->picture_coding_type==I_TYPE) ? 8 : 0;

     /* IMPLEMENTATION: clear MACROBLOCK_INTRA */
     *macroblock_type&= ~MACROBLOCK_INTRA;

}

/* return==-1 means go to next picture */
/* the expression "start of slice" is used throughout the normative
   body of the MPEG specification */
static int
start_of_slice(MPEG2_Decoder *dec,
int MBAmax,
int *MBA,
int *MBAinc,
int dc_dct_pred[3],
int PMV[2][2][2])
{
     unsigned int code;
     int slice_vert_pos_ext;

     dec->Fault_Flag = 0;

     MPEG2_next_start_code(dec);
     code = MPEG2_Show_Bits(dec, 32);

     if (code<SLICE_START_CODE_MIN || code>SLICE_START_CODE_MAX) {
          /* only slice headers are allowed in picture_data */
          if (!MPEG2_Quiet_Flag)
               printf("start_of_slice(): Premature end of picture\n");

          return(-1);  /* trigger: go to next picture */
     }

     MPEG2_Flush_Buffer32(dec); 

     /* decode slice header (may change quantizer_scale) */
     slice_vert_pos_ext = MPEG2_slice_header(dec);

     /* decode macroblock address increment */
     *MBAinc = MPEG2_Get_macroblock_address_increment(dec);

     if (dec->Fault_Flag) {
          printf("start_of_slice(): MBAinc unsuccessful\n");
          return(0);   /* trigger: go to next slice */
     }

     /* set current location */
     /* NOTE: the arithmetic used to derive macroblock_address below is
      *       equivalent to ISO/IEC 13818-2 section 6.3.17: Macroblock
      */
     *MBA = ((slice_vert_pos_ext<<7) + (code&255) - 1)*dec->mb_width + *MBAinc - 1;
     *MBAinc = 1; /* first macroblock in slice: not skipped */

     /* reset all DC coefficient and motion vector predictors */
     /* reset all DC coefficient and motion vector predictors */
     /* ISO/IEC 13818-2 section 7.2.1: DC coefficients in intra blocks */
     dc_dct_pred[0]=dc_dct_pred[1]=dc_dct_pred[2]=0;

     /* ISO/IEC 13818-2 section 7.6.3.4: Resetting motion vector predictors */
     PMV[0][0][0]=PMV[0][0][1]=PMV[1][0][0]=PMV[1][0][1]=0;
     PMV[0][1][0]=PMV[0][1][1]=PMV[1][1][0]=PMV[1][1][1]=0;

     /* successfull: trigger decode macroblocks in slice */
     return(1);
}


/* ISO/IEC 13818-2 sections 7.2 through 7.5 */
static int decode_macroblock(MPEG2_Decoder *dec,
int *macroblock_type,
int *stwtype,
int *stwclass,
int *motion_type,
int *dct_type,
int PMV[2][2][2],
int dc_dct_pred[3],
int motion_vertical_field_select[2][2],
int dmvector[2])
{
     /* locals */
     int quantizer_scale_code; 
     int comp;

     int motion_vector_count = 0;
     int mv_format = 0;
     int dmv = 0;
     int mvscale = 0;
     int coded_block_pattern;

     /* ISO/IEC 13818-2 section 6.3.17.1: Macroblock modes */
     macroblock_modes(dec, macroblock_type, stwtype, stwclass,
                      motion_type, &motion_vector_count, &mv_format, &dmv, &mvscale,
                      dct_type);

     if (dec->Fault_Flag) return(0);  /* trigger: go to next slice */

     if (*macroblock_type & MACROBLOCK_QUANT) {
          quantizer_scale_code = MPEG2_Get_Bits(dec, 5);

          /* ISO/IEC 13818-2 section 7.4.2.2: Quantizer scale factor */
          if (dec->MPEG2_Flag)
               dec->quantizer_scale =
               dec->q_scale_type ? MPEG2_Non_Linear_quantizer_scale[quantizer_scale_code] 
               : (quantizer_scale_code << 1);
          else
               dec->quantizer_scale = quantizer_scale_code;
     }

     /* motion vectors */


     /* ISO/IEC 13818-2 section 6.3.17.2: Motion vectors */

     /* decode forward motion vectors */
     if ((*macroblock_type & MACROBLOCK_MOTION_FORWARD) 
         || ((*macroblock_type & MACROBLOCK_INTRA) 
             && dec->concealment_motion_vectors)) {
          if (dec->MPEG2_Flag)
               MPEG2_motion_vectors(dec, PMV,dmvector,motion_vertical_field_select,
                                    0,motion_vector_count,mv_format,dec->f_code[0][0]-1,dec->f_code[0][1]-1,
                                    dmv,mvscale);
          else
               MPEG2_motion_vector(dec, PMV[0][0],dmvector,
                                   dec->forward_f_code-1,dec->forward_f_code-1,0,0,dec->full_pel_forward_vector);
     }

     if (dec->Fault_Flag) return(0);  /* trigger: go to next slice */

     /* decode backward motion vectors */
     if (*macroblock_type & MACROBLOCK_MOTION_BACKWARD) {
          if (dec->MPEG2_Flag)
               MPEG2_motion_vectors(dec, PMV,dmvector,motion_vertical_field_select,
                                    1,motion_vector_count,mv_format,dec->f_code[1][0]-1,dec->f_code[1][1]-1,0,
                                    mvscale);
          else
               MPEG2_motion_vector(dec, PMV[0][1],dmvector,
                                   dec->backward_f_code-1,dec->backward_f_code-1,0,0,dec->full_pel_backward_vector);
     }

     if (dec->Fault_Flag) return(0);  /* trigger: go to next slice */

     if ((*macroblock_type & MACROBLOCK_INTRA) && dec->concealment_motion_vectors)
          MPEG2_Flush_Buffer(dec, 1); /* remove MPEG2_marker_bit */

     /* macroblock_pattern */
     /* ISO/IEC 13818-2 section 6.3.17.4: Coded block pattern */
     if (*macroblock_type & MACROBLOCK_PATTERN) {
          coded_block_pattern = MPEG2_Get_coded_block_pattern(dec);

          if (dec->chroma_format==CHROMA422) {
               /* coded_block_pattern_1 */
               coded_block_pattern = (coded_block_pattern<<2) | MPEG2_Get_Bits(dec, 2); 

          }
          else if (dec->chroma_format==CHROMA444) {
               /* coded_block_pattern_2 */
               coded_block_pattern = (coded_block_pattern<<6) | MPEG2_Get_Bits(dec, 6); 
          }
     }
     else
          coded_block_pattern = (*macroblock_type & MACROBLOCK_INTRA) ? 
                                (1<<dec->block_count)-1 : 0;

     if (dec->Fault_Flag) return(0);  /* trigger: go to next slice */

     /* decode blocks */
     for (comp=0; comp<dec->block_count; comp++) {
          Clear_Block(dec, comp);

          if (coded_block_pattern & (1<<(dec->block_count-1-comp))) {
               if (*macroblock_type & MACROBLOCK_INTRA) {
                    if (dec->MPEG2_Flag)
                         MPEG2_Decode_MPEG2_Intra_Block(dec, comp,dc_dct_pred);
                    else
                         MPEG2_Decode_MPEG1_Intra_Block(dec, comp,dc_dct_pred);
               }
               else {
                    if (dec->MPEG2_Flag)
                         MPEG2_Decode_MPEG2_Non_Intra_Block(dec, comp);
                    else
                         MPEG2_Decode_MPEG1_Non_Intra_Block(dec, comp);
               }

               if (dec->Fault_Flag) return(0);  /* trigger: go to next slice */
          }
     }

     if (dec->picture_coding_type==D_TYPE) {
          /* remove end_of_macroblock (always 1, prevents startcode emulation) */
          /* ISO/IEC 11172-2 section 2.4.2.7 and 2.4.3.6 */
          MPEG2_marker_bit(dec, "D picture end_of_macroblock bit");
     }

     /* reset intra_dc predictors */
     /* ISO/IEC 13818-2 section 7.2.1: DC coefficients in intra blocks */
     if (!(*macroblock_type & MACROBLOCK_INTRA))
          dc_dct_pred[0]=dc_dct_pred[1]=dc_dct_pred[2]=0;

     /* reset motion vector predictors */
     if ((*macroblock_type & MACROBLOCK_INTRA) && !dec->concealment_motion_vectors) {
          /* intra mb without concealment motion vectors */
          /* ISO/IEC 13818-2 section 7.6.3.4: Resetting motion vector predictors */
          PMV[0][0][0]=PMV[0][0][1]=PMV[1][0][0]=PMV[1][0][1]=0;
          PMV[0][1][0]=PMV[0][1][1]=PMV[1][1][0]=PMV[1][1][1]=0;
     }

     /* special "No_MC" macroblock_type case */
     /* ISO/IEC 13818-2 section 7.6.3.5: Prediction in P pictures */
     if ((dec->picture_coding_type==P_TYPE) 
         && !(*macroblock_type & (MACROBLOCK_MOTION_FORWARD|MACROBLOCK_INTRA))) {
          /* non-intra mb without forward mv in a P picture */
          /* ISO/IEC 13818-2 section 7.6.3.4: Resetting motion vector predictors */
          PMV[0][0][0]=PMV[0][0][1]=PMV[1][0][0]=PMV[1][0][1]=0;

          /* derive motion_type */
          /* ISO/IEC 13818-2 section 6.3.17.1: Macroblock modes, frame_motion_type */
          if (dec->picture_structure==FRAME_PICTURE)
               *motion_type = MC_FRAME;
          else {
               *motion_type = MC_FIELD;
               /* predict from field of same parity */
               motion_vertical_field_select[0][0] = (dec->picture_structure==BOTTOM_FIELD);
          }
     }

     if (*stwclass==4) {
          /* purely spatially predicted macroblock */
          /* ISO/IEC 13818-2 section 7.7.5.1: Resetting motion vector predictions */
          PMV[0][0][0]=PMV[0][0][1]=PMV[1][0][0]=PMV[1][0][1]=0;
          PMV[0][1][0]=PMV[0][1][1]=PMV[1][1][0]=PMV[1][1][1]=0;
     }

     /* successfully decoded macroblock */
     return(1);

} /* decode_macroblock */


