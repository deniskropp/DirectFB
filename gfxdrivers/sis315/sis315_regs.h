#ifndef _SIS315_REGS_H
#define _SIS315_REGS_H

#define PAT_REG_SIZE 384

enum sisfb_raster_op_bitblt {
	SIS315_ROP_CLEAR		= 0x00, /* dst =    0,            0    */
	SIS315_ROP_AND			= 0x88, /* dst =  dst &  src,     DSa  */
	SIS315_RON_AND_REVERSE		= 0x44, /* dst = ~dst &  src,     SDna */
	SIS315_ROP_COPY			= 0xCC, /* dst =  src,            S    */
	SIS315_ROP_AND_INVERTED		= 0x22, /* dst =  dst & ~src,     DSna */
	SIS315_ROP_NOOP			= 0xAA, /* dst =  dst,            D    */
	SIS315_ROP_XOR			= 0x66, /* dst =  dst ^  src,     DSx  */
	SIS315_ROP_OR			= 0xEE, /* dst =  dst |  src,     DSo  */
	SIS315_ROP_NOR			= 0x11, /* dst =  ~(dst | src),   DSon */
	SIS315_ROP_EQUIV		= 0x99, /* dst =  dst ^ ~src,     DSxn */
	SIS315_ROP_INVERT  		= 0x55, /* dst = ~dst,            Dn   */
	SIS315_ROP_OR_INVERSE		= 0xDD, /* dst = ~dst | src,      SDno */
	SIS315_ROP_COPY_INVERTED	= 0x33, /* dst = ~src,            Sn   */
	SIS315_ROP_OR_INVERTED		= 0xBB, /* dst = ~src | dst,      DSno */
	SIS315_ROP_NAND			= 0x77, /* dst = ~(dst & src),    DSan */
	SIS315_ROP_SET			= 0xFF, /* dst =    1,            1    */

	/* same as above, but with pattern as source */
	SIS315_ROP_CLEAR_PAT		= 0x00, /* dst =    0,            0    */
	SIS315_ROP_AND_PAT		= 0xA0, /* dst =  dst &  src,     DSa  */
	SIS315_RON_AND_REVERSE_PAT	= 0x50, /* dst = ~dst &  src,     SDna */
	SIS315_ROP_COPY_PAT		= 0xF0, /* dst =  src,            S    */
	SIS315_ROP_AND_INVERTED_PAT	= 0x0A, /* dst =  dst & ~src,     DSna */
	SIS315_ROP_NOOP_PAT		= 0xAA, /* dst =  dst,            D    */
	SIS315_ROP_XOR_PAT		= 0x5A, /* dst =  dst ^  src,     DSx  */
	SIS315_ROP_OR_PAT		= 0xFA, /* dst =  dst |  src,     DSo  */
	SIS315_ROP_NOR_PAT	       	= 0x05, /* dst =  ~(dst | src),   DSon */
	SIS315_ROP_EQUIV_PAT		= 0xA5, /* dst =  dst ^ ~src,     DSxn */
	SIS315_ROP_INVERT_PAT		= 0x55, /* dst = ~dst,            Dn   */
	SIS315_ROP_OR_REVERSE_PAT	= 0xDD, /* dst = ~dst | src,      SDno */
	SIS315_ROP_COPY_INVERTED_PAT	= 0x0F, /* dst = ~src,            Sn   */
	SIS315_ROP_OR_INVERTED_PAT	= 0xAF, /* dst = ~src | dst,      DSno */
	SIS315_ROP_NAND_PAT		= 0x5F, /* dst = ~(dst & src),    DSan */
	SIS315_ROP_SET_PAT		= 0xFF, /* dst =    1,            1    */
};

enum sisfb_raster_op_transparent_bitblt {
	SIS315_ROP_BLACK,
	SIS315_ROP_NOT_MERGE_PEN,
};

enum sis315_2d_registers {
	SIS315_2D_SRC_ADDR     = 0x8200,
	SIS315_2D_SRC_PITCH    = 0x8204, SIS315_2D_AGP_BASE     = 0x8206,
	SIS315_2D_SRC_Y        = 0x8208, SIS315_2D_SRC_X        = 0x820A,
	SIS315_2D_DST_Y        = 0x820C, SIS315_2D_DST_X        = 0x820E,
	SIS315_2D_DST_ADDR     = 0x8210,
	SIS315_2D_DST_PITCH    = 0x8214, SIS315_2D_DST_HEIGHT   = 0x8216,
	SIS315_2D_RECT_WIDTH   = 0x8218, SIS315_2D_RECT_HEIGHT  = 0x821A,
	SIS315_2D_PAT_FG_COLOR = 0x821C,
	SIS315_2D_PAT_BG_COLOR = 0x8220,
	SIS315_2D_SRC_FG_COLOR = 0x8224,
	SIS315_2D_SRC_BG_COLOR = 0x8228,
	SIS315_2D_MONO_MASK    = 0x822C,
	SIS315_2D_LEFT_CLIP    = 0x8234, SIS315_2D_TOP_CLIP     = 0x8236,
	SIS315_2D_RIGHT_CLIP   = 0x8238, SIS315_2D_BOT_CLIP     = 0x823A,
	SIS315_2D_CMD          = 0x823C,
	SIS315_2D_FIRE_TRIGGER = 0x8240,
	SIS315_2D_PATTERN_REG  = 0x8300
};

enum sis315_2d_registers_drawline {
	SIS315_2D_LINE_X0      = 0x8208, SIS315_2D_LINE_Y0           = 0x820A,
	SIS315_2D_LINE_X1      = 0x820C, SIS315_2D_LINE_Y1           = 0x820E,
	SIS315_2D_LINE_COUNT   = 0x8218, SIS315_2D_LINE_STYLE_PERIOD = 0x821A,
	SIS315_2D_LINE_STYLE_0 = 0x822C,
	SIS315_2D_LINE_STYLE_1 = 0x8230,
	SIS315_2D_LINE_Xn      = 0x8300, SIS315_2D_LINE_Yn           = 0x8302,
};

enum sis315_2d_register_transparent_bitblt {
	SIS315_2D_TRANS_DEST_KEY_HIGH = 0x821C,
	SIS315_2D_TRANS_DEST_KEY_LOW  = 0x8220,
	SIS315_2D_TRANS_SRC_KEY_HIGH  = 0x8224,
	SIS315_2D_TRANS_SRC_KEY_LOW   = 0x8228,
};

enum sis315_2d_cmd_type {
	SIS315_2D_CMD_BITBLT             = 0x00, SIS315_2D_CMD_COLOREXP           = 0x01,
	SIS315_2D_CMD_ENCOLOREXP         = 0x02, SIS315_2D_CMD_MULTIPLE_SCANLINE  = 0x03,
	SIS315_2D_CMD_LINE_DRAW          = 0x04, SIS315_2D_CMD_TRAPEZOID_FILL     = 0x05,
	SIS315_2D_CMD_TRANSPARENT_BITBLT = 0x06, SIS315_2D_CMD_ALPHA_BLENDING     = 0x07,
	SIS315_2D_CMD_3D_FUNCTION        = 0x08, SIS315_2D_CMD_CLEAR_Z_BUFFER     = 0x09,
       	SIS315_2D_CMD_GRADIENT_FILL      = 0x0A, SIS315_2D_CMD_STRETCH_BITBLT     = 0x0B
};

enum sis315_2d_cmd_control {
	SIS315_2D_CMD_SRC_VIDEO     = 0x00,
	SIS315_2D_CMD_SRC_SYSTEM    = 0x10,
	SIS315_2D_CMD_SRC_AGP       = 0x20,

	SIS315_2D_CMD_PAT_FG_REG    = 0x00,
	SIS315_2D_CMD_PAT_PAT_REG   = 0x40,
	SIS315_2D_CMD_PAT_MONO_MASK = 0x80,

	SIS315_2D_CMD_CFB_8         = 0x00000000,
	SIS315_2D_CMD_CFB_16        = 0x00010000,
	SIS315_2D_CMD_CFB_32        = 0x00020000,

	SIS315_2D_CMD_RECT_CLIP_EN  = 0x00040000,
	SIS315_2D_CMD_TRANSPARENT   = 0x00100000,

	/* Subfunction for Color/Enhanced Color Expansion */
	SIS315_2D_CMD_COLOR_TO_MONO = 0x00100000,
	SIS315_2D_CMD_AA_TEXT       = 0x00200000,

	SIS315_2D_CMD_MERGE_CLIP_DIS    = 0x04000000,

	SIS315_2D_CMD_LINE_STLYE_ENABLE = 0x00800000

#if 0
	SIS315_2D_CMD_DIR_X_INC     = 0x00010000,
	SIS315_2D_CMD_DIR_X_DEC     = 0x00000000,
	SIS315_2D_CMD_DIR_Y_INC     = 0x00020000,
	SIS315_2D_CMD_DIR_Y_DEC     = 0x00000000,
#endif
};

enum sis315_command_queue_registers {
	SIS315_2D_CMD_QUEUE_BASE_ADDRESS  = 0x85C0,
	SIS315_2D_CMD_QUEUE_WRITE_POINTER = 0x85C4,
	SIS315_2D_CMD_QUEUE_READ_POINTER  = 0x85C8,
	SIS315_2D_CMD_QUEUE_STATUS        = 0x85CC
};

#endif /* _SIS315_REGS_H */
