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

/* choose between declaration (GLOBAL undefined)
 * and definition (GLOBAL defined)
 * GLOBAL is defined in exactly one file mpeg2dec.c)
 */

/* prototypes of global functions */

/* Get_Bits.c */
void Initialize_Buffer (void);
void Fill_Buffer (void);
unsigned int Show_Bits (int n);
unsigned int Get_Bits1 (void);
void Flush_Buffer (int n);
unsigned int Get_Bits (int n);

/* systems.c */
void Flush_Buffer32 (void);
unsigned int Get_Bits32 (void);

/* getblk.c */
void Decode_MPEG1_Intra_Block (int comp, int dc_dct_pred[]);
void Decode_MPEG1_Non_Intra_Block (int comp);
void Decode_MPEG2_Intra_Block (int comp, int dc_dct_pred[]);
void Decode_MPEG2_Non_Intra_Block (int comp);

/* gethdr.c */
int Get_Hdr (void);
void next_start_code (void);
int slice_header (void);
void marker_bit (char *text);

/* getpic.c */
void Decode_Picture (int bitstream_framenum, 
  int sequence_framenum);
void Output_Last_Frame_of_Sequence (int framenum);

/* getvlc.c */
int Get_macroblock_type (void);
int Get_motion_code (void);
int Get_dmvector (void);
int Get_coded_block_pattern (void);
int Get_macroblock_address_increment (void);
int Get_Luma_DC_dct_diff (void);
int Get_Chroma_DC_dct_diff (void);

/* idct.c */
void Fast_IDCT (short *block);
void Initialize_Fast_IDCT (void);

/* Reference_IDCT.c */
void Initialize_Reference_IDCT (void);
void Reference_IDCT (short *block);

/* motion.c */
void motion_vectors (int PMV[2][2][2], int dmvector[2],
  int motion_vertical_field_select[2][2], int s, int motion_vector_count, 
  int mv_format, int h_r_size, int v_r_size, int dmv, int mvscale);
void motion_vector (int *PMV, int *dmvector,
  int h_r_size, int v_r_size, int dmv, int mvscale, int full_pel_vector);
void Dual_Prime_Arithmetic (int DMV[][2], int *dmvector, int mvx, int mvy);

/* mpeg2dec.c */
void Error (char *text);
void Warning (char *text);

/* recon.c */
void form_predictions (int bx, int by, int macroblock_type, 
  int motion_type, int PMV[2][2][2], int motion_vertical_field_select[2][2], 
  int dmvector[2], int stwtype);

/* store.c */
void Write_Frame (unsigned char *src[], int frame);

/* global variables */

/* zig-zag and alternate MPEG2_scan patterns */
const unsigned char MPEG2_scan[2][64]
#ifdef GLOBAL
=
{
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
}
#endif
;

/* non-linear quantization coefficient table */
const unsigned char MPEG2_Non_Linear_quantizer_scale[32]
#ifdef GLOBAL
=
{
   0, 1, 2, 3, 4, 5, 6, 7,
   8,10,12,14,16,18,20,22,
  24,28,32,36,40,44,48,52,
  56,64,72,80,88,96,104,112
}
#endif
;



/* decoder operation control flags */
int MPEG2_Quiet_Flag;
int MPEG2_Fault_Flag;
int MPEG2_Reference_IDCT_Flag;


/* buffers for multiuse purposes */
char MPEG2_Error_Text[256];
unsigned char *MPEG2_Clip;

/* pointers to generic picture buffers */
unsigned char *backward_reference_frame[3];
unsigned char *forward_reference_frame[3];

unsigned char *auxframe[3];
unsigned char *current_frame[3];




/* non-normative variables derived from normative elements */
int Coded_Picture_Width;
int Coded_Picture_Height;
int Chroma_Width;
int Chroma_Height;
int block_count;
int Second_Field;
int profile, level;

/* normative derived variables (as per ISO/IEC 13818-2) */
int horizontal_size;
int vertical_size;
int mb_width;
int mb_height;
double bit_rate;
double frame_rate; 



/* headers */

/* ISO/IEC 13818-2 section 6.2.2.1:  sequence_header() */
int aspect_ratio_information;
int frame_rate_code; 
int bit_rate_value; 
int vbv_buffer_size;
int constrained_parameters_flag;

/* ISO/IEC 13818-2 section 6.2.2.3:  sequence_extension() */
int profile_and_level_indication;
int progressive_sequence;
int chroma_format;
int low_delay;
int frame_rate_extension_n;
int frame_rate_extension_d;

/* ISO/IEC 13818-2 section 6.2.2.4:  sequence_display_extension() */
int video_format;  
int color_description;
int color_primaries;
int transfer_characteristics;
int matrix_coefficients;
int display_horizontal_size;
int display_vertical_size;

/* ISO/IEC 13818-2 section 6.2.3: picture_header() */
int temporal_reference;
int picture_coding_type;
int vbv_delay;
int full_pel_forward_vector;
int forward_f_code;
int full_pel_backward_vector;
int backward_f_code;


/* ISO/IEC 13818-2 section 6.2.3.1: picture_coding_extension() header */
int f_code[2][2];
int intra_dc_precision;
int picture_structure;
int top_field_first;
int frame_pred_frame_dct;
int concealment_motion_vectors;

int intra_vlc_format;

int repeat_first_field;

int chroma_420_type;
int progressive_frame;
int composite_display_flag;
int v_axis;
int field_sequence;
int sub_carrier;
int burst_amplitude;
int sub_carrier_phase;



/* ISO/IEC 13818-2 section 6.2.3.3: picture_display_extension() header */
int frame_center_horizontal_offset[3];
int frame_center_vertical_offset[3];



/* ISO/IEC 13818-2 section 6.2.2.5: sequence_scalable_extension() header */
int layer_id;
int lower_layer_prediction_horizontal_size;
int lower_layer_prediction_vertical_size;
int horizontal_subsampling_factor_m;
int horizontal_subsampling_factor_n;
int vertical_subsampling_factor_m;
int vertical_subsampling_factor_n;


/* ISO/IEC 13818-2 section 6.2.3.5: picture_spatial_scalable_extension() header */
int lower_layer_temporal_reference;
int lower_layer_horizontal_offset;
int lower_layer_vertical_offset;
int spatial_temporal_weight_code_table_index;
int lower_layer_progressive_frame;
int lower_layer_deinterlaced_field_select;






/* ISO/IEC 13818-2 section 6.2.3.6: copyright_extension() header */
int copyright_flag;
int copyright_identifier;
int original_or_copy;
int copyright_number_1;
int copyright_number_2;
int copyright_number_3;

/* ISO/IEC 13818-2 section 6.2.2.6: group_of_pictures_header()  */
int drop_flag;
int hour;
int minute;
int sec;
int frame;
int closed_gop;
int broken_link;



/* layer specific variables (needed for SNR and DP scalability) */
struct layer_data {
  /* bit input */
  MPEG2_Read   mpeg2_read;
  void        *mpeg2_read_ctx;

  MPEG2_Write  mpeg2_write;
  void        *mpeg2_write_ctx;

  unsigned char Rdbfr[2048];
  unsigned char *Rdptr;
  unsigned char Inbfr[16];
  /* from mpeg2play */
  unsigned int Bfr;
  unsigned char *Rdmax;
  int Incnt;
  int Bitcnt;
  /* sequence header and quant_matrix_extension() */
  int intra_quantizer_matrix[64];
  int non_intra_quantizer_matrix[64];
  int chroma_intra_quantizer_matrix[64];
  int chroma_non_intra_quantizer_matrix[64];
  
  int load_intra_quantizer_matrix;
  int load_non_intra_quantizer_matrix;
  int load_chroma_intra_quantizer_matrix;
  int load_chroma_non_intra_quantizer_matrix;

  int MPEG2_Flag;
  /* sequence scalable extension */
  int scalable_mode;
  /* picture coding extension */
  int q_scale_type;
  int alternate_scan;
  /* picture spatial scalable extension */
  int pict_scal;
  /* slice/macroblock */
  int priority_breakpoint;
  int quantizer_scale;
  int intra_slice;
  short block[12][64];
} base, enhan, *ld;


int global_MBA;
int global_pic;
int True_Framenum;
