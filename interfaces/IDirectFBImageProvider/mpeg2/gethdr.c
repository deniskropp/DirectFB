/* gethdr.c, header decoding                                                */

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
static void sequence_header (MPEG2_Decoder *dec);
static void group_of_pictures_header (MPEG2_Decoder *dec);
static void picture_header (MPEG2_Decoder *dec);
static void extension_and_user_data (MPEG2_Decoder *dec);
static void sequence_extension (MPEG2_Decoder *dec);
static void sequence_display_extension (MPEG2_Decoder *dec);
static void quant_matrix_extension (MPEG2_Decoder *dec);
static void sequence_scalable_extension (MPEG2_Decoder *dec);
static void picture_display_extension (MPEG2_Decoder *dec);
static void picture_coding_extension (MPEG2_Decoder *dec);
static void picture_spatial_scalable_extension (MPEG2_Decoder *dec);
static void picture_temporal_scalable_extension (MPEG2_Decoder *dec);
static int  extra_bit_information (MPEG2_Decoder *dec);
static void copyright_extension (MPEG2_Decoder *dec);
static void user_data (MPEG2_Decoder *dec);
static void user_data (MPEG2_Decoder *dec);




#define RESERVED    -1 
static const double frame_rate_Table[16] =
{
     0.0,
     ((23.0*1000.0)/1001.0),
     24.0,
     25.0,
     ((30.0*1000.0)/1001.0),
     30.0,
     50.0,
     ((60.0*1000.0)/1001.0),
     60.0,

     RESERVED,
     RESERVED,
     RESERVED,
     RESERVED,
     RESERVED,
     RESERVED,
     RESERVED
};

/* default intra quantization matrix */
static const unsigned char default_intra_quantizer_matrix[64] = {
     8, 16, 19, 22, 26, 27, 29, 34,
     16, 16, 22, 24, 27, 29, 34, 37,
     19, 22, 26, 27, 29, 34, 34, 38,
     22, 22, 26, 27, 29, 34, 37, 40,
     22, 26, 27, 29, 32, 35, 40, 48,
     26, 27, 29, 32, 35, 40, 48, 58,
     26, 27, 29, 34, 38, 46, 56, 69,
     27, 29, 35, 38, 46, 56, 69, 83
};


/*
 * decode headers from one input stream
 * until an End of Sequence or picture start code
 * is found
 */
int
MPEG2_Get_Hdr(MPEG2_Decoder *dec)
{
     unsigned int code;

     for (;;) {
          /* look for MPEG2_next_start_code */
          MPEG2_next_start_code(dec);
          code = MPEG2_Get_Bits32(dec);

          switch (code) {
               case SEQUENCE_HEADER_CODE:
                    sequence_header(dec);
                    break;
               case GROUP_START_CODE:
                    group_of_pictures_header(dec);
                    break;
               case PICTURE_START_CODE:
                    picture_header(dec);
                    return 1;
                    break;
               case SEQUENCE_END_CODE:
                    return 0;
                    break;
               default:
                    if (!MPEG2_Quiet_Flag)
                         fprintf(stderr,"Unexpected MPEG2_next_start_code %08x (ignored)\n",code);
                    break;
          }
     }
}


/* align to start of next MPEG2_next_start_code */

void
MPEG2_next_start_code(MPEG2_Decoder *dec)
{
     /* byte align */
     MPEG2_Flush_Buffer(dec, dec->Incnt&7);
     while (MPEG2_Show_Bits(dec, 24)!=0x01L)
          MPEG2_Flush_Buffer(dec, 8);
}


/* decode sequence header */

static void
sequence_header(MPEG2_Decoder *dec)
{
     int i;
     int pos;

     pos = dec->Bitcnt;
     dec->horizontal_size             = MPEG2_Get_Bits(dec, 12);
     dec->vertical_size               = MPEG2_Get_Bits(dec, 12);
     dec->aspect_ratio_information    = MPEG2_Get_Bits(dec, 4);
     dec->frame_rate_code             = MPEG2_Get_Bits(dec, 4);
     dec->bit_rate_value              = MPEG2_Get_Bits(dec, 18);
     MPEG2_marker_bit(dec, "sequence_header()");
     dec->vbv_buffer_size             = MPEG2_Get_Bits(dec, 10);
     dec->constrained_parameters_flag = MPEG2_Get_Bits(dec, 1);

     if ((dec->load_intra_quantizer_matrix = MPEG2_Get_Bits(dec, 1))) {
          for (i=0; i<64; i++)
               dec->intra_quantizer_matrix[MPEG2_scan[ZIG_ZAG][i]] = MPEG2_Get_Bits(dec, 8);
     }
     else {
          for (i=0; i<64; i++)
               dec->intra_quantizer_matrix[i] = default_intra_quantizer_matrix[i];
     }

     if ((dec->load_non_intra_quantizer_matrix = MPEG2_Get_Bits(dec, 1))) {
          for (i=0; i<64; i++)
               dec->non_intra_quantizer_matrix[MPEG2_scan[ZIG_ZAG][i]] = MPEG2_Get_Bits(dec, 8);
     }
     else {
          for (i=0; i<64; i++)
               dec->non_intra_quantizer_matrix[i] = 16;
     }

     /* copy luminance to chrominance matrices */
     for (i=0; i<64; i++) {
          dec->chroma_intra_quantizer_matrix[i] =
          dec->intra_quantizer_matrix[i];

          dec->chroma_non_intra_quantizer_matrix[i] =
          dec->non_intra_quantizer_matrix[i];
     }

     extension_and_user_data(dec);
}



/* decode group of pictures header */
/* ISO/IEC 13818-2 section 6.2.2.6 */
static void
group_of_pictures_header(MPEG2_Decoder *dec)
{
     int pos;

     pos = dec->Bitcnt;
     dec->drop_flag   = MPEG2_Get_Bits(dec, 1);
     dec->hour        = MPEG2_Get_Bits(dec, 5);
     dec->minute      = MPEG2_Get_Bits(dec, 6);
     MPEG2_marker_bit(dec, "group_of_pictures_header()");
     dec->sec         = MPEG2_Get_Bits(dec, 6);
     dec->frame       = MPEG2_Get_Bits(dec, 6);
     dec->closed_gop  = MPEG2_Get_Bits(dec, 1);
     dec->broken_link = MPEG2_Get_Bits(dec, 1);

     extension_and_user_data(dec);
}


/* decode picture header */

/* ISO/IEC 13818-2 section 6.2.3 */
static void
picture_header(MPEG2_Decoder *dec)
{
     int pos;
     int Extra_Information_Byte_Count;

     pos = dec->Bitcnt;
     dec->temporal_reference  = MPEG2_Get_Bits(dec, 10);
     dec->picture_coding_type = MPEG2_Get_Bits(dec, 3);
     dec->vbv_delay           = MPEG2_Get_Bits(dec, 16);

     if (dec->picture_coding_type==P_TYPE || dec->picture_coding_type==B_TYPE) {
          dec->full_pel_forward_vector = MPEG2_Get_Bits(dec, 1);
          dec->forward_f_code = MPEG2_Get_Bits(dec, 3);
     }
     if (dec->picture_coding_type==B_TYPE) {
          dec->full_pel_backward_vector = MPEG2_Get_Bits(dec, 1);
          dec->backward_f_code = MPEG2_Get_Bits(dec, 3);
     }

     Extra_Information_Byte_Count = extra_bit_information(dec);

     extension_and_user_data(dec);
}

/* decode slice header */

/* ISO/IEC 13818-2 section 6.2.4 */
int
MPEG2_slice_header(MPEG2_Decoder *dec)
{
     int slice_vertical_position_extension;
     int quantizer_scale_code;
     int pos;
     int slice_picture_id_enable = 0;
     int slice_picture_id = 0;
     int extra_information_slice = 0;

     pos = dec->Bitcnt;

     slice_vertical_position_extension =
     (dec->MPEG2_Flag && dec->vertical_size>2800) ? MPEG2_Get_Bits(dec, 3) : 0;

     quantizer_scale_code = MPEG2_Get_Bits(dec, 5);
     dec->quantizer_scale =
     dec->MPEG2_Flag ? (dec->q_scale_type ? MPEG2_Non_Linear_quantizer_scale[quantizer_scale_code] : quantizer_scale_code<<1) : quantizer_scale_code;

     /* slice_id introduced in March 1995 as part of the video corridendum
        (after the IS was drafted in November 1994) */
     if (MPEG2_Get_Bits(dec, 1)) {
          dec->intra_slice = MPEG2_Get_Bits(dec, 1);

          slice_picture_id_enable = MPEG2_Get_Bits(dec, 1);
          slice_picture_id = MPEG2_Get_Bits(dec, 6);

          extra_information_slice = extra_bit_information(dec);
     }
     else
          dec->intra_slice = 0;


     return slice_vertical_position_extension;
}


/* decode extension and user data */
/* ISO/IEC 13818-2 section 6.2.2.2 */
static void
extension_and_user_data(MPEG2_Decoder *dec)
{
     int code,ext_ID;

     MPEG2_next_start_code(dec);

     while ((code = MPEG2_Show_Bits(dec, 32))==EXTENSION_START_CODE || code==USER_DATA_START_CODE) {
          if (code==EXTENSION_START_CODE) {
               MPEG2_Flush_Buffer32(dec);
               ext_ID = MPEG2_Get_Bits(dec, 4);
               switch (ext_ID) {
                    case SEQUENCE_EXTENSION_ID:
                         sequence_extension(dec);
                         break;
                    case SEQUENCE_DISPLAY_EXTENSION_ID:
                         sequence_display_extension(dec);
                         break;
                    case QUANT_MATRIX_EXTENSION_ID:
                         quant_matrix_extension(dec);
                         break;
                    case SEQUENCE_SCALABLE_EXTENSION_ID:
                         sequence_scalable_extension(dec);
                         break;
                    case PICTURE_DISPLAY_EXTENSION_ID:
                         picture_display_extension(dec);
                         break;
                    case PICTURE_CODING_EXTENSION_ID:
                         picture_coding_extension(dec);
                         break;
                    case PICTURE_SPATIAL_SCALABLE_EXTENSION_ID:
                         picture_spatial_scalable_extension(dec);
                         break;
                    case PICTURE_TEMPORAL_SCALABLE_EXTENSION_ID:
                         picture_temporal_scalable_extension(dec);
                         break;
                    case COPYRIGHT_EXTENSION_ID:
                         copyright_extension(dec);
                         break;
                    default:
                         fprintf(stderr,"reserved extension start code ID %d\n",ext_ID);
                         break;
               }
               MPEG2_next_start_code(dec);
          }
          else {
               MPEG2_Flush_Buffer32(dec);
               user_data(dec);
          }
     }
}


/* decode sequence extension */

/* ISO/IEC 13818-2 section 6.2.2.3 */
static void
sequence_extension(MPEG2_Decoder *dec)
{
     int horizontal_size_extension;
     int vertical_size_extension;
     int bit_rate_extension;
     int vbv_buffer_size_extension;

     /* derive bit position for trace */
     dec->MPEG2_Flag = 1;

     dec->profile_and_level_indication = MPEG2_Get_Bits(dec, 8);
     dec->progressive_sequence         = MPEG2_Get_Bits(dec, 1);
     dec->chroma_format                = MPEG2_Get_Bits(dec, 2);
     horizontal_size_extension    = MPEG2_Get_Bits(dec, 2);
     vertical_size_extension      = MPEG2_Get_Bits(dec, 2);
     bit_rate_extension           = MPEG2_Get_Bits(dec, 12);
     MPEG2_marker_bit(dec, "sequence_extension");
     vbv_buffer_size_extension    = MPEG2_Get_Bits(dec, 8);
     dec->low_delay                    = MPEG2_Get_Bits(dec, 1);
     dec->frame_rate_extension_n       = MPEG2_Get_Bits(dec, 2);
     dec->frame_rate_extension_d       = MPEG2_Get_Bits(dec, 5);

     dec->frame_rate = frame_rate_Table[dec->frame_rate_code] *
                  ((dec->frame_rate_extension_n+1)/(dec->frame_rate_extension_d+1));

     /* special case for 422 profile & level must be made */
     if ((dec->profile_and_level_indication>>7) & 1) {  /* escape bit of profile_and_level_indication set */

          /* 4:2:2 Profile @ Main Level */
          if ((dec->profile_and_level_indication&15)==5) {
               dec->profile = PROFILE_422;
               dec->level   = MAIN_LEVEL;  
          }
     }
     else {
          dec->profile = dec->profile_and_level_indication >> 4;  /* Profile is upper nibble */
          dec->level   = dec->profile_and_level_indication & 0xF;  /* Level is lower nibble */
     }


     dec->horizontal_size = (horizontal_size_extension<<12) | (dec->horizontal_size&0x0fff);
     dec->vertical_size = (vertical_size_extension<<12) | (dec->vertical_size&0x0fff);


     /* ISO/IEC 13818-2 does not define bit_rate_value to be composed of
      * both the original bit_rate_value parsed in sequence_header() and
      * the optional bit_rate_extension in sequence_extension_header(). 
      * However, we use it for bitstream verification purposes. 
      */

     dec->bit_rate_value += (bit_rate_extension << 18);
     dec->bit_rate = ((double) dec->bit_rate_value) * 400.0;
     dec->vbv_buffer_size += (vbv_buffer_size_extension << 10);
}


/* decode sequence display extension */

static void
sequence_display_extension(MPEG2_Decoder *dec)
{
     int pos;

     pos = dec->Bitcnt;
     dec->video_format      = MPEG2_Get_Bits(dec, 3);
     dec->color_description = MPEG2_Get_Bits(dec, 1);

     if (dec->color_description) {
          dec->color_primaries          = MPEG2_Get_Bits(dec, 8);
          dec->transfer_characteristics = MPEG2_Get_Bits(dec, 8);
          dec->matrix_coefficients      = MPEG2_Get_Bits(dec, 8);
     }

     dec->display_horizontal_size = MPEG2_Get_Bits(dec, 14);
     MPEG2_marker_bit(dec, "sequence_display_extension");
     dec->display_vertical_size   = MPEG2_Get_Bits(dec, 14);

}


/* decode quant matrix entension */
/* ISO/IEC 13818-2 section 6.2.3.2 */
static void
quant_matrix_extension(MPEG2_Decoder *dec)
{
     int i;
     int pos;

     pos = dec->Bitcnt;

     if ((dec->load_intra_quantizer_matrix = MPEG2_Get_Bits(dec, 1))) {
          for (i=0; i<64; i++) {
               dec->chroma_intra_quantizer_matrix[MPEG2_scan[ZIG_ZAG][i]]
               = dec->intra_quantizer_matrix[MPEG2_scan[ZIG_ZAG][i]]
                 = MPEG2_Get_Bits(dec, 8);
          }
     }

     if ((dec->load_non_intra_quantizer_matrix = MPEG2_Get_Bits(dec, 1))) {
          for (i=0; i<64; i++) {
               dec->chroma_non_intra_quantizer_matrix[MPEG2_scan[ZIG_ZAG][i]]
               = dec->non_intra_quantizer_matrix[MPEG2_scan[ZIG_ZAG][i]]
                 = MPEG2_Get_Bits(dec, 8);
          }
     }

     if ((dec->load_chroma_intra_quantizer_matrix = MPEG2_Get_Bits(dec, 1))) {
          for (i=0; i<64; i++)
               dec->chroma_intra_quantizer_matrix[MPEG2_scan[ZIG_ZAG][i]] = MPEG2_Get_Bits(dec, 8);
     }

     if ((dec->load_chroma_non_intra_quantizer_matrix = MPEG2_Get_Bits(dec, 1))) {
          for (i=0; i<64; i++)
               dec->chroma_non_intra_quantizer_matrix[MPEG2_scan[ZIG_ZAG][i]] = MPEG2_Get_Bits(dec, 8);
     }
}


/* decode sequence scalable extension */
/* ISO/IEC 13818-2   section 6.2.2.5 */
static void
sequence_scalable_extension(MPEG2_Decoder *dec)
{
     MPEG2_Error(dec, "scalability not implemented\n");
}


/* decode picture display extension */
/* ISO/IEC 13818-2 section 6.2.3.3. */
static void
picture_display_extension(MPEG2_Decoder *dec)
{
     int i;
     int number_of_frame_center_offsets;
     int pos;

     pos = dec->Bitcnt;
     /* based on ISO/IEC 13818-2 section 6.3.12 
       (November 1994) Picture display extensions */

     /* derive number_of_frame_center_offsets */
     if (dec->progressive_sequence) {
          if (dec->repeat_first_field) {
               if (dec->top_field_first)
                    number_of_frame_center_offsets = 3;
               else
                    number_of_frame_center_offsets = 2;
          }
          else {
               number_of_frame_center_offsets = 1;
          }
     }
     else {
          if (dec->picture_structure!=FRAME_PICTURE) {
               number_of_frame_center_offsets = 1;
          }
          else {
               if (dec->repeat_first_field)
                    number_of_frame_center_offsets = 3;
               else
                    number_of_frame_center_offsets = 2;
          }
     }


     /* now parse */
     for (i=0; i<number_of_frame_center_offsets; i++) {
          dec->frame_center_horizontal_offset[i] = MPEG2_Get_Bits(dec, 16);
          MPEG2_marker_bit(dec, "picture_display_extension, first marker bit");

          dec->frame_center_vertical_offset[i]   = MPEG2_Get_Bits(dec, 16);
          MPEG2_marker_bit(dec, "picture_display_extension, second marker bit");
     }
}


/* decode picture coding extension */
static void
picture_coding_extension(MPEG2_Decoder *dec)
{
     int pos;

     pos = dec->Bitcnt;

     dec->f_code[0][0] = MPEG2_Get_Bits(dec, 4);
     dec->f_code[0][1] = MPEG2_Get_Bits(dec, 4);
     dec->f_code[1][0] = MPEG2_Get_Bits(dec, 4);
     dec->f_code[1][1] = MPEG2_Get_Bits(dec, 4);

     dec->intra_dc_precision         = MPEG2_Get_Bits(dec, 2);
     dec->picture_structure          = MPEG2_Get_Bits(dec, 2);
     dec->top_field_first            = MPEG2_Get_Bits(dec, 1);
     dec->frame_pred_frame_dct       = MPEG2_Get_Bits(dec, 1);
     dec->concealment_motion_vectors = MPEG2_Get_Bits(dec, 1);
     dec->q_scale_type               = MPEG2_Get_Bits(dec, 1);
     dec->intra_vlc_format           = MPEG2_Get_Bits(dec, 1);
     dec->alternate_scan             = MPEG2_Get_Bits(dec, 1);
     dec->repeat_first_field         = MPEG2_Get_Bits(dec, 1);
     dec->chroma_420_type            = MPEG2_Get_Bits(dec, 1);
     dec->progressive_frame          = MPEG2_Get_Bits(dec, 1);
     dec->composite_display_flag     = MPEG2_Get_Bits(dec, 1);

     if (dec->composite_display_flag) {
          dec->v_axis            = MPEG2_Get_Bits(dec, 1);
          dec->field_sequence    = MPEG2_Get_Bits(dec, 3);
          dec->sub_carrier       = MPEG2_Get_Bits(dec, 1);
          dec->burst_amplitude   = MPEG2_Get_Bits(dec, 7);
          dec->sub_carrier_phase = MPEG2_Get_Bits(dec, 8);
     }
}


/* decode picture spatial scalable extension */
/* ISO/IEC 13818-2 section 6.2.3.5. */
static void
picture_spatial_scalable_extension(MPEG2_Decoder *dec)
{
     MPEG2_Error(dec, "picture spatial scalable extension not supported\n");
}


/* decode picture temporal scalable extension
 *
 * not implemented
 */
/* ISO/IEC 13818-2 section 6.2.3.4. */
static void
picture_temporal_scalable_extension(MPEG2_Decoder *dec)
{
     MPEG2_Error(dec, "temporal scalability not supported\n");
}


/* decode extra bit information */
/* ISO/IEC 13818-2 section 6.2.3.4. */
static int
extra_bit_information(MPEG2_Decoder *dec)
{
     int Byte_Count = 0;

     while (MPEG2_Get_Bits1(dec)) {
          MPEG2_Flush_Buffer(dec, 8);
          Byte_Count++;
     }

     return(Byte_Count);
}



/* ISO/IEC 13818-2 section 5.3 */
/* Purpose: this function is mainly designed to aid in bitstream conformance
   testing.  A simple MPEG2_Flush_Buffer(1) would do */
void
MPEG2_marker_bit(MPEG2_Decoder *dec, char *text)
{
     int marker;

     marker = MPEG2_Get_Bits(dec, 1);
}


/* ISO/IEC 13818-2  sections 6.3.4.1 and 6.2.2.2.2 */
static void
user_data(MPEG2_Decoder *dec)
{
     /* skip ahead to the next start code */
     MPEG2_next_start_code(dec);
}



/* Copyright extension */
/* ISO/IEC 13818-2 section 6.2.3.6. */
/* (header added in November, 1994 to the IS document) */


static void
copyright_extension(MPEG2_Decoder *dec)
{
     int pos;
     int reserved_data;

     pos = dec->Bitcnt;


     dec->copyright_flag =       MPEG2_Get_Bits(dec, 1); 
     dec->copyright_identifier = MPEG2_Get_Bits(dec, 8);
     dec->original_or_copy =     MPEG2_Get_Bits(dec, 1);

     /* reserved */
     reserved_data = MPEG2_Get_Bits(dec, 7);

     MPEG2_marker_bit(dec, "copyright_extension(), first marker bit");
     dec->copyright_number_1 =   MPEG2_Get_Bits(dec, 20);
     MPEG2_marker_bit(dec, "copyright_extension(), second marker bit");
     dec->copyright_number_2 =   MPEG2_Get_Bits(dec, 22);
     MPEG2_marker_bit(dec, "copyright_extension(), third marker bit");
     dec->copyright_number_3 =   MPEG2_Get_Bits(dec, 22);
}

