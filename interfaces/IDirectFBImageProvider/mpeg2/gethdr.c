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
static void sequence_header (void);
static void group_of_pictures_header (void);
static void picture_header (void);
static void extension_and_user_data (void);
static void sequence_extension (void);
static void sequence_display_extension (void);
static void quant_matrix_extension (void);
static void sequence_scalable_extension (void);
static void picture_display_extension (void);
static void picture_coding_extension (void);
static void picture_spatial_scalable_extension (void);
static void picture_temporal_scalable_extension (void);
static int  extra_bit_information (void);
static void copyright_extension (void);
static void user_data (void);
static void user_data (void);




/* introduced in September 1995 to assist spatial scalable decoding */
static void Update_Temporal_Reference_Tacking_Data (void);
/* private variables */
static int Temporal_Reference_Base = 0;
static int True_Framenum_max  = -1;
static int Temporal_Reference_GOP_Reset = 0;

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
int MPEG2_Get_Hdr()
{
  unsigned int code;

  for (;;)
  {
    /* look for MPEG2_next_start_code */
    MPEG2_next_start_code();
    code = MPEG2_Get_Bits32();
  
    switch (code)
    {
    case SEQUENCE_HEADER_CODE:
      sequence_header();
      break;
    case GROUP_START_CODE:
      group_of_pictures_header();
      break;
    case PICTURE_START_CODE:
      picture_header();
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

void MPEG2_next_start_code()
{
  /* byte align */
  MPEG2_Flush_Buffer(ld->Incnt&7);
  while (MPEG2_Show_Bits(24)!=0x01L)
    MPEG2_Flush_Buffer(8);
}


/* decode sequence header */

static void sequence_header()
{
  int i;
  int pos;

  pos = ld->Bitcnt;
  horizontal_size             = MPEG2_Get_Bits(12);
  vertical_size               = MPEG2_Get_Bits(12);
  aspect_ratio_information    = MPEG2_Get_Bits(4);
  frame_rate_code             = MPEG2_Get_Bits(4);
  bit_rate_value              = MPEG2_Get_Bits(18);
  MPEG2_marker_bit("sequence_header()");
  vbv_buffer_size             = MPEG2_Get_Bits(10);
  constrained_parameters_flag = MPEG2_Get_Bits(1);

  if((ld->load_intra_quantizer_matrix = MPEG2_Get_Bits(1)))
  {
    for (i=0; i<64; i++)
      ld->intra_quantizer_matrix[MPEG2_scan[ZIG_ZAG][i]] = MPEG2_Get_Bits(8);
  }
  else
  {
    for (i=0; i<64; i++)
      ld->intra_quantizer_matrix[i] = default_intra_quantizer_matrix[i];
  }

  if((ld->load_non_intra_quantizer_matrix = MPEG2_Get_Bits(1)))
  {
    for (i=0; i<64; i++)
      ld->non_intra_quantizer_matrix[MPEG2_scan[ZIG_ZAG][i]] = MPEG2_Get_Bits(8);
  }
  else
  {
    for (i=0; i<64; i++)
      ld->non_intra_quantizer_matrix[i] = 16;
  }

  /* copy luminance to chrominance matrices */
  for (i=0; i<64; i++)
  {
    ld->chroma_intra_quantizer_matrix[i] =
      ld->intra_quantizer_matrix[i];

    ld->chroma_non_intra_quantizer_matrix[i] =
      ld->non_intra_quantizer_matrix[i];
  }

  extension_and_user_data();
}



/* decode group of pictures header */
/* ISO/IEC 13818-2 section 6.2.2.6 */
static void group_of_pictures_header()
{
  int pos;

  if (ld == &base)
  {
    Temporal_Reference_Base = True_Framenum_max + 1; 	/* *CH* */
    Temporal_Reference_GOP_Reset = 1;
  }
  pos = ld->Bitcnt;
  drop_flag   = MPEG2_Get_Bits(1);
  hour        = MPEG2_Get_Bits(5);
  minute      = MPEG2_Get_Bits(6);
  MPEG2_marker_bit("group_of_pictures_header()");
  sec         = MPEG2_Get_Bits(6);
  frame       = MPEG2_Get_Bits(6);
  closed_gop  = MPEG2_Get_Bits(1);
  broken_link = MPEG2_Get_Bits(1);

  extension_and_user_data();

}


/* decode picture header */

/* ISO/IEC 13818-2 section 6.2.3 */
static void picture_header()
{
  int pos;
  int Extra_Information_Byte_Count;

  /* unless later overwritten by picture_spatial_scalable_extension() */
  ld->pict_scal = 0; 
  
  pos = ld->Bitcnt;
  temporal_reference  = MPEG2_Get_Bits(10);
  picture_coding_type = MPEG2_Get_Bits(3);
  vbv_delay           = MPEG2_Get_Bits(16);

  if (picture_coding_type==P_TYPE || picture_coding_type==B_TYPE)
  {
    full_pel_forward_vector = MPEG2_Get_Bits(1);
    forward_f_code = MPEG2_Get_Bits(3);
  }
  if (picture_coding_type==B_TYPE)
  {
    full_pel_backward_vector = MPEG2_Get_Bits(1);
    backward_f_code = MPEG2_Get_Bits(3);
  }

  Extra_Information_Byte_Count = 
    extra_bit_information();
  
  extension_and_user_data();

  /* update tracking information used to assist spatial scalability */
  Update_Temporal_Reference_Tacking_Data();
}

/* decode slice header */

/* ISO/IEC 13818-2 section 6.2.4 */
int MPEG2_slice_header()
{
  int slice_vertical_position_extension;
  int quantizer_scale_code;
  int pos;
  int slice_picture_id_enable = 0;
  int slice_picture_id = 0;
  int extra_information_slice = 0;

  pos = ld->Bitcnt;

  slice_vertical_position_extension =
    (ld->MPEG2_Flag && vertical_size>2800) ? MPEG2_Get_Bits(3) : 0;

  if (ld->scalable_mode==SC_DP)
    ld->priority_breakpoint = MPEG2_Get_Bits(7);

  quantizer_scale_code = MPEG2_Get_Bits(5);
  ld->quantizer_scale =
    ld->MPEG2_Flag ? (ld->q_scale_type ? MPEG2_Non_Linear_quantizer_scale[quantizer_scale_code] : quantizer_scale_code<<1) : quantizer_scale_code;

  /* slice_id introduced in March 1995 as part of the video corridendum
     (after the IS was drafted in November 1994) */
  if (MPEG2_Get_Bits(1))
  {
    ld->intra_slice = MPEG2_Get_Bits(1);

    slice_picture_id_enable = MPEG2_Get_Bits(1);
	slice_picture_id = MPEG2_Get_Bits(6);

    extra_information_slice = extra_bit_information();
  }
  else
    ld->intra_slice = 0;


  return slice_vertical_position_extension;
}


/* decode extension and user data */
/* ISO/IEC 13818-2 section 6.2.2.2 */
static void extension_and_user_data()
{
  int code,ext_ID;

  MPEG2_next_start_code();

  while ((code = MPEG2_Show_Bits(32))==EXTENSION_START_CODE || code==USER_DATA_START_CODE)
  {
    if (code==EXTENSION_START_CODE)
    {
      MPEG2_Flush_Buffer32();
      ext_ID = MPEG2_Get_Bits(4);
      switch (ext_ID)
      {
      case SEQUENCE_EXTENSION_ID:
        sequence_extension();
        break;
      case SEQUENCE_DISPLAY_EXTENSION_ID:
        sequence_display_extension();
        break;
      case QUANT_MATRIX_EXTENSION_ID:
        quant_matrix_extension();
        break;
      case SEQUENCE_SCALABLE_EXTENSION_ID:
        sequence_scalable_extension();
        break;
      case PICTURE_DISPLAY_EXTENSION_ID:
        picture_display_extension();
        break;
      case PICTURE_CODING_EXTENSION_ID:
        picture_coding_extension();
        break;
      case PICTURE_SPATIAL_SCALABLE_EXTENSION_ID:
        picture_spatial_scalable_extension();
        break;
      case PICTURE_TEMPORAL_SCALABLE_EXTENSION_ID:
        picture_temporal_scalable_extension();
        break;
      case COPYRIGHT_EXTENSION_ID:
        copyright_extension();
        break;
     default:
        fprintf(stderr,"reserved extension start code ID %d\n",ext_ID);
        break;
      }
      MPEG2_next_start_code();
    }
    else
    {
      MPEG2_Flush_Buffer32();
      user_data();
    }
  }
}


/* decode sequence extension */

/* ISO/IEC 13818-2 section 6.2.2.3 */
static void sequence_extension()
{
  int horizontal_size_extension;
  int vertical_size_extension;
  int bit_rate_extension;
  int vbv_buffer_size_extension;

  /* derive bit position for trace */
  ld->MPEG2_Flag = 1;

  ld->scalable_mode = SC_NONE; /* unless overwritten by sequence_scalable_extension() */
  layer_id = 0;                /* unless overwritten by sequence_scalable_extension() */
  
  profile_and_level_indication = MPEG2_Get_Bits(8);
  progressive_sequence         = MPEG2_Get_Bits(1);
  chroma_format                = MPEG2_Get_Bits(2);
  horizontal_size_extension    = MPEG2_Get_Bits(2);
  vertical_size_extension      = MPEG2_Get_Bits(2);
  bit_rate_extension           = MPEG2_Get_Bits(12);
  MPEG2_marker_bit("sequence_extension");
  vbv_buffer_size_extension    = MPEG2_Get_Bits(8);
  low_delay                    = MPEG2_Get_Bits(1);
  frame_rate_extension_n       = MPEG2_Get_Bits(2);
  frame_rate_extension_d       = MPEG2_Get_Bits(5);

  frame_rate = frame_rate_Table[frame_rate_code] *
    ((frame_rate_extension_n+1)/(frame_rate_extension_d+1));

  /* special case for 422 profile & level must be made */
  if((profile_and_level_indication>>7) & 1)
  {  /* escape bit of profile_and_level_indication set */
  
    /* 4:2:2 Profile @ Main Level */
    if((profile_and_level_indication&15)==5)
    {
      profile = PROFILE_422;
      level   = MAIN_LEVEL;  
    }
  }
  else
  {
    profile = profile_and_level_indication >> 4;  /* Profile is upper nibble */
    level   = profile_and_level_indication & 0xF;  /* Level is lower nibble */
  }
  
 
  horizontal_size = (horizontal_size_extension<<12) | (horizontal_size&0x0fff);
  vertical_size = (vertical_size_extension<<12) | (vertical_size&0x0fff);


  /* ISO/IEC 13818-2 does not define bit_rate_value to be composed of
   * both the original bit_rate_value parsed in sequence_header() and
   * the optional bit_rate_extension in sequence_extension_header(). 
   * However, we use it for bitstream verification purposes. 
   */

  bit_rate_value += (bit_rate_extension << 18);
  bit_rate = ((double) bit_rate_value) * 400.0;
  vbv_buffer_size += (vbv_buffer_size_extension << 10);
}


/* decode sequence display extension */

static void sequence_display_extension()
{
  int pos;

  pos = ld->Bitcnt;
  video_format      = MPEG2_Get_Bits(3);
  color_description = MPEG2_Get_Bits(1);

  if (color_description)
  {
    color_primaries          = MPEG2_Get_Bits(8);
    transfer_characteristics = MPEG2_Get_Bits(8);
    matrix_coefficients      = MPEG2_Get_Bits(8);
  }

  display_horizontal_size = MPEG2_Get_Bits(14);
  MPEG2_marker_bit("sequence_display_extension");
  display_vertical_size   = MPEG2_Get_Bits(14);

}


/* decode quant matrix entension */
/* ISO/IEC 13818-2 section 6.2.3.2 */
static void quant_matrix_extension()
{
  int i;
  int pos;

  pos = ld->Bitcnt;

  if((ld->load_intra_quantizer_matrix = MPEG2_Get_Bits(1)))
  {
    for (i=0; i<64; i++)
    {
      ld->chroma_intra_quantizer_matrix[MPEG2_scan[ZIG_ZAG][i]]
      = ld->intra_quantizer_matrix[MPEG2_scan[ZIG_ZAG][i]]
      = MPEG2_Get_Bits(8);
    }
  }

  if((ld->load_non_intra_quantizer_matrix = MPEG2_Get_Bits(1)))
  {
    for (i=0; i<64; i++)
    {
      ld->chroma_non_intra_quantizer_matrix[MPEG2_scan[ZIG_ZAG][i]]
      = ld->non_intra_quantizer_matrix[MPEG2_scan[ZIG_ZAG][i]]
      = MPEG2_Get_Bits(8);
    }
  }

  if((ld->load_chroma_intra_quantizer_matrix = MPEG2_Get_Bits(1)))
  {
    for (i=0; i<64; i++)
      ld->chroma_intra_quantizer_matrix[MPEG2_scan[ZIG_ZAG][i]] = MPEG2_Get_Bits(8);
  }

  if((ld->load_chroma_non_intra_quantizer_matrix = MPEG2_Get_Bits(1)))
  {
    for (i=0; i<64; i++)
      ld->chroma_non_intra_quantizer_matrix[MPEG2_scan[ZIG_ZAG][i]] = MPEG2_Get_Bits(8);
  }
}


/* decode sequence scalable extension */
/* ISO/IEC 13818-2   section 6.2.2.5 */
static void sequence_scalable_extension()
{
  int pos;

  pos = ld->Bitcnt;

  /* values (without the +1 offset) of scalable_mode are defined in 
     Table 6-10 of ISO/IEC 13818-2 */
  ld->scalable_mode = MPEG2_Get_Bits(2) + 1; /* add 1 to make SC_DP != SC_NONE */

  layer_id = MPEG2_Get_Bits(4);

  if (ld->scalable_mode==SC_SPAT)
  {
    lower_layer_prediction_horizontal_size = MPEG2_Get_Bits(14);
    MPEG2_marker_bit("sequence_scalable_extension()");
    lower_layer_prediction_vertical_size   = MPEG2_Get_Bits(14); 
    horizontal_subsampling_factor_m        = MPEG2_Get_Bits(5);
    horizontal_subsampling_factor_n        = MPEG2_Get_Bits(5);
    vertical_subsampling_factor_m          = MPEG2_Get_Bits(5);
    vertical_subsampling_factor_n          = MPEG2_Get_Bits(5);
  }

  if (ld->scalable_mode==SC_TEMP)
    MPEG2_Error("temporal scalability not implemented\n");
}


/* decode picture display extension */
/* ISO/IEC 13818-2 section 6.2.3.3. */
static void picture_display_extension()
{
  int i;
  int number_of_frame_center_offsets;
  int pos;

  pos = ld->Bitcnt;
  /* based on ISO/IEC 13818-2 section 6.3.12 
    (November 1994) Picture display extensions */

  /* derive number_of_frame_center_offsets */
  if(progressive_sequence)
  {
    if(repeat_first_field)
    {
      if(top_field_first)
        number_of_frame_center_offsets = 3;
      else
        number_of_frame_center_offsets = 2;
    }
    else
    {
      number_of_frame_center_offsets = 1;
    }
  }
  else
  {
    if(picture_structure!=FRAME_PICTURE)
    {
      number_of_frame_center_offsets = 1;
    }
    else
    {
      if(repeat_first_field)
        number_of_frame_center_offsets = 3;
      else
        number_of_frame_center_offsets = 2;
    }
  }


  /* now parse */
  for (i=0; i<number_of_frame_center_offsets; i++)
  {
    frame_center_horizontal_offset[i] = MPEG2_Get_Bits(16);
    MPEG2_marker_bit("picture_display_extension, first marker bit");
    
    frame_center_vertical_offset[i]   = MPEG2_Get_Bits(16);
    MPEG2_marker_bit("picture_display_extension, second marker bit");
  }
}


/* decode picture coding extension */
static void picture_coding_extension()
{
  int pos;

  pos = ld->Bitcnt;

  f_code[0][0] = MPEG2_Get_Bits(4);
  f_code[0][1] = MPEG2_Get_Bits(4);
  f_code[1][0] = MPEG2_Get_Bits(4);
  f_code[1][1] = MPEG2_Get_Bits(4);

  intra_dc_precision         = MPEG2_Get_Bits(2);
  picture_structure          = MPEG2_Get_Bits(2);
  top_field_first            = MPEG2_Get_Bits(1);
  frame_pred_frame_dct       = MPEG2_Get_Bits(1);
  concealment_motion_vectors = MPEG2_Get_Bits(1);
  ld->q_scale_type           = MPEG2_Get_Bits(1);
  intra_vlc_format           = MPEG2_Get_Bits(1);
  ld->alternate_scan         = MPEG2_Get_Bits(1);
  repeat_first_field         = MPEG2_Get_Bits(1);
  chroma_420_type            = MPEG2_Get_Bits(1);
  progressive_frame          = MPEG2_Get_Bits(1);
  composite_display_flag     = MPEG2_Get_Bits(1);

  if (composite_display_flag)
  {
    v_axis            = MPEG2_Get_Bits(1);
    field_sequence    = MPEG2_Get_Bits(3);
    sub_carrier       = MPEG2_Get_Bits(1);
    burst_amplitude   = MPEG2_Get_Bits(7);
    sub_carrier_phase = MPEG2_Get_Bits(8);
  }
}


/* decode picture spatial scalable extension */
/* ISO/IEC 13818-2 section 6.2.3.5. */
static void picture_spatial_scalable_extension()
{
  int pos;

  pos = ld->Bitcnt;

  ld->pict_scal = 1; /* use spatial scalability in this picture */

  lower_layer_temporal_reference = MPEG2_Get_Bits(10);
  MPEG2_marker_bit("picture_spatial_scalable_extension(), first marker bit");
  lower_layer_horizontal_offset = MPEG2_Get_Bits(15);
  if (lower_layer_horizontal_offset>=16384)
    lower_layer_horizontal_offset-= 32768;
  MPEG2_marker_bit("picture_spatial_scalable_extension(), second marker bit");
  lower_layer_vertical_offset = MPEG2_Get_Bits(15);
  if (lower_layer_vertical_offset>=16384)
    lower_layer_vertical_offset-= 32768;
  spatial_temporal_weight_code_table_index = MPEG2_Get_Bits(2);
  lower_layer_progressive_frame = MPEG2_Get_Bits(1);
  lower_layer_deinterlaced_field_select = MPEG2_Get_Bits(1);
}


/* decode picture temporal scalable extension
 *
 * not implemented
 */
/* ISO/IEC 13818-2 section 6.2.3.4. */
static void picture_temporal_scalable_extension()
{
  MPEG2_Error("temporal scalability not supported\n");
}


/* decode extra bit information */
/* ISO/IEC 13818-2 section 6.2.3.4. */
static int extra_bit_information()
{
  int Byte_Count = 0;

  while (MPEG2_Get_Bits1())
  {
    MPEG2_Flush_Buffer(8);
    Byte_Count++;
  }

  return(Byte_Count);
}



/* ISO/IEC 13818-2 section 5.3 */
/* Purpose: this function is mainly designed to aid in bitstream conformance
   testing.  A simple MPEG2_Flush_Buffer(1) would do */
void MPEG2_marker_bit(text)
char *text;
{
  int marker;

  marker = MPEG2_Get_Bits(1);
}


/* ISO/IEC 13818-2  sections 6.3.4.1 and 6.2.2.2.2 */
static void user_data()
{
  /* skip ahead to the next start code */
  MPEG2_next_start_code();
}



/* Copyright extension */
/* ISO/IEC 13818-2 section 6.2.3.6. */
/* (header added in November, 1994 to the IS document) */


static void copyright_extension()
{
  int pos;
  int reserved_data;

  pos = ld->Bitcnt;
  

  copyright_flag =       MPEG2_Get_Bits(1); 
  copyright_identifier = MPEG2_Get_Bits(8);
  original_or_copy =     MPEG2_Get_Bits(1);
  
  /* reserved */
  reserved_data = MPEG2_Get_Bits(7);

  MPEG2_marker_bit("copyright_extension(), first marker bit");
  copyright_number_1 =   MPEG2_Get_Bits(20);
  MPEG2_marker_bit("copyright_extension(), second marker bit");
  copyright_number_2 =   MPEG2_Get_Bits(22);
  MPEG2_marker_bit("copyright_extension(), third marker bit");
  copyright_number_3 =   MPEG2_Get_Bits(22);
}



/* introduced in September 1995 to assist Spatial Scalability */
static void Update_Temporal_Reference_Tacking_Data()
{
  static int temporal_reference_wrap  = 0;
  static int temporal_reference_old   = 0;

  if (ld == &base)			/* *CH* */
  {
    if (picture_coding_type!=B_TYPE && temporal_reference!=temporal_reference_old) 	
    /* check first field of */
    {							
       /* non-B-frame */
      if (temporal_reference_wrap) 		
      {/* wrap occured at previous I- or P-frame */	
       /* now all intervening B-frames which could 
          still have high temporal_reference values are done  */
        Temporal_Reference_Base += 1024;
	    temporal_reference_wrap = 0;
      }
      
      /* distinguish from a reset */
      if (temporal_reference<temporal_reference_old && !Temporal_Reference_GOP_Reset)	
	    temporal_reference_wrap = 1;  /* we must have just passed a GOP-Header! */
      
      temporal_reference_old = temporal_reference;
      Temporal_Reference_GOP_Reset = 0;
    }

    True_Framenum = Temporal_Reference_Base + temporal_reference;
    
    /* temporary wrap of TR at 1024 for M frames */
    if (temporal_reference_wrap && temporal_reference <= temporal_reference_old)	
      True_Framenum += 1024;				

    True_Framenum_max = (True_Framenum > True_Framenum_max) ?
                        True_Framenum : True_Framenum_max;
  }
}
