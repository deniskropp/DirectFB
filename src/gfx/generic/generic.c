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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <asm/types.h>

#include <pthread.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/gfxcard.h>
#include <misc/gfx_util.h>
#include <misc/util.h>
#include <misc/conf.h>
#include <gfx/convert.h>
#include <gfx/util.h>

#include <config.h>

#include "generic.h"

/*
 * state values
 */
static void *dst_org = NULL;
static void *src_org = NULL;
static int dst_pitch = 0;
static int src_pitch = 0;

static int dst_bpp   = 0;
static int src_bpp   = 0;

DFBColor color;

/*
 * operands
 */
static void *Aop = NULL;
static void *Bop = NULL;
static __u32 Cop = 0;

/*
 * color keys
 */
static __u32 Dkey = 0;
static __u32 Skey = 0;

/*
 * accumulators
 */
static Accumulator Aacc[ACC_WIDTH]; // FIXME: dynamically
static Accumulator Bacc[ACC_WIDTH]; // FIXME: dynamically
Accumulator Cacc;

/*
 * operations
 */
static GFunc gfuncs[32];

/*
 * dataflow control
 */
Accumulator *Xacc = NULL;
Accumulator *Dacc = NULL;
Accumulator *Sacc = NULL;
void        *Dop  = NULL;
void        *Sop  = NULL;

/* controls horizontal blitting direction */
int Ostep = 0;

int Dlength = 0;
int SperD = 0; /* for scaled routines only */


static pthread_mutex_t generic_lock = PTHREAD_MUTEX_INITIALIZER;

static int source_locked = 0;

/********************************* Cop_to_Dop_PFI *****************************/
static void Cop_to_Dop_8()
{
     memset( Dop, (__u8)Cop, Dlength );
}

static void Cop_to_Dop_16()
{
     int    w = Dlength;
     __u16 *D = (__u16*)Dop;

     while (w--)
          *D++ = (__u16)Cop;
}

static void Cop_to_Dop_24()
{
     int   w = Dlength;
     __u8 *D = (__u8*)Dop;

     while (w--) {
          *D++ = color.b;
          *D++ = color.g;
          *D++ = color.r;
     }
}

static void Cop_to_Dop_32()
{
     int    w = Dlength;
     __u32 *D = (__u32*)Dop;

     while (w--)
          *D++ = (__u32)Cop;
}

static GFunc Cop_to_Dop_PFI[] = {
     Cop_to_Dop_16,
     Cop_to_Dop_16,
     Cop_to_Dop_24,
     Cop_to_Dop_32,
     Cop_to_Dop_32,
     Cop_to_Dop_8
};

/********************************* Cop_toK_Dop_PFI ****************************/

static void Cop_toK_Dop_8()
{
     int   w = Dlength;
     __u8 *D = (__u8*)Dop;

     while (w--) {
          if ((__u8)Dkey == *D)
               *D = (__u8)Cop;

          D++;
     }
}

static void Cop_toK_Dop_16()
{
     int    w = Dlength;
     __u16 *D = (__u16*)Dop;

     while (w--) {
          if ((__u16)Dkey == *D)
               *D = (__u16)Cop;

          D++;
     }
}

static void Cop_toK_Dop_24()
{
     ONCE("Cop_toK_Dop_24() unimplemented");
}

static void Cop_toK_Dop_32()
{
     int    w = Dlength;
     __u32 *D = (__u32*)Dop;

     while (w--) {
          if ((__u32)Dkey != *D)
               *D = (__u32)Cop;

          D++;
     }
}

static GFunc Cop_toK_Dop_PFI[] = {
     Cop_toK_Dop_16,
     Cop_toK_Dop_16,
     Cop_toK_Dop_24,
     Cop_toK_Dop_32,
     Cop_toK_Dop_32,
     Cop_toK_Dop_8
};

/********************************* Sop_PFI_to_Dop_PFI *************************/

static void Sop_8_to_Dop()
{
     memmove( Dop, Sop, Dlength );
}

static void Sop_16_to_Dop()
{

     memmove( Dop, Sop, Dlength*2 );
}

static void Sop_24_to_Dop()
{
     memmove( Dop, Sop, Dlength*3 );
}

static void Sop_32_to_Dop()
{
     memmove( Dop, Sop, Dlength*4 );
}

static GFunc Sop_PFI_to_Dop_PFI[] = {
     Sop_16_to_Dop,
     Sop_16_to_Dop,
     Sop_24_to_Dop,
     Sop_32_to_Dop,
     Sop_32_to_Dop,
     Sop_8_to_Dop
};

/********************************* Sop_PFI_Kto_Dop_PFI ************************/

static void Sop_rgb15_Kto_Dop()
{
     int    w = Dlength;
     __u16 *D = (__u16*)Dop;
     __u16 *S = (__u16*)Sop;

     if (Ostep < 0) {
          D+= Dlength - 1;
          S+= Dlength - 1;
     }

     while (w--) {
          __u16 spixel = *S & 0x7FFF;

          if (spixel != (__u16)Skey)
               *D = spixel;

          S+=Ostep;
          D+=Ostep;
     }
}

static void Sop_rgb16_Kto_Dop()
{
     int    w = Dlength;
     __u16 *D = (__u16*)Dop;
     __u16 *S = (__u16*)Sop;

     if (Ostep < 0) {
          D+= Dlength - 1;
          S+= Dlength - 1;
     }

     while (w--) {
          __u16 spixel = *S;

          if (spixel != (__u16)Skey)
               *D = spixel;

          S+=Ostep;
          D+=Ostep;
     }
}

static void Sop_rgb24_Kto_Dop()
{
     int    w = Dlength;
     __u8 *D = (__u8*)Dop;
     __u8 *S = (__u8*)Sop;

     if (Ostep < 0) {
          D+= (Dlength - 1) * 3;
          S+= (Dlength - 1) * 3;
     }

     while (w--) {
          __u8 b = *S;
          __u8 g = *(S+1);
          __u8 r = *(S+2);
          
          if (Skey != (r<<16 | g<<8 | b ))
          {
               *D     = b;
               *(D+1) = g;
               *(D+2) = r;
          }

          S+=Ostep * 3;
          D+=Ostep * 3 ;
     }     
}

static void Sop_rgb32_Kto_Dop()
{
     int    w = Dlength;
     __u32 *D = (__u32*)Dop;
     __u32 *S = (__u32*)Sop;

     if (Ostep < 0) {
          D+= Dlength - 1;
          S+= Dlength - 1;
     }

     while (w--) {
          __u32 spixel = *S & 0x00FFFFFF;

          if (spixel != Skey)
               *D = spixel;

          S+=Ostep;
          D+=Ostep;
     }
}

static void Sop_argb_Kto_Dop()
{
     int    w = Dlength;
     __u32 *D = (__u32*)Dop;
     __u32 *S = (__u32*)Sop;

     if (Ostep < 0) {
          D+= Dlength - 1;
          S+= Dlength - 1;
     }

     while (w--) {
          __u32 spixel = *S;

          if (spixel != Skey)
               *D = spixel;

          S+=Ostep;
          D+=Ostep;
     }
}

static void Sop_a8_Kto_Dop()
{
     ONCE( "Sop_a8_Kto_Dop() unimplemented");
}

static GFunc Sop_PFI_Kto_Dop_PFI[] = {
     Sop_rgb15_Kto_Dop,
     Sop_rgb16_Kto_Dop,
     Sop_rgb24_Kto_Dop,     
     Sop_rgb32_Kto_Dop,
     Sop_argb_Kto_Dop,
     Sop_a8_Kto_Dop
};

/********************************* Sop_PFI_Sto_Dop ****************************/

static void Sop_16_Sto_Dop()
{
     int    w = Dlength;
     int    i = 0;
     __u16 *D = (__u16*)Dop;
     __u16 *S = (__u16*)Sop;

     while (w--) {
          *D++ = S[i>>16];

          i += SperD;
     }
}

static void Sop_24_Sto_Dop()
{
     int    w = Dlength;
     int    i = 0;
     __u8 *D = (__u8*)Dop;
     __u8 *S = (__u8*)Sop;

     while (w--) {
          int pixelstart = (i>>16)*3;

          *D++ = S[pixelstart+0];
          *D++ = S[pixelstart+1];
          *D++ = S[pixelstart+2];

          i += SperD;
     }
}

static void Sop_32_Sto_Dop()
{
     int    w = Dlength;
     int    i = 0;
     __u32 *D = (__u32*)Dop;
     __u32 *S = (__u32*)Sop;

     while (w--) {
          *D++ = S[i>>16];

          i += SperD;
     }
}

static void Sop_8_Sto_Dop()
{
     ONCE( "Sop_8_Sto_Dop() unimplemented");
}

static GFunc Sop_PFI_Sto_Dop[] = {
     Sop_16_Sto_Dop,
     Sop_16_Sto_Dop,
     Sop_24_Sto_Dop,
     Sop_32_Sto_Dop,
     Sop_32_Sto_Dop,
     Sop_8_Sto_Dop
};

/********************************* Sop_PFI_SKto_Dop ***************************/

static void Sop_rgb15_SKto_Dop()
{
     int    w = Dlength;
     int    i = 0;
     __u16 *D = (__u16*)Dop;
     __u16 *S = (__u16*)Sop;

     while (w--) {
          __u16 s = S[i>>16] & 0x7FFF;

          if (s != Skey)
               *D = s;

          D++;
          i += SperD;
     }
}

static void Sop_rgb16_SKto_Dop()
{
     int    w = Dlength;
     int    i = 0;
     __u16 *D = (__u16*)Dop;
     __u16 *S = (__u16*)Sop;

     while (w--) {
          __u16 s = S[i>>16];

          if (s != Skey)
               *D = s;

          D++;
          i += SperD;
     }
}

static void Sop_rgb24_SKto_Dop()
{
     int    w = Dlength;
     int    i = 0;
     __u8 *D = (__u8*)Dop;
     __u8 *S = (__u8*)Sop;
     
     while (w--) {
          int pixelstart = (i>>16)*3;
          
          __u8 b = S[pixelstart+0];
          __u8 g = S[pixelstart+1];
          __u8 r = S[pixelstart+2];
     
          if (Skey != (r<<16 | g<<8 | b ))
          {
               *D     = b;
               *(D+1) = g;
               *(D+2) = r;
          }

          D += 3;     
          i += SperD;
     }
}

static void Sop_rgb32_SKto_Dop()
{
     int    w = Dlength;
     int    i = 0;
     __u32 *D = (__u32*)Dop;
     __u32 *S = (__u32*)Sop;

     while (w--) {
          __u32 s = S[i>>16] & 0x00FFFFFF;

          if (s != Skey)
               *D = s;

          D++;
          i += SperD;
     }
}

static void Sop_argb_SKto_Dop()
{
     int    w = Dlength;
     int    i = 0;
     __u32 *D = (__u32*)Dop;
     __u32 *S = (__u32*)Sop;

     while (w--) {
          __u32 s = S[i>>16];

          if (s != Skey)
               *D = s;

          D++;
          i += SperD;
     }
}

static void Sop_a8_SKto_Dop()
{
     ONCE( "Sop_a8_SKto_Dop() unimplemented" );
}

static GFunc Sop_PFI_SKto_Dop[] = {
     Sop_rgb15_SKto_Dop,
     Sop_rgb16_SKto_Dop,
     Sop_rgb24_SKto_Dop,
     Sop_rgb32_SKto_Dop,
     Sop_argb_SKto_Dop,
     Sop_a8_SKto_Dop
};

/********************************* Sop_PFI_Sto_Dacc ***************************/

#ifdef USE_MMX
void Sop_argb_Sto_Dacc_MMX();
#endif

static void Sop_rgb15_Sto_Dacc()
{
     int    w = Dlength;
     int    i = 0;

     Accumulator *D = Dacc;
     __u16       *S = (__u16*)Sop;

     while (w--) {
          __u16 s = S[i>>16];

          D->a = 0xFF;
          D->r = (s & 0x7C00) >> 7;
          D->g = (s & 0x03E0) >> 2;
          D->b = (s & 0x001F) << 3;

          i += SperD;

          D++;
     }
}

static void Sop_rgb16_Sto_Dacc()
{
     int    w = Dlength;
     int    i = 0;

     Accumulator *D = Dacc;
     __u16       *S = (__u16*)Sop;

     while (w--) {
          __u16 s = S[i>>16];

          D->a = 0xFF;
          D->r = (s & 0xF800) >> 8;
          D->g = (s & 0x07E0) >> 3;
          D->b = (s & 0x001F) << 3;

          i += SperD;

          D++;
     }
}

static void Sop_rgb24_Sto_Dacc()
{
     int    w = Dlength;
     int    i = 0;

     Accumulator *D = Dacc;
     __u8        *S = (__u8*)Sop;

     while (w--) {
          int pixelstart = (i>>16)*3;
               
          D->a = 0xFF;
          D->r = S[pixelstart+2];
          D->g = S[pixelstart+1];
          D->b = S[pixelstart+0];

          i += SperD;

          D++;
     }
}

static void Sop_rgb32_Sto_Dacc()
{
     int    w = Dlength;
     int    i = 0;

     Accumulator *D = Dacc;
     __u32       *S = (__u32*)Sop;

     while (w--) {
          __u32 s = S[i>>16];

          D->a = 0xFF;
          D->r = (s & 0x00FF0000) >> 16;
          D->g = (s & 0x0000FF00) >>  8;
          D->b = (s & 0x000000FF);

          i += SperD;

          D++;
     }
}


static void Sop_argb_Sto_Dacc()
{
     int    w = Dlength;
     int    i = 0;

     Accumulator *D = Dacc;
     __u32       *S = (__u32*)Sop;

     while (w--) {
          __u32 s = S[i>>16];

          D->a = (s & 0xFF000000) >> 24;
          D->r = (s & 0x00FF0000) >> 16;
          D->g = (s & 0x0000FF00) >>  8;
          D->b = (s & 0x000000FF);

          i += SperD;

          D++;
     }
}

static void Sop_a8_Sto_Dacc()
{
     ONCE( "Sop_a8_Sto_Dacc() unimplemented" );
}

static GFunc Sop_PFI_Sto_Dacc[] = {
     Sop_rgb15_Sto_Dacc,
     Sop_rgb16_Sto_Dacc,
     Sop_rgb24_Sto_Dacc,
     Sop_rgb32_Sto_Dacc,
     Sop_argb_Sto_Dacc,
     Sop_a8_Sto_Dacc
};

/********************************* Sop_PFI_SKto_Dacc **************************/

static void Sop_rgb15_SKto_Dacc()
{
     int    w = Dlength;
     int    i = 0;

     Accumulator *D = Dacc;
     __u16       *S = (__u16*)Sop;

     while (w--) {
          __u16 s = S[i>>16] & 0x7FFF;

          if (s != Skey) {
               D->a = 0xFF;
               D->r = (s & 0x7C00) >> 7;
               D->g = (s & 0x03E0) >> 2;
               D->b = (s & 0x001F) << 3;
          }
          else
               D->a = 0xF000;

          i += SperD;

          D++;
     }
}

static void Sop_rgb16_SKto_Dacc()
{
     int    w = Dlength;
     int    i = 0;

     Accumulator *D = Dacc;
     __u16       *S = (__u16*)Sop;

     while (w--) {
          __u16 s = S[i>>16];

          if (s != Skey) {
               D->a = 0xFF;
               D->r = (s & 0xF800) >> 8;
               D->g = (s & 0x07E0) >> 3;
               D->b = (s & 0x001F) << 3;
          }
          else
               D->a = 0xF000;

          i += SperD;

          D++;
     }
}

static void Sop_rgb24_SKto_Dacc()
{
     int    w = Dlength;
     int    i = 0;
     
     Accumulator *D = Dacc;
     __u8        *S = (__u8*)Sop;
     
     while (w--) {
          int pixelstart = (i>>16)*3;

          __u8 b = S[pixelstart+0];
          __u8 g = S[pixelstart+1];
          __u8 r = S[pixelstart+2];

          if (Skey != (r<<16 | g<<8 | b ))
          {
               D->a = 0xFF;
               D->r = r; 
               D->g = g;
               D->b = b;
          }
          else
               D->a = 0xFF00;

          i += SperD;
     
          D++;
     }

     //ONCE( "Sop_rgb24_SKto_Dacc() unimplemented");
}

static void Sop_rgb32_SKto_Dacc()
{
     int    w = Dlength;
     int    i = 0;

     Accumulator *D = Dacc;
     __u32       *S = (__u32*)Sop;

     while (w--) {
          __u32 s = S[i>>16] & 0x00FFFFFF;

          if (s != Skey) {
               D->a = 0xFF;
               D->r = (s & 0x00FF0000) >> 16;
               D->g = (s & 0x0000FF00) >>  8;
               D->b = (s & 0x000000FF);
          }
          else
               D->a = 0xF000;

          i += SperD;

          D++;
     }
}


static void Sop_argb_SKto_Dacc()
{
     int    w = Dlength;
     int    i = 0;

     Accumulator *D = Dacc;
     __u32       *S = (__u32*)Sop;

     while (w--) {
          __u32 s = S[i>>16];

          if (s != Skey) {
               D->a = (s & 0xFF000000) >> 24;
               D->r = (s & 0x00FF0000) >> 16;
               D->g = (s & 0x0000FF00) >>  8;
               D->b = (s & 0x000000FF);
          }
          else
               D->a = 0xF000;

          i += SperD;

          D++;
     }
}

static void Sop_a8_SKto_Dacc()
{
     ONCE( "Sop_a8_SKto_Dacc() unimplemented");
}

static GFunc Sop_PFI_SKto_Dacc[] = {
     Sop_rgb15_SKto_Dacc,
     Sop_rgb16_SKto_Dacc,
     Sop_rgb24_SKto_Dacc,
     Sop_rgb32_SKto_Dacc,
     Sop_argb_SKto_Dacc,
     Sop_a8_SKto_Dacc
};

/********************************* Sop_PFI_to_Dacc ****************************/

#ifdef USE_MMX
void Sop_rgb16_to_Dacc_MMX();
void Sop_rgb32_to_Dacc_MMX();
void Sop_argb_to_Dacc_MMX();
#endif


static void Sop_rgb15_to_Dacc()
{
     int          w = Dlength;
     Accumulator *D = Dacc;
     __u16       *S = (__u16*)Sop;

     while (w--) {
          __u16 spixel = *S++;

          D->a = 0xFF;
          D->r = (spixel & 0x7C00) >> 7;
          D->g = (spixel & 0x03E0) >> 2;
          D->b = (spixel & 0x001F) << 3;

          D++;
     }
}

static void Sop_rgb16_to_Dacc()
{
     int          w = Dlength;
     Accumulator *D = Dacc;
     __u16       *S = (__u16*)Sop;

     while (w--) {
          __u16 spixel = *S++;

          D->a = 0xFF;
          D->r = (spixel & 0xF800) >> 8;
          D->g = (spixel & 0x07E0) >> 3;
          D->b = (spixel & 0x001F) << 3;

          D++;
     }
}

static void Sop_rgb24_to_Dacc()
{
     int          w = Dlength;
     Accumulator *D = Dacc;
     __u8        *S = (__u8*)Sop;

     while (w--) {
          D->a = 0xFF;
          D->b = *S++;
          D->g = *S++;
          D->r = *S++;

          D++;
     }
}

static void Sop_a8_to_Dacc()
{
     int          w = Dlength;
     Accumulator *D = Dacc;
     __u8        *S = (__u8*)Sop;

     while (w--) {
          D->a = *S++;
          D->r = 0xFF;
          D->g = 0xFF;
          D->b = 0xFF;

          D++;
     }
}

static void Sop_a1_to_Dacc()
{
     int          w = Dlength;
     Accumulator *D = Dacc;
     __u8        *S = (__u8*)Sop;

     while (w--) {
          D->a = *S++;
          D->r = 0xFF;
          D->g = 0xFF;
          D->b = 0xFF;

          D++;
     }
}

static void Sop_rgb32_to_Dacc()
{
     int          w = Dlength;
     Accumulator *D = Dacc;
     __u32       *S = (__u32*)Sop;

     while (w--) {
          __u32 s = *S++;

          D->a = 0xFF;
          D->r = (s & 0xFF0000) >> 16;
          D->g = (s & 0x00FF00) >>  8;
          D->b = (s & 0x0000FF);

          D++;
     }
}

static void Sop_argb_to_Dacc()
{
     int          w = Dlength;
     Accumulator *D = Dacc;
     __u32       *S = (__u32*)Sop;

     while (w--) {
          __u32 s = *S++;

          D->a = (s & 0xFF000000) >> 24;
          D->r = (s & 0x00FF0000) >> 16;
          D->g = (s & 0x0000FF00) >>  8;
          D->b = (s & 0x000000FF);

          D++;
     }
}

static GFunc Sop_PFI_to_Dacc[] = {
     Sop_rgb15_to_Dacc,
     Sop_rgb16_to_Dacc,
     Sop_rgb24_to_Dacc,
     Sop_rgb32_to_Dacc,
     Sop_argb_to_Dacc,
     Sop_a8_to_Dacc,
     Sop_a1_to_Dacc
};

/********************************* Sop_PFI_Kto_Dacc ***************************/

static void Sop_rgb15_Kto_Dacc()
{
     int          w = Dlength;
     Accumulator *D = Dacc;
     __u16       *S = (__u16*)Sop;

     while (w--) {
          __u16 s = *S++ & 0x7FFF;

          if (s != Skey) {
               D->a = 0xFF;
               D->r = (s & 0x7C00) >> 7;
               D->g = (s & 0x03E0) >> 2;
               D->b = (s & 0x001F) << 3;
          }
          else
               D->a = 0xF000;

          D++;
     }
}

static void Sop_rgb16_Kto_Dacc()
{
     int          w = Dlength;
     Accumulator *D = Dacc;
     __u16       *S = (__u16*)Sop;

     while (w--) {
          __u16 s = *S++;

          if (s != Skey) {
               D->a = 0xFF;
               D->r = (s & 0xF800) >> 8;
               D->g = (s & 0x07E0) >> 3;
               D->b = (s & 0x001F) << 3;
          }
          else
               D->a = 0xF000;

          D++;
     }
}

static void Sop_rgb24_Kto_Dacc()
{
     int          w = Dlength;
     Accumulator *D = Dacc;
     __u8        *S = (__u8*)Sop;

     while (w--) {
          __u8 b = *S++;
          __u8 g = *S++;
          __u8 r = *S++;
     
          if (Skey != (r<<16 | g<<8 | b ))
          {                    
               D->a = 0xFF;
               D->r = r;
               D->g = g;
               D->b = b;
          }
          else
               D->a = 0xF000;

          D++;
     }
}
     
static void Sop_rgb32_Kto_Dacc()
{
     int          w = Dlength;
     Accumulator *D = Dacc;
     __u32       *S = (__u32*)Sop;

     while (w--) {
          __u32 s = *S++ & 0x00FFFFFF;

          if (s != Skey) {
               D->a = 0xFF;
               D->r = (s & 0xFF0000) >> 16;
               D->g = (s & 0x00FF00) >>  8;
               D->b = (s & 0x0000FF);
          }
          else
               D->a = 0xF000;

          D++;
     }
}

static void Sop_argb_Kto_Dacc()
{
     int          w = Dlength;
     Accumulator *D = Dacc;
     __u32       *S = (__u32*)Sop;

     while (w--) {
          __u32 s = *S++;

          if (s != Skey) {
               D->a = (s & 0xFF000000) >> 24;
               D->r = (s & 0x00FF0000) >> 16;
               D->g = (s & 0x0000FF00) >>  8;
               D->b = (s & 0x000000FF);
          }
          else
               D->a = 0xF000;

          D++;
     }
}

static void Sop_a8_Kto_Dacc()
{
     ONCE( "Sop_a8_Kto_Dacc() unimplemented" );
}

static GFunc Sop_PFI_Kto_Dacc[] = {
     Sop_rgb15_Kto_Dacc,
     Sop_rgb16_Kto_Dacc,
     Sop_rgb24_Kto_Dacc,
     Sop_rgb32_Kto_Dacc,
     Sop_argb_Kto_Dacc,
     Sop_a8_Kto_Dacc
};

/********************************* Sacc_to_Dop_PFI ****************************/

#ifdef USE_MMX
void Sacc_to_Dop_rgb16_MMX();
void Sacc_to_Dop_rgb32_MMX();
#endif

static void Sacc_to_Dop_rgb15()
{
     int          w = Dlength;
     Accumulator *S = Sacc;
     __u16       *D = (__u16*)Dop;

     while (w--) {
          if (!(S->a & 0xF000)) {
               *D = PIXEL_RGB15( (S->r & 0xFF00) ? 0xFF : S->r,
                                 (S->g & 0xFF00) ? 0xFF : S->g,
                                 (S->b & 0xFF00) ? 0xFF : S->b );
          }

          D++;
          S++;
     }
}

static void Sacc_to_Dop_rgb16()
{
     int          w = Dlength;
     Accumulator *S = Sacc;
     __u16       *D = (__u16*)Dop;

     while (w--) {
          if (!(S->a & 0xF000)) {
               *D = PIXEL_RGB16( (S->r & 0xFF00) ? 0xFF : S->r,
                                 (S->g & 0xFF00) ? 0xFF : S->g,
                                 (S->b & 0xFF00) ? 0xFF : S->b );
          }

          D++;
          S++;
     }
}

static void Sacc_to_Dop_rgb24()
{
     int          w = Dlength;
     Accumulator *S = Sacc;
     __u8        *D = (__u8*)Dop;

     while (w--) {
          if (!(S->a & 0xF000)) {
               *D++ = (S->b & 0xFF00) ? 0xFF : S->b;
               *D++ = (S->g & 0xFF00) ? 0xFF : S->g;
               *D++ = (S->r & 0xFF00) ? 0xFF : S->r;
          }
          else
               D += 3;

          S++;
     }
}

static void Sacc_to_Dop_rgb32()
{
     int          w = Dlength;
     Accumulator *S = Sacc;
     __u32       *D = (__u32*)Dop;

     while (w--) {
          if (!(S->a & 0xF000)) {
               *D = PIXEL_RGB32( (S->r & 0xFF00) ? 0xFF : S->r,
                                 (S->g & 0xFF00) ? 0xFF : S->g,
                                 (S->b & 0xFF00) ? 0xFF : S->b );
          }

          D++;
          S++;
     }
}

static void Sacc_to_Dop_argb()
{
     int          w = Dlength;
     Accumulator *S = Sacc;
     __u32       *D = (__u32*)Dop;

     while (w--) {
          if (!(S->a & 0xF000)) {
               *D = PIXEL_ARGB( (S->a & 0xFF00) ? 0xFF : S->a,
                                (S->r & 0xFF00) ? 0xFF : S->r,
                                (S->g & 0xFF00) ? 0xFF : S->g,
                                (S->b & 0xFF00) ? 0xFF : S->b );
          }

          D++;
          S++;
     }
}

static void Sacc_to_Dop_a8()
{
     ONCE( "Sacc_to_Dop_a8() unimplemented" );
}

GFunc Sacc_to_Dop_PFI[] = {
     Sacc_to_Dop_rgb15,
     Sacc_to_Dop_rgb16,
     Sacc_to_Dop_rgb24,
     Sacc_to_Dop_rgb32,
     Sacc_to_Dop_argb,
     Sacc_to_Dop_a8
};

/************** Sop_a8_set_alphapixel_Dop_PFI *********************************/

static void Sop_a8_set_alphapixel_Dop_rgb15()
{
     int    w = Dlength;
     __u8  *S = Sop;
     __u16 *D = (__u16*)Dop;

     while (w--) {
          SET_ALPHA_PIXEL_RGB15( D, color.r, color.g, color.b, *S );

          D++;
          S++;
     }
}

static void Sop_a8_set_alphapixel_Dop_rgb16()
{
     int    w = Dlength;
     __u8  *S = Sop;
     __u16 *D = (__u16*)Dop;

     while (w--) {
          SET_ALPHA_PIXEL_RGB16( D, color.r, color.g, color.b, *S );

          D++;
          S++;
     }
}

static void Sop_a8_set_alphapixel_Dop_rgb24()
{
     int    w = Dlength;
     __u8  *S = Sop;
     __u8  *D = (__u8*)Dop;

     while (w--) {
          SET_ALPHA_PIXEL_RGB24( D, color.r, color.g, color.b, *S );

          D+=3;
          S++;
     }
          
}

static void Sop_a8_set_alphapixel_Dop_rgb32()
{
     int    w = Dlength;
     __u8  *S = Sop;
     __u32 *D = (__u32*)Dop;

     while (w--) {
          SET_ALPHA_PIXEL_RGB32( D, color.r, color.g, color.b, *S );

          D++;
          S++;
     }
}

static void Sop_a8_set_alphapixel_Dop_argb()
{
     int    w = Dlength;
     __u8  *S = Sop;
     __u32 *D = (__u32*)Dop;

     while (w--) {
          SET_ALPHA_PIXEL_ARGB( D, color.r, color.g, color.b, *S );

          D++;
          S++;
     }
}

static void Sop_a8_set_alphapixel_Dop_a8()
{
     ONCE( "Sop_a8_set_alphapixel_Dop_a8() unimplemented" );
}

GFunc Sop_a8_set_alphapixel_Dop_PFI[] = {
     Sop_a8_set_alphapixel_Dop_rgb15,
     Sop_a8_set_alphapixel_Dop_rgb16,
     Sop_a8_set_alphapixel_Dop_rgb24,
     Sop_a8_set_alphapixel_Dop_rgb32,
     Sop_a8_set_alphapixel_Dop_argb,
     Sop_a8_set_alphapixel_Dop_a8
};

/********************************* Xacc_blend *********************************/

#ifdef USE_MMX
void Xacc_blend_srcalpha_MMX();
void Xacc_blend_invsrcalpha_MMX();
#endif

static void Xacc_blend_zero()
{
     ONCE("should not be called, please optimize, developer!");

     memset( Xacc, 0, sizeof(Accumulator) * Dlength );
}

static void Xacc_blend_one()
{
     ONCE("should not be called, please optimize, developer!");
}

static void Xacc_blend_srccolor() 
{
     ONCE( "Xacc_blend_srccolor() unimplemented" );
}

static void Xacc_blend_invsrccolor()
{
     ONCE( "Xacc_blend_invsrccolor() unimplemented" );
}
     
static void Xacc_blend_srcalpha()
{
     int          w = Dlength;
     Accumulator *X = Xacc;

     if (Sacc) {
          Accumulator *S = Sacc;

          while (w--) {
               if (!(X->a & 0xF000)) {
                    __u16 Sa = S->a + 1;

                    X->r = (Sa * X->r) >> 8;
                    X->g = (Sa * X->g) >> 8;
                    X->b = (Sa * X->b) >> 8;
                    X->a = (Sa * X->a) >> 8;
               }

               X++;
               S++;
          }
     }
     else {
          __u16 Sa = color.a + 1;

          while (w--) {
               if (!(X->a & 0xF000)) {
                    X->r = (Sa * X->r) >> 8;
                    X->g = (Sa * X->g) >> 8;
                    X->b = (Sa * X->b) >> 8;
                    X->a = (Sa * X->a) >> 8;
               }

               X++;
          }
     }
}



static void Xacc_blend_invsrcalpha()
{
     int          w = Dlength;
     Accumulator *X = Xacc;

     if (Sacc) {
          Accumulator *S = Sacc;

          while (w--) {
               if (!(X->a & 0xF000)) {
                    __u16 Sa = 0x100 - S->a;

                    X->r = (Sa * X->r) >> 8;
                    X->g = (Sa * X->g) >> 8;
                    X->b = (Sa * X->b) >> 8;
                    X->a = (Sa * X->a) >> 8;
               }

               X++;
               S++;
          }
     }
     else {
          __u16 Sa = 0x100 - color.a;

          while (w--) {
               if (!(X->a & 0xF000)) {
                    X->a = (Sa * X->a) >> 8;
                    X->r = (Sa * X->r) >> 8;
                    X->g = (Sa * X->g) >> 8;
                    X->b = (Sa * X->b) >> 8;
               }

               X++;
          }
     }
}

static void Xacc_blend_dstalpha()
{
     int          w = Dlength;
     Accumulator *X = Xacc;
     Accumulator *D = Dacc;

     while (w--) {
          if (!(X->a & 0xF000)) {
               __u16 Da = D->a + 1;

               X->r = (Da * X->r) >> 8;
               X->g = (Da * X->g) >> 8;
               X->b = (Da * X->b) >> 8;
               X->a = (Da * X->a) >> 8;
          }

          X++;
          D++;
     }
}

static void Xacc_blend_invdstalpha()
{
     int          w = Dlength;
     Accumulator *X = Xacc;
     Accumulator *D = Dacc;

     while (w--) {
          if (!(X->a & 0xF000)) {
               __u16 Da = 0x100 - D->a;

               X->r = (Da * X->r) >> 8;
               X->g = (Da * X->g) >> 8;
               X->b = (Da * X->b) >> 8;
               X->a = (Da * X->a) >> 8;
          }

          X++;
          D++;
     }
}

static void Xacc_blend_destcolor()
{
     ONCE( "Xacc_blend_destcolor() unimplemented" );
}

static void Xacc_blend_invdestcolor()
{
     ONCE( "Xacc_blend_invdestcolor() unimplemented" );
}

static void Xacc_blend_srcalphasat()
{
     ONCE( "Xacc_blend_srcalphasat() unimplemented" );
}          

static GFunc Xacc_blend[] = {
     Xacc_blend_zero,         /* DSBF_ZERO         */
     Xacc_blend_one,          /* DSBF_ONE          */
     Xacc_blend_srccolor,     /* DSBF_SRCCOLOR     */
     Xacc_blend_invsrccolor,  /* DSBF_INVSRCCOLOR  */
     Xacc_blend_srcalpha,     /* DSBF_SRCALPHA     */
     Xacc_blend_invsrcalpha,  /* DSBF_INVSRCALPHA  */
     Xacc_blend_dstalpha,     /* DSBF_DESTALPHA    */
     Xacc_blend_invdstalpha,  /* DSBF_INVDESTALPHA */
     Xacc_blend_destcolor,    /* DSBF_DESTCOLOR    */
     Xacc_blend_invdestcolor, /* DSBF_INVDESTCOLOR */
     Xacc_blend_srcalphasat   /* DSBF_SRCALPHASAT  */
};

/********************************* Dacc_modulation ****************************/

#ifdef USE_MMX
void Dacc_modulate_argb_MMX();
#endif

static void Dacc_set_alpha()
{
     int          w = Dlength;
     Accumulator *D = Dacc;

     while (w--) {
          if (!(D->a & 0xF000)) {
               D->a = color.a;
          }

          D++;
     }
}

static void Dacc_modulate_alpha()
{
     int          w = Dlength;
     Accumulator *D = Dacc;

     while (w--) {
          if (!(D->a & 0xF000)) {
               D->a = (Cacc.a * D->a) >> 8;
          }

          D++;
     }
}

static void Dacc_modulate_rgb()
{
     int          w = Dlength;
     Accumulator *D = Dacc;

     while (w--) {
          if (!(D->a & 0xF000)) {
               D->r = (Cacc.r * D->r) >> 8;
               D->g = (Cacc.g * D->g) >> 8;
               D->b = (Cacc.b * D->b) >> 8;
          }

          D++;
     }
}

static void Dacc_modulate_rgb_set_alpha()
{
     int          w = Dlength;
     Accumulator *D = Dacc;

     while (w--) {
          if (!(D->a & 0xF000)) {
               D->a = color.a;
               D->r = (Cacc.r * D->r) >> 8;
               D->g = (Cacc.g * D->g) >> 8;
               D->b = (Cacc.b * D->b) >> 8;
          }

          D++;
     }
}

static void Dacc_modulate_argb()
{
     int          w = Dlength;
     Accumulator *D = Dacc;

     while (w--) {
          if (!(D->a & 0xF000)) {
               D->a = (Cacc.a * D->a) >> 8;
               D->r = (Cacc.r * D->r) >> 8;
               D->g = (Cacc.g * D->g) >> 8;
               D->b = (Cacc.b * D->b) >> 8;
          }

          D++;
     }
}

static GFunc Dacc_modulation[] = {
     NULL,
     NULL,
     Dacc_set_alpha,
     Dacc_modulate_alpha,
     Dacc_modulate_rgb,
     Dacc_modulate_rgb,
     Dacc_modulate_rgb_set_alpha,
     Dacc_modulate_argb
};

/********************************* misc accumulator operations ****************/

#ifdef USE_MMX
void Cacc_add_to_Dacc_MMX();
void Sacc_add_to_Dacc_MMX();
#endif

static void Cacc_to_Dacc()
{
     int          w = Dlength;
     Accumulator *D = Dacc;

     while (w--)
          *D++ = Cacc;
}



static void Cacc_add_to_Dacc_noMMX()
{
     int          w = Dlength;
     Accumulator *D = Dacc;

     while (w--) {
          D->a += Cacc.a;
          D->r += Cacc.r;
          D->g += Cacc.g;
          D->b += Cacc.b;

          D++;
     }
}

GFunc Cacc_add_to_Dacc = Cacc_add_to_Dacc_noMMX;

static void Sacc_add_to_Dacc_noMMX()
{
     int          w = Dlength;
     Accumulator *S = Sacc;
     Accumulator *D = Dacc;

     while (w--) {
          D->a += S->a;
          D->r += S->r;
          D->g += S->g;
          D->b += S->b;

          D++;
          S++;
     }
}

GFunc Sacc_add_to_Dacc = Sacc_add_to_Dacc_noMMX;

static void Sop_is_Aop() { Sop = Aop;}
static void Sop_is_Bop() { Sop = Bop;}

static void Dop_is_Aop() { Dop = Aop;}
static void Dop_is_Bop() { Dop = Bop;}

static void Sacc_is_NULL() { Sacc = NULL;}
static void Sacc_is_Aacc() { Sacc = Aacc;}
static void Sacc_is_Bacc() { Sacc = Bacc;}

static void Dacc_is_Aacc() { Dacc = Aacc;}
static void Dacc_is_Bacc() { Dacc = Bacc;}

static void Xacc_is_Aacc() { Xacc = Aacc;}
static void Xacc_is_Bacc() { Xacc = Bacc;}

int gAquire( CardState *state, DFBAccelerationMask accel )
{
     GFunc *funcs = gfuncs;
     int   pindex = PIXELFORMAT_INDEX(state->destination->format);

     DFBSurfaceLockFlags lock_flags;

     pthread_mutex_lock( &generic_lock );

     dst_bpp = BYTES_PER_PIXEL( state->destination->format );

     if (accel & 0xFFFF0000) {
          src_bpp = BYTES_PER_PIXEL( state->source->format );

          lock_flags = state->blittingflags & ( DSBLIT_BLEND_ALPHACHANNEL |
                                                DSBLIT_BLEND_COLORALPHA   |
                                                DSBLIT_DST_COLORKEY ) ?
                       DSLF_READ | DSLF_WRITE : DSLF_WRITE;
     }
     else
          lock_flags = state->drawingflags & ( DSDRAW_BLEND |
                                               DSDRAW_DST_COLORKEY ) ?
                       DSLF_READ | DSLF_WRITE : DSLF_WRITE;

     if (surface_soft_lock( state->destination,
                            lock_flags, &dst_org, &dst_pitch, 0 )) {
          pthread_mutex_unlock( &generic_lock );
          return 0;
     }

     if (accel & 0xFFFF0000) {
          if (surface_soft_lock( state->source,
                                 DSLF_READ, &src_org, &src_pitch, 1 )) {
               surface_unlock( state->destination, 0 );
               pthread_mutex_unlock( &generic_lock );
               return 0;
          }

          source_locked = 1;
     }
     else
          source_locked = 0;

     color = state->color;

     switch (state->destination->format) {
          case DSPF_RGB15:
               Cop = PIXEL_RGB15( color.r, color.g, color.b );
               break;
          case DSPF_RGB16:
               Cop = PIXEL_RGB16( color.r, color.g, color.b );
               break;
          case DSPF_RGB24:
               Cop = PIXEL_RGB24( color.r, color.g, color.b );
               break;
          case DSPF_RGB32:
               Cop = PIXEL_RGB32( color.r, color.g, color.b );
               break;
          case DSPF_ARGB:
               Cop = PIXEL_ARGB( color.a, color.r, color.g, color.b );
               break;
          case DSPF_A8:
               Cop = color.a;
               break;
          case DSPF_A1:
               Cop = (color.a >> 7) ? 0xFF : 0;
               break;
          default:
               ONCE("");
     }

     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_DRAWLINE:
          case DFXL_FILLTRIANGLE:
               if (state->drawingflags & DSDRAW_BLEND) {

                    /* not yet completed optimizing checks */
                    if (state->src_blend == DSBF_ZERO) {
                         if (state->dst_blend == DSBF_ZERO) {
                              Cop = 0;
                              *funcs++ = Dop_is_Aop;
                              if (state->drawingflags & DSDRAW_DST_COLORKEY) {
                                   Dkey = state->dst_colorkey;
                                   *funcs++ = Cop_toK_Dop_PFI[pindex];
                              }
                              else
                                   *funcs++ = Cop_to_Dop_PFI[pindex];
                              break;
                         }
                         else if (state->dst_blend == DSBF_ONE) {
                              break;
                         }
                    }

                    /* load from destination */
                    *funcs++ = Sop_is_Aop;
                    *funcs++ = Dacc_is_Aacc;
                    if (state->drawingflags & DSDRAW_DST_COLORKEY) {
                         Skey = state->dst_colorkey;
                         *funcs++ = Sop_PFI_Kto_Dacc[pindex];
                    }
                    else
                         *funcs++ = Sop_PFI_to_Dacc[pindex];

                    /* source blending */
                    Cacc.a = color.a;
                    Cacc.r = color.r;
                    Cacc.g = color.g;
                    Cacc.b = color.b;

                    switch (state->src_blend) {
                         case DSBF_ZERO:
                         case DSBF_ONE:
                              break;
                         case DSBF_SRCALPHA: {
                                   __u16 ca = color.a + 1;

                                   Cacc.a = (Cacc.a * ca) >> 8;
                                   Cacc.r = (Cacc.r * ca) >> 8;
                                   Cacc.g = (Cacc.g * ca) >> 8;
                                   Cacc.b = (Cacc.b * ca) >> 8;

                                   break;
                              }
                         case DSBF_INVSRCALPHA: {
                                   __u16 ca = 0x100 - color.a;

                                   Cacc.a = (Cacc.a * ca) >> 8;
                                   Cacc.r = (Cacc.r * ca) >> 8;
                                   Cacc.g = (Cacc.g * ca) >> 8;
                                   Cacc.b = (Cacc.b * ca) >> 8;

                                   break;
                              }
                         case DSBF_DESTALPHA:
                         case DSBF_INVDESTALPHA:
                         case DSBF_DESTCOLOR:
                         case DSBF_INVDESTCOLOR:
                              *funcs++ = Dacc_is_Bacc;
                              *funcs++ = Cacc_to_Dacc;

                              *funcs++ = Dacc_is_Aacc;
                              *funcs++ = Xacc_is_Bacc;
                              *funcs++ = Xacc_blend[state->src_blend - 1];

                              break;
                         case DSBF_SRCCOLOR:
                         case DSBF_INVSRCCOLOR:
                         case DSBF_SRCALPHASAT:
                              ONCE("unimplemented src blend function");
                    }

                    /* destination blending */
                    *funcs++ = Sacc_is_NULL;
                    *funcs++ = Xacc_is_Aacc;
                    *funcs++ = Xacc_blend[state->dst_blend - 1];

                    /* add source to destination accumulator */
                    switch (state->src_blend) {
                         case DSBF_ZERO:
                              break;
                         case DSBF_ONE:
                         case DSBF_SRCALPHA:
                         case DSBF_INVSRCALPHA:
                              if (Cacc.a || Cacc.r || Cacc.g || Cacc.b)
                                   *funcs++ = Cacc_add_to_Dacc;
                              break;
                         case DSBF_DESTALPHA:
                         case DSBF_INVDESTALPHA:
                         case DSBF_DESTCOLOR:
                         case DSBF_INVDESTCOLOR:
                              *funcs++ = Sacc_is_Bacc;
                              *funcs++ = Sacc_add_to_Dacc;
                              break;
                         case DSBF_SRCCOLOR:
                         case DSBF_INVSRCCOLOR:
                         case DSBF_SRCALPHASAT:
                              ONCE("unimplemented src blend function");
                    }

                    /* write to destination */
                    *funcs++ = Sacc_is_Aacc;
                    *funcs++ = Dop_is_Aop;
                    *funcs++ = Sacc_to_Dop_PFI[pindex];
               }
               else {
                    *funcs++ = Dop_is_Aop;

                    if (state->drawingflags & DSDRAW_DST_COLORKEY) {
                         Dkey = state->dst_colorkey;
                         *funcs++ = Cop_toK_Dop_PFI[pindex];
                    }
                    else
                         *funcs++ = Cop_to_Dop_PFI[pindex];
               }
               break;
          case DFXL_BLIT:
               if ((state->source->format == DSPF_A8) &&
                   (state->blittingflags &
                     (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_COLORIZE)) &&
                   (state->src_blend == DSBF_SRCALPHA) &&
                   (state->dst_blend == DSBF_INVSRCALPHA))
               {
                    *funcs++ = Sop_is_Bop;
                    *funcs++ = Dop_is_Aop;
                    *funcs++ = Sop_a8_set_alphapixel_Dop_PFI[pindex];

                    break;
               }
          case DFXL_STRETCHBLIT: {
                    int modulation = state->blittingflags & 0x7;

                    if (modulation) {

                         if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL |
                                                     DSBLIT_BLEND_COLORALPHA)) {
                              *funcs++ = Sop_is_Aop;
                              *funcs++ = Dacc_is_Aacc;
                              *funcs++ = Sop_PFI_to_Dacc[pindex];
                         }

                         *funcs++ = Sop_is_Bop;
                         *funcs++ = Dacc_is_Bacc;
                         if (state->blittingflags & DSBLIT_SRC_COLORKEY)
                              if (accel == DFXL_BLIT)
                                   *funcs++ = Sop_PFI_Kto_Dacc[PIXELFORMAT_INDEX(state->source->format)];
                              else
                                   *funcs++ = Sop_PFI_SKto_Dacc[PIXELFORMAT_INDEX(state->source->format)];
                         else
                              if (accel == DFXL_BLIT)
                                   *funcs++ = Sop_PFI_to_Dacc[PIXELFORMAT_INDEX(state->source->format)];
                              else
                                   *funcs++ = Sop_PFI_Sto_Dacc[PIXELFORMAT_INDEX(state->source->format)];

                         if (Dacc_modulation[modulation]) {
                              /* modulation source */
                              Cacc.a = color.a + 1;
                              Cacc.r = color.r + 1;
                              Cacc.g = color.g + 1;
                              Cacc.b = color.b + 1;

                              *funcs++ = Dacc_modulation[modulation];
                         }

                         if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL |
                                                     DSBLIT_BLEND_COLORALPHA)) {
                              *funcs++ = Sacc_is_Bacc;
                              *funcs++ = Dacc_is_Aacc;

                              *funcs++ = Xacc_is_Aacc;
                              *funcs++ = Xacc_blend[state->dst_blend - 1];

                              *funcs++ = Xacc_is_Bacc;
                              *funcs++ = Xacc_blend[state->src_blend - 1];

                              *funcs++ = Sacc_add_to_Dacc; /* don't do this when we know
                                                              Sacc is completely 0 */

                              *funcs++ = Sacc_is_Aacc;
                         }
                         else {
                              *funcs++ = Sacc_is_Bacc;
                         }

                         *funcs++ = Dop_is_Aop;
                         *funcs++ = Sacc_to_Dop_PFI[pindex];
                    }
                    else if (state->source->format == state->destination->format) {
                         *funcs++ = Sop_is_Bop;
                         *funcs++ = Dop_is_Aop;

                         if (accel == DFXL_BLIT) {
                              if (state->blittingflags & DSBLIT_SRC_COLORKEY) {
                                   Skey = state->src_colorkey;
                                   *funcs++ = Sop_PFI_Kto_Dop_PFI[pindex];
                              }
                              else
                                   *funcs++ = Sop_PFI_to_Dop_PFI[pindex];
                         }
                         else {
                              if (state->blittingflags & DSBLIT_SRC_COLORKEY) {
                                   Skey = state->src_colorkey;
                                   *funcs++ = Sop_PFI_SKto_Dop[pindex];
                              }
                              else
                                   *funcs++ = Sop_PFI_Sto_Dop[pindex];
                         }
                    }
                    else {
                         /* slow */
                         Sacc = Dacc = Aacc;

                         *funcs++ = Sop_is_Bop;
                         *funcs++ = Dop_is_Aop;

                         if (accel == DFXL_BLIT) {
                              if (state->blittingflags & DSBLIT_SRC_COLORKEY ) {
                                   Skey = state->src_colorkey;
                                   *funcs++ = Sop_PFI_Kto_Dacc[PIXELFORMAT_INDEX(state->source->format)];
                              }
                              else
                                   *funcs++ = Sop_PFI_to_Dacc[PIXELFORMAT_INDEX(state->source->format)];
                         }
                         else { // DFXL_STRETCHBLIT

                              if (state->blittingflags & DSBLIT_SRC_COLORKEY ) {
                                   Skey = state->src_colorkey;
                                   *funcs++ = Sop_PFI_SKto_Dacc[PIXELFORMAT_INDEX(state->source->format)];
                              }
                              else
                                   *funcs++ = Sop_PFI_Sto_Dacc[PIXELFORMAT_INDEX(state->source->format)];

                         }

                         *funcs++ = Sacc_to_Dop_PFI[pindex];
                    }
                    break;
               }
          default:
               ONCE("unimplemented drawing/blitting function");
     }

     *funcs = NULL;

     return 1;
}

void gRelease( CardState *state )
{
     surface_unlock( state->destination, 0 );

     if (source_locked)
          surface_unlock( state->source, 1 );

     pthread_mutex_unlock( &generic_lock );
}

void gUpload( int offset, void *data, int len )
{
     memcpy( (char*)card->framebuffer.base + offset, data, len );
}

#define RUN_PIPELINE()             \
     {                             \
          GFunc *funcs = gfuncs;   \
                                   \
          while (*funcs)           \
               (*funcs++)();       \
     }                             \

void gFillRectangle( DFBRectangle *rect )
{
     Aop = dst_org  +  rect->y * dst_pitch  +  rect->x * dst_bpp;
     Dlength = rect->w;

     while (rect->h--) {
          RUN_PIPELINE();

          Aop += dst_pitch;
     }
}

void gDrawLine( DFBRegion *line )
{
     int i,dx,dy,sdx,sdy,dxabs,dyabs,x,y,px,py;

     dx = line->x2 - line->x1;      /* the horizontal distance of the line */
     dxabs = abs(dx);

     dy = line->y2 - line->y1;      /* the vertical distance of the line */
     dyabs = abs(dy);

     if (!dx || !dy) {              /* draw horizontal/vertical line */
          DFBRectangle rect = { MIN (line->x1, line->x2),
               MIN (line->y1, line->y2),
               dxabs + 1, dyabs + 1};

          gFillRectangle( &rect );
          return;
     }

     sdx = SIGN(dx) * dst_bpp;
     sdy = SIGN(dy) * dst_pitch;
     x   = dyabs >> 1;
     y   = dxabs >> 1;
     px  = line->x1 * dst_bpp;
     py  = line->y1 * dst_pitch;

     if (dxabs >= dyabs) { /* the line is more horizontal than vertical */

          for (i=0, Dlength=1; i<dxabs; i++, Dlength++) {
               y += dyabs;
               if (y >= dxabs) {
                    Aop = dst_org + py + px;
                    RUN_PIPELINE();
                    if (sdx > 0)
                         px += Dlength * dst_bpp;
                    Dlength = 0;
                    y -= dxabs;
                    py += sdy;
               }
               if (sdx < 0)
                    px += sdx;
          }
          Aop = dst_org + py + px;
          RUN_PIPELINE();
     }
     else { /* the line is more vertical than horizontal */

          Dlength = 1;
          Aop = dst_org + py + px;
          RUN_PIPELINE();

          for (i=0; i<dyabs; i++) {
               x += dxabs;
               if (x >= dyabs) {
                    x -= dyabs;
                    px += sdx;
               }
               py += sdy;

               Aop = dst_org + py + px;
               RUN_PIPELINE();
          }
     }
}

static inline void gFillTrapezoid( int Xl, int Xr, int X2l, int X2r, int Y, int dY )
{
  int dXl = ((X2l - Xl) << 20) / dY;
  int dXr = ((X2r - Xr) << 20) / dY;

  Xl <<= 20;
  Xr <<= 20;

  while (dY--) {
    Aop = dst_org  +  Y++ * dst_pitch  +  ((Xl + (1<<19)) >> 20) * dst_bpp;
    Dlength = ((Xr + (1<<19)) >> 20) - ((Xl + (1<<19)) >> 20) + 1;

    if (Dlength)
      RUN_PIPELINE();

    Xl += dXl;
    Xr += dXr;
  }
}

void gFillTriangle( DFBTriangle *tri )
{
     sort_triangle( tri );

     if (tri->y2 == tri->y3) {
       gFillTrapezoid( tri->x1, tri->x1,
		       MIN( tri->x2, tri->x3 ), MAX( tri->x2, tri->x3 ),
		       tri->y1, tri->y3 - tri->y1 + 1 );
     } else
     if (tri->y1 == tri->y2) {
       gFillTrapezoid( MIN( tri->x1, tri->x2 ), MAX( tri->x1, tri->x2 ),
		       tri->x3, tri->x3,
		       tri->y1, tri->y3 - tri->y1 + 1 );
     }
     else {
       int majDx = tri->x3 - tri->x1;
       int majDy = tri->y3 - tri->y1;
       int topDx = tri->x2 - tri->x1;
       int topDy = tri->y2 - tri->y1;
       int botDy = tri->y3 - tri->y2;

       int topXperY = (topDx << 20) / topDy;
       int X2a = tri->x1 + (((topXperY * topDy) + (1<<19)) >> 20);

       int majXperY = (majDx << 20) / majDy;
       int majX2  = tri->x1 + (((majXperY * topDy) + (1<<19)) >> 20);
       int majX2a = majX2 - ((majXperY + (1<<19)) >> 20);

       
       gFillTrapezoid( tri->x1, tri->x1,
		       MIN( X2a, majX2a ), MAX( X2a, majX2a ),
		       tri->y1, topDy );
       gFillTrapezoid( MIN( tri->x2, majX2 ), MAX( tri->x2, majX2 ),
		       tri->x3, tri->x3,
		       tri->y2, botDy + 1 );
     }
}

void gBlit( DFBRectangle *rect, int dx, int dy )
{
     if (dx > rect->x)
          /* we must blit from right to left */
          Ostep = -1;
     else
          /* we must blit from left to right*/
          Ostep = 1;

     if (dy > rect->y) {
          /* we must blit from bottom to top */
          Aop = dst_org + (dy      + rect->h-1) * dst_pitch +      dx * dst_bpp;
          Bop = src_org + (rect->y + rect->h-1) * src_pitch + rect->x * src_bpp;
          Dlength = rect->w;

          while (rect->h--) {
               RUN_PIPELINE();

               Aop -= dst_pitch;
               Bop -= src_pitch;
          }
     }
     else {
          /* we must blit from top to bottom */
          Aop = dst_org  +       dy * dst_pitch  +       dx * dst_bpp;
          Bop = src_org  +  rect->y * src_pitch  +  rect->x * src_bpp;

          Dlength = rect->w;

          while (rect->h--) {
               RUN_PIPELINE();

               Aop += dst_pitch;
               Bop += src_pitch;
          }
     }
}

void gStretchBlit( DFBRectangle *srect, DFBRectangle *drect )
{
     int f;
     int i = 0;

     Aop = dst_org  +  drect->y * dst_pitch  +  drect->x * dst_bpp;
     Bop = src_org  +  srect->y * src_pitch  +  srect->x * src_bpp;

     Dlength = drect->w;
     SperD = (srect->w << 16) / drect->w;

     f = (srect->h << 16) / drect->h;

     while (drect->h--) {
          RUN_PIPELINE();

          Aop += dst_pitch;

          i += f;

          while (i > 0xFFFF) {
               i -= 0x10000;
               Bop += src_pitch;
          }
     }
}


#ifdef USE_MMX

/*
 * patches function pointers to MMX functions
 */
void gInit_MMX()
{
/********************************* Sop_PFI_Sto_Dacc ***************************/
     Sop_PFI_Sto_Dacc[PIXELFORMAT_INDEX(DSPF_ARGB)] = Sop_argb_Sto_Dacc_MMX;
/********************************* Sop_PFI_to_Dacc ****************************/
     Sop_PFI_to_Dacc[PIXELFORMAT_INDEX(DSPF_RGB16)] = Sop_rgb16_to_Dacc_MMX;
     Sop_PFI_to_Dacc[PIXELFORMAT_INDEX(DSPF_RGB32)] = Sop_rgb32_to_Dacc_MMX;
     Sop_PFI_to_Dacc[PIXELFORMAT_INDEX(DSPF_ARGB )] = Sop_argb_to_Dacc_MMX;
/********************************* Sacc_to_Dop_PFI ****************************/
     Sacc_to_Dop_PFI[PIXELFORMAT_INDEX(DSPF_RGB16)] = Sacc_to_Dop_rgb16_MMX;
     Sacc_to_Dop_PFI[PIXELFORMAT_INDEX(DSPF_RGB32)] = Sacc_to_Dop_rgb32_MMX;
/********************************* Xacc_blend *********************************/
     Xacc_blend[DSBF_SRCALPHA-1] = Xacc_blend_srcalpha_MMX;
     Xacc_blend[DSBF_INVSRCALPHA-1] = Xacc_blend_invsrcalpha_MMX;
/********************************* Dacc_modulation ****************************/
     Dacc_modulation[7] = Dacc_modulate_argb_MMX; //FIXME: do not hardcode
/********************************* misc accumulator operations ****************/
     Cacc_add_to_Dacc = Cacc_add_to_Dacc_MMX;
     Sacc_add_to_Dacc = Sacc_add_to_Dacc_MMX;
}

#endif

