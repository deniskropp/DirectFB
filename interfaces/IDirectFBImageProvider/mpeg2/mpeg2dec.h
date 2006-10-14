/* mpeg2dec.h, MPEG specific defines                                        */

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

#include <dfb_types.h>

typedef int  (*MPEG2_Read)(void *buf, int count, void *ctx);
typedef void (*MPEG2_Write)(int x, int y, u32 argb, void *ctx);

typedef struct {
     int Fault_Flag;

     /* buffers for multiuse purposes */
     char Error_Text[256];
     unsigned char _Clip[1024];
     unsigned char *Clip;

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

     /* picture coding extension */
     int q_scale_type;
     int alternate_scan;

     /* slice/macroblock */
     int priority_breakpoint;
     int quantizer_scale;
     int intra_slice;
     short block[12][64];

     int global_MBA;
     int global_pic;
     int True_Framenum;

} MPEG2_Decoder;


MPEG2_Decoder *MPEG2_Init(MPEG2_Read mpeg2_read, void *read_ctx, int *width, int *height);
int  MPEG2_Decode(MPEG2_Decoder *dec, MPEG2_Write mpeg2_write, void *write_ctx);
void MPEG2_Close(MPEG2_Decoder *dec);


#define ERROR (-1)

#define PICTURE_START_CODE      0x100
#define SLICE_START_CODE_MIN    0x101
#define SLICE_START_CODE_MAX    0x1AF
#define USER_DATA_START_CODE    0x1B2
#define SEQUENCE_HEADER_CODE    0x1B3
#define SEQUENCE_ERROR_CODE     0x1B4
#define EXTENSION_START_CODE    0x1B5
#define SEQUENCE_END_CODE       0x1B7
#define GROUP_START_CODE        0x1B8
#define SYSTEM_START_CODE_MIN   0x1B9
#define SYSTEM_START_CODE_MAX   0x1FF

#define ISO_END_CODE            0x1B9
#define PACK_START_CODE         0x1BA
#define SYSTEM_START_CODE       0x1BB

#define VIDEO_ELEMENTARY_STREAM 0x1e0

/* scalable_mode */
#define SC_NONE 0
#define SC_DP   1
#define SC_SPAT 2
#define SC_SNR  3
#define SC_TEMP 4

/* picture coding type */
#define I_TYPE 1
#define P_TYPE 2
#define B_TYPE 3
#define D_TYPE 4

/* picture structure */
#define TOP_FIELD     1
#define BOTTOM_FIELD  2
#define FRAME_PICTURE 3

/* macroblock type */
#define MACROBLOCK_INTRA                        1
#define MACROBLOCK_PATTERN                      2
#define MACROBLOCK_MOTION_BACKWARD              4
#define MACROBLOCK_MOTION_FORWARD               8
#define MACROBLOCK_QUANT                        16
#define SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG       32
#define PERMITTED_SPATIAL_TEMPORAL_WEIGHT_CLASS 64


/* motion_type */
#define MC_FIELD 1
#define MC_FRAME 2
#define MC_16X8  2
#define MC_DMV   3

/* mv_format */
#define MV_FIELD 0
#define MV_FRAME 1

/* chroma_format */
#define CHROMA420 1
#define CHROMA422 2
#define CHROMA444 3

/* extension start code IDs */

#define SEQUENCE_EXTENSION_ID                    1
#define SEQUENCE_DISPLAY_EXTENSION_ID            2
#define QUANT_MATRIX_EXTENSION_ID                3
#define COPYRIGHT_EXTENSION_ID                   4
#define SEQUENCE_SCALABLE_EXTENSION_ID           5
#define PICTURE_DISPLAY_EXTENSION_ID             7
#define PICTURE_CODING_EXTENSION_ID              8
#define PICTURE_SPATIAL_SCALABLE_EXTENSION_ID    9
#define PICTURE_TEMPORAL_SCALABLE_EXTENSION_ID  10

#define ZIG_ZAG                                  0

#define PROFILE_422                             (128+5)
#define MAIN_LEVEL                              8

/* Layers: used by Verbose_Flag, Verifier_Flag, Stats_Flag, and Trace_Flag */
#define NO_LAYER                                0
#define SEQUENCE_LAYER                          1
#define PICTURE_LAYER                           2
#define SLICE_LAYER                             3
#define MACROBLOCK_LAYER                        4
#define BLOCK_LAYER                             5
#define EVENT_LAYER                             6
#define ALL_LAYERS                              7



#define FILENAME_LENGTH                       256




#define MB_WEIGHT                  32
#define MB_CLASS4                  64

