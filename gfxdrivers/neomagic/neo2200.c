/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include <asm/types.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <malloc.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/gfxcard.h>

#include <gfx/convert.h>

#include "neomagic.h"

typedef volatile struct {
  __u32 bltStat;
  __u32 bltCntl;
  __u32 xpColor;
  __u32 fgColor;
  __u32 bgColor;
  __u32 pitch;
  __u32 clipLT;
  __u32 clipRB;
  __u32 srcBitOffset;
  __u32 srcStart;
  __u32 reserved0;
  __u32 dstStart;
  __u32 xyExt;

  __u32 reserved1[19];

  __u32 pageCntl;
  __u32 pageBase;
  __u32 postBase;
  __u32 postPtr;
  __u32 dataPtr;
} Neo2200;

Neo2200 *neo2200 = NULL;


static inline void neo2200_waitidle()
{
  while (neo2200->bltStat & 1)
    neo_idle_waitcycles++;
}

static inline void neo2200_waitfifo( int requested_fifo_space )
{
  neo_waitfifo_calls++;
  neo_waitfifo_sum += requested_fifo_space;
     
  /* FIXME: does not work
  if (neo_fifo_space < requested_fifo_space)
    {
      neo_fifo_waitcycles++;

      while (1)
	{
	  neo_fifo_space = (neo2200->bltStat >> 8);
	  if (neo_fifo_space >= requested_fifo_space)
	    break;
	}
    }
  else
    {
      neo_fifo_cache_hits++;
    }

  neo_fifo_space -= requested_fifo_space;
  */

  neo2200_waitidle();
}


static int Neo2200_dstOrg = 0;
static int Neo2200_dstPitch = 0;
static int Neo2200_dstPixelWidth = 0;

static int Neo2200_srcOrg = 0;
static int Neo2200_srcPitch = 0;
static int Neo2200_srcPixelWidth = 0;


/* state validation */
static int neo2200_bltMode_dst = 0;
static int neo2200_src = 0;
static int neo2200_fgColor = 0;
static int neo2200_xpColor = 0;

     
static inline void neo2200_validate_bltMode_dst()
{
  int bltMode = 0;
  CoreSurface *destination = neo->state->destination;
  SurfaceBuffer *buffer = destination->back_buffer;

  if (neo2200_bltMode_dst)
    return;
     
  switch (destination->format)
    {
    case DSPF_A8:
      bltMode |= NEO_MODE1_DEPTH8;
      break;
    case DSPF_RGB15:
    case DSPF_RGB16:
      bltMode |= NEO_MODE1_DEPTH16;
      break;
    default:
      BUG( "destination format unsupported but was not rejected" );
    }

  Neo2200_dstOrg = buffer->video.offset;
  Neo2200_dstPitch = buffer->video.pitch;
  Neo2200_dstPixelWidth = BYTES_PER_PIXEL(destination->format);


  neo2200_waitfifo( 2 );

  neo2200->bltStat = bltMode << 16;
  neo2200->pitch = (Neo2200_dstPitch << 16) | (Neo2200_srcPitch & 0xffff);


  neo2200_bltMode_dst = 1;
}

static inline void neo2200_validate_src()
{
  CoreSurface *source = neo->state->source;
  SurfaceBuffer *buffer = source->front_buffer;

  if (neo2200_src)
    return;

  Neo2200_srcOrg = buffer->video.offset;
  Neo2200_srcPitch = buffer->video.pitch;
  Neo2200_srcPixelWidth = BYTES_PER_PIXEL(source->format);

  neo2200_waitfifo( 1 );
  neo2200->pitch = (Neo2200_dstPitch << 16) | (Neo2200_srcPitch & 0xffff);

  neo2200_src = 1;
}

static inline void neo2200_validate_fgColor()
{
  if (neo2200_fgColor)
    return;

  neo2200_waitfifo( 1 );

  switch (neo->state->destination->format)
    {
    case DSPF_A8:
      neo2200->fgColor = neo->state->color.a;
      break;
    case DSPF_RGB15:
      neo2200->fgColor = PIXEL_RGB15( neo->state->color.r,
				      neo->state->color.g,
				      neo->state->color.b );
      break;
    case DSPF_RGB16:
      neo2200->fgColor = PIXEL_RGB16( neo->state->color.r,
				      neo->state->color.g,
				      neo->state->color.b );
      break;
    default:
      BUG( "destination format unsupported but was not rejected" );
    }

  neo2200_fgColor = 1;
}

static inline void neo2200_validate_xpColor()
{
  if (neo2200_xpColor)
    return;

  neo2200_waitfifo( 1 );

  neo2200->xpColor = neo->state->src_colorkey;
  
  neo2200_xpColor = 1;
}


/* required implementations */

static void neo2200EngineSync()
{
  neo2200_waitidle( mmio_base );
}

#define NEO_SUPPORTED_DRAWINGFLAGS \
               (DSDRAW_NOFX)

#define NEO_SUPPORTED_DRAWINGFUNCTIONS \
               (DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE)

#define NEO_SUPPORTED_BLITTINGFLAGS \
               (DSBLIT_SRC_COLORKEY)

#define NEO_SUPPORTED_BLITTINGFUNCTIONS \
               (DFXL_BLIT)

static void neo2200CheckState( CardState *state, DFBAccelerationMask accel )
{
  switch (state->destination->format)
    {
    case DSPF_A8:
    case DSPF_RGB15:
    case DSPF_RGB16:
      break;
    default:
      return;
    }

  /* if there are no other drawing flags than the supported */
  if (!(state->drawingflags & ~NEO_SUPPORTED_DRAWINGFLAGS))
    state->accel |= NEO_SUPPORTED_DRAWINGFUNCTIONS;

  /* if there are no other blitting flags than the supported
     and the source and destination formats are the same */
  if (!(state->blittingflags & ~NEO_SUPPORTED_BLITTINGFLAGS)  &&
      state->source  &&  state->source->format == state->destination->format  &&
      state->source->front_buffer != state->destination->back_buffer)
    state->accel |= NEO_SUPPORTED_BLITTINGFUNCTIONS;
}

static void neo2200SetState( CardState *state, DFBAccelerationMask accel )
{
  if (state != neo->state)
    {
      state->modified |= SMF_ALL;
      state->set = 0;
      neo->state = state;
     
      neo2200_xpColor = neo2200_fgColor = neo2200_bltMode_dst = neo2200_src = 0;
    }
  else
    {
      if (state->modified & SMF_DESTINATION)
	neo2200_fgColor = neo2200_bltMode_dst = 0;
      else
	if (state->modified & SMF_COLOR)
	  neo2200_fgColor = 0;

      if (state->modified & SMF_SOURCE)
	neo2200_src = 0;

      if (state->modified & SMF_SRC_COLORKEY)
	neo2200_xpColor = 0;
    }
          
  switch (accel)
    {
    case DFXL_BLIT:
      neo2200_validate_src();
      if (state->blittingflags & DSBLIT_SRC_COLORKEY)
	neo2200_validate_xpColor();

    case DFXL_FILLRECTANGLE:
    case DFXL_DRAWRECTANGLE:
      neo2200_validate_fgColor();
      neo2200_validate_bltMode_dst();
      
      state->set |= DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE | DFXL_BLIT;
      break;

    default:
      break;
    }

  state->modified = 0;
}

static void neo2200FillRectangle( DFBRectangle *rect )
{
  neo2200_waitfifo( 3 );

  /* set blt control */
  neo2200->bltCntl = NEO_BC3_FIFO_EN      |
                     NEO_BC0_SRC_IS_FG    |
                     NEO_BC3_SKIP_MAPPING |  0x0c0000;

  neo2200->dstStart = Neo2200_dstOrg +
           (rect->y * Neo2200_dstPitch) +
           (rect->x * Neo2200_dstPixelWidth);

  neo2200->xyExt    = (rect->h << 16) | (rect->w & 0xffff);
}

static void neo2200DrawRectangle( DFBRectangle *rect )
{
  __u32 dst = Neo2200_dstOrg +
             (rect->y * Neo2200_dstPitch) +
             (rect->x * Neo2200_dstPixelWidth);

  neo2200_waitfifo( 3 );

  /* set blt control */
  neo2200->bltCntl = NEO_BC3_FIFO_EN      |
                     NEO_BC0_SRC_IS_FG    |
                     NEO_BC3_SKIP_MAPPING | 0x0c0000;

  neo2200->dstStart = dst;
  neo2200->xyExt    = (1 << 16) | (rect->w & 0xffff);


  dst += (rect->h - 1) * Neo2200_dstPitch;
  neo2200_waitfifo( 2 );
  neo2200->dstStart = dst;
  neo2200->xyExt    = (1 << 16) | (rect->w & 0xffff);


  dst -= (rect->h - 2) * Neo2200_dstPitch;
  neo2200_waitfifo( 2 );
  neo2200->dstStart = dst;
  neo2200->xyExt    = ((rect->h - 2) << 16) | 1;


  dst += (rect->w - 1) * Neo2200_dstPixelWidth;
  neo2200_waitfifo( 2 );
  neo2200->dstStart = dst;
  neo2200->xyExt    = ((rect->h - 2) << 16) | 1;
}

static void neo2200Blit( DFBRectangle *rect, int dx, int dy )
{
  __u32 bltCntl = (neo->state->blittingflags & DSBLIT_SRC_COLORKEY) ? NEO_BC0_SRC_TRANS : 0;

  __u32 start;

  /* shit, doesn't work */
#if 0
  if (rect->x < dx)
    bltCntl |= NEO_BC0_X_DEC;

  if (rect->y < dy) {
    bltCntl |= NEO_BC0_DST_Y_DEC | NEO_BC0_SRC_Y_DEC;

    rect->y += rect->h - 1;
    dy += rect->h - 1;
  }
#endif

  start = rect->y * Neo2200_srcPitch + rect->x * Neo2200_srcPixelWidth;

#if 0
  if (bltCntl & NEO_BC0_X_DEC)
    start += (rect->w-1) * Neo2200_srcPixelWidth;
#endif

  neo2200_waitfifo( 3 );

  /* set blt control */
  neo2200->bltCntl = bltCntl |
                     NEO_BC3_FIFO_EN      |
                     NEO_BC3_SKIP_MAPPING |  0x0c0000;

  neo2200->srcStart = Neo2200_srcOrg + start;

  neo2200->dstStart =  Neo2200_dstOrg +
                 (dy * Neo2200_dstPitch) +
                 (dx * Neo2200_dstPixelWidth);

  neo2200->xyExt = (rect->h << 16) | (rect->w & 0xffff);
}

DFBResult neo2200_init( GfxCard *card )
{
  //  int i;
  neo2200 = (Neo2200*) mmio_base;

  //  printf( "reserved0: %#0x\n", neo2200->reserved0 );

  //  for (i=0; i<19; i++)
  //    printf( "reserved1[%02d]: %#0x\n", i, neo2200->reserved1[i] );

  strcat( card->info.driver_name, " (2200/2230/2360/2380)" );

  card->caps.flags    = 0;
  card->caps.accel    = NEO_SUPPORTED_DRAWINGFUNCTIONS |
                        NEO_SUPPORTED_BLITTINGFUNCTIONS;
  card->caps.drawing  = NEO_SUPPORTED_DRAWINGFLAGS;
  card->caps.blitting = NEO_SUPPORTED_BLITTINGFLAGS;

  card->CheckState = neo2200CheckState;
  card->SetState = neo2200SetState;
  card->EngineSync = neo2200EngineSync;          
  
  card->FillRectangle = neo2200FillRectangle;
  card->DrawRectangle = neo2200DrawRectangle;
  //     card->DrawLine = neoDrawLine2D;
  card->Blit = neo2200Blit;
  //     card->StretchBlit = neoStretchBlit;

  card->byteoffset_align = 32 * 4;
  card->pixelpitch_align = 32;

  return DFB_OK;
}
