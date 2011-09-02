/* motion.c, motion vector decoding                                         */

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

/* private prototypes */
static void decode_motion_vector (MPEG2_Decoder *dec, int *pred, int r_size, int motion_code,
                                  int motion_residualesidual, int full_pel_vector);

/* ISO/IEC 13818-2 sections 6.2.5.2, 6.3.17.2, and 7.6.3: Motion vectors */
void
MPEG2_motion_vectors(MPEG2_Decoder *dec,
                     int PMV[2][2][2],
                     int dmvector[2],
                     int motion_vertical_field_select[2][2],
                     int s, int motion_vector_count, int mv_format, int h_r_size, int v_r_size, int dmv, int mvscale)
{
     if (motion_vector_count==1) {
          if (mv_format==MV_FIELD && !dmv) {
               motion_vertical_field_select[1][s] = motion_vertical_field_select[0][s] = MPEG2_Get_Bits(dec, 1);
          }

          MPEG2_motion_vector(dec, PMV[0][s],dmvector,h_r_size,v_r_size,dmv,mvscale,0);

          /* update other motion vector predictors */
          PMV[1][s][0] = PMV[0][s][0];
          PMV[1][s][1] = PMV[0][s][1];
     }
     else {
          motion_vertical_field_select[0][s] = MPEG2_Get_Bits(dec, 1);
          MPEG2_motion_vector(dec, PMV[0][s],dmvector,h_r_size,v_r_size,dmv,mvscale,0);

          motion_vertical_field_select[1][s] = MPEG2_Get_Bits(dec, 1);
          MPEG2_motion_vector(dec, PMV[1][s],dmvector,h_r_size,v_r_size,dmv,mvscale,0);
     }
}

/* get and decode motion vector and differential motion vector 
   for one prediction */
void
MPEG2_motion_vector(MPEG2_Decoder *dec,
                    int *PMV,
                    int *dmvector,
                    int h_r_size,
                    int v_r_size,
                    int dmv, /* MPEG-2 only: get differential motion vectors */
                    int mvscale, /* MPEG-2 only: field vector in frame pic */
                    int full_pel_vector) /* MPEG-1 only */
{
     int motion_code, motion_residual;

     /* horizontal component */
     /* ISO/IEC 13818-2 Table B-10 */
     motion_code = MPEG2_Get_motion_code(dec);

     motion_residual = (h_r_size!=0 && motion_code!=0) ? MPEG2_Get_Bits(dec, h_r_size) : 0;


     decode_motion_vector(dec, &PMV[0],h_r_size,motion_code,motion_residual,full_pel_vector);

     if (dmv)
          dmvector[0] = MPEG2_Get_dmvector(dec);


     /* vertical component */
     motion_code     = MPEG2_Get_motion_code(dec);
     motion_residual = (v_r_size!=0 && motion_code!=0) ? MPEG2_Get_Bits(dec, v_r_size) : 0;

     if (mvscale)
          PMV[1] >>= 1; /* DIV 2 */

     decode_motion_vector(dec, &PMV[1],v_r_size,motion_code,motion_residual,full_pel_vector);

     if (mvscale)
          PMV[1] <<= 1;

     if (dmv)
          dmvector[1] = MPEG2_Get_dmvector(dec);
}

/* calculate motion vector component */
/* ISO/IEC 13818-2 section 7.6.3.1: Decoding the motion vectors */
/* Note: the arithmetic here is more elegant than that which is shown 
   in 7.6.3.1.  The end results (PMV[][][]) should, however, be the same.  */

static void
decode_motion_vector(MPEG2_Decoder *dec,
                     int *pred,
                     int r_size, int motion_code, int motion_residual,
                     int full_pel_vector) /* MPEG-1 (ISO/IEC 11172-1) support */
{
     int lim, vec;

     lim = 16<<r_size;
     vec = full_pel_vector ? (*pred >> 1) : (*pred);

     if (motion_code>0) {
          vec+= ((motion_code-1)<<r_size) + motion_residual + 1;
          if (vec>=lim)
               vec-= lim + lim;
     }
     else if (motion_code<0) {
          vec-= ((-motion_code-1)<<r_size) + motion_residual + 1;
          if (vec<-lim)
               vec+= lim + lim;
     }
     *pred = full_pel_vector ? (vec<<1) : vec;
}


/* ISO/IEC 13818-2 section 7.6.3.6: Dual prime additional arithmetic */
void
MPEG2_Dual_Prime_Arithmetic(MPEG2_Decoder *dec,
                            int DMV[][2],
                            int *dmvector, /* differential motion vector */
                            int mvx, int mvy)  /* decoded mv components (always in field format) */
{
     if (dec->picture_structure==FRAME_PICTURE) {
          if (dec->top_field_first) {
               /* vector for prediction of top field from bottom field */
               DMV[0][0] = ((mvx  +(mvx>0))>>1) + dmvector[0];
               DMV[0][1] = ((mvy  +(mvy>0))>>1) + dmvector[1] - 1;

               /* vector for prediction of bottom field from top field */
               DMV[1][0] = ((3*mvx+(mvx>0))>>1) + dmvector[0];
               DMV[1][1] = ((3*mvy+(mvy>0))>>1) + dmvector[1] + 1;
          }
          else {
               /* vector for prediction of top field from bottom field */
               DMV[0][0] = ((3*mvx+(mvx>0))>>1) + dmvector[0];
               DMV[0][1] = ((3*mvy+(mvy>0))>>1) + dmvector[1] - 1;

               /* vector for prediction of bottom field from top field */
               DMV[1][0] = ((mvx  +(mvx>0))>>1) + dmvector[0];
               DMV[1][1] = ((mvy  +(mvy>0))>>1) + dmvector[1] + 1;
          }
     }
     else {
          /* vector for prediction from field of opposite 'parity' */
          DMV[0][0] = ((mvx+(mvx>0))>>1) + dmvector[0];
          DMV[0][1] = ((mvy+(mvy>0))>>1) + dmvector[1];

          /* correct for vertical field shift */
          if (dec->picture_structure==TOP_FIELD)
               DMV[0][1]--;
          else
               DMV[0][1]++;
     }
}

