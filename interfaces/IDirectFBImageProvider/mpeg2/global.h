/* global.h, global variables                                               */

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

#include "mpeg2dec.h"


/* prototypes of global functions */

/* MPEG2_Get_Bits.c */
void MPEG2_Initialize_Buffer (MPEG2_Decoder *dec);
void MPEG2_Fill_Buffer (MPEG2_Decoder *dec);
unsigned int MPEG2_Show_Bits (MPEG2_Decoder *dec, int n);
unsigned int MPEG2_Get_Bits1 (MPEG2_Decoder *dec);
void MPEG2_Flush_Buffer (MPEG2_Decoder *dec, int n);
unsigned int MPEG2_Get_Bits (MPEG2_Decoder *dec, int n);

/* systems.c */
void MPEG2_Flush_Buffer32 (MPEG2_Decoder *dec);
unsigned int MPEG2_Get_Bits32 (MPEG2_Decoder *dec);

/* getblk.c */
void MPEG2_Decode_MPEG1_Intra_Block (MPEG2_Decoder *dec,
                                     int comp, int dc_dct_pred[]);
void MPEG2_Decode_MPEG1_Non_Intra_Block (MPEG2_Decoder *dec, int comp);
void MPEG2_Decode_MPEG2_Intra_Block (MPEG2_Decoder *dec,
                                     int comp, int dc_dct_pred[]);
void MPEG2_Decode_MPEG2_Non_Intra_Block (MPEG2_Decoder *dec, int comp);

/* gethdr.c */
int MPEG2_Get_Hdr (MPEG2_Decoder *dec);
void MPEG2_next_start_code (MPEG2_Decoder *dec);
int MPEG2_slice_header (MPEG2_Decoder *dec);
void MPEG2_marker_bit (MPEG2_Decoder *dec, char *text);

/* getpic.c */
void MPEG2_Decode_Picture (MPEG2_Decoder *dec,
                           int bitstream_framenum, int sequence_framenum);
void MPEG2_Output_Last_Frame_of_Sequence (MPEG2_Decoder *dec, int framenum);

/* getvlc.c */
int MPEG2_Get_macroblock_type (MPEG2_Decoder *dec);
int MPEG2_Get_motion_code (MPEG2_Decoder *dec);
int MPEG2_Get_dmvector (MPEG2_Decoder *dec);
int MPEG2_Get_coded_block_pattern (MPEG2_Decoder *dec);
int MPEG2_Get_macroblock_address_increment (MPEG2_Decoder *dec);
int MPEG2_Get_Luma_DC_dct_diff (MPEG2_Decoder *dec);
int MPEG2_Get_Chroma_DC_dct_diff (MPEG2_Decoder *dec);

/* idct.c */
void MPEG2_Fast_IDCT (MPEG2_Decoder *dec, short *block);
void MPEG2_Initialize_Fast_IDCT (MPEG2_Decoder *dec);

/* MPEG2_Reference_IDCT.c */
void MPEG2_Initialize_Reference_IDCT (MPEG2_Decoder *dec);
void MPEG2_Reference_IDCT (MPEG2_Decoder *dec, short *block);

/* motion.c */
void MPEG2_motion_vectors (MPEG2_Decoder *dec, int PMV[2][2][2], int dmvector[2],
                           int motion_vertical_field_select[2][2], int s, int motion_vector_count, 
                           int mv_format, int h_r_size, int v_r_size, int dmv, int mvscale);
void MPEG2_motion_vector (MPEG2_Decoder *dec, int *PMV, int *dmvector,
                          int h_r_size, int v_r_size, int dmv, int mvscale, int full_pel_vector);
void MPEG2_Dual_Prime_Arithmetic (MPEG2_Decoder *dec, int DMV[][2], int *dmvector, int mvx, int mvy);

/* mpeg2dec.c */
void MPEG2_Error (MPEG2_Decoder *dec, char *text);
void MPEG2_Warning (MPEG2_Decoder *dec, char *text);

/* recon.c */
void MPEG2_form_predictions (MPEG2_Decoder *dec, int bx, int by, int macroblock_type, 
                             int motion_type, int PMV[2][2][2], int motion_vertical_field_select[2][2], 
                             int dmvector[2], int stwtype);

/* store.c */
void MPEG2_Write_Frame (MPEG2_Decoder *dec, unsigned char *src[], int frame);

/* global variables */

/* zig-zag and alternate MPEG2_scan patterns */
extern const unsigned char MPEG2_scan[2][64];

/* non-linear quantization coefficient table */
extern const unsigned char MPEG2_Non_Linear_quantizer_scale[32];


/* decoder operation control flags */
extern int MPEG2_Quiet_Flag;
extern int MPEG2_Reference_IDCT_Flag;

