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

#include "config.h"

#include <endian.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <asm/types.h>

#include <pthread.h>

#include "directfb.h"

#include "core/core.h"
#include "core/coredefs.h"
#include "core/coretypes.h"

#include "core/gfxcard.h"
#include "core/state.h"
#include "core/surfacemanager.h"

#include "misc/gfx_util.h"
#include "misc/util.h"
#include "misc/conf.h"
#include "misc/memcpy.h"
#include "misc/cpu_accel.h"

#include "gfx/convert.h"
#include "gfx/util.h"

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
void *Aop = NULL;
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
void        *Sop  = NULL;

/* controls horizontal blitting direction */
int Ostep = 0;

int Dlength = 0;
int SperD = 0; /* for scaled routines only */

static int use_mmx = 0;

static pthread_mutex_t generic_lock = PTHREAD_MUTEX_INITIALIZER;

/* lookup tables for 2/3bit to 8bit color conversion */
#ifdef SUPPORT_RGB332
static const __u8 lookup3to8[] = { 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff };
static const __u8 lookup2to8[] = { 0x00, 0x55, 0xaa, 0xff };
#endif

/********************************* Cop_to_Aop_PFI *****************************/
static void Cop_to_Aop_8()
{
     memset( Aop, (__u8)Cop, Dlength );
}

static void Cop_to_Aop_16()
{
     int    w, l = Dlength;
     __u32 *D = (__u32*)Aop;

     __u32 DCop = ((Cop << 16) | Cop);

     if (((long)D)&2) {         /* align */
          __u16* tmp=Aop;
          --l;
          *tmp = Cop;
          D = (__u32*)(tmp+1);
     }

     w = (l >> 1);
     while (w) {
          *D = DCop;
          --w;
          ++D;
     }

     if (l & 1)                 /* do the last ential pixel */
          *((__u16*)D) = (__u16)Cop;
}

static void Cop_to_Aop_24()
{
     int   w = Dlength;
     __u8 *D = (__u8*)Aop;

     while (w) {
          D[0] = color.b;
          D[1] = color.g;
          D[2] = color.r;

          D += 3;
          --w;
     }
}

static void Cop_to_Aop_32()
{
     int    w = Dlength;
     __u32 *D = (__u32*)Aop;

     while (w--)
          *D++ = (__u32)Cop;
}

static GFunc Cop_to_Aop_PFI[] = {
     Cop_to_Aop_16,
     Cop_to_Aop_16,
     Cop_to_Aop_24,
     Cop_to_Aop_32,
     Cop_to_Aop_32,
     Cop_to_Aop_8,
     NULL,
#ifdef SUPPORT_RGB332
     Cop_to_Aop_8,
#else     
     NULL,
#endif
     NULL,
     NULL,
     NULL
};

/********************************* Cop_toK_Aop_PFI ****************************/

static void Cop_toK_Aop_8()
{
     int   w = Dlength;
     __u8 *D = (__u8*)Aop;

     while (w--) {
          if ((__u8)Dkey == *D)
               *D = (__u8)Cop;

          D++;
     }
}

static void Cop_toK_Aop_16()
{
     int    w = Dlength;
     __u16 *D = (__u16*)Aop;

     while (w--) {
          if ((__u16)Dkey == *D)
               *D = (__u16)Cop;

          D++;
     }
}

static void Cop_toK_Aop_24()
{
     ONCE("Cop_toK_Aop_24() unimplemented");
}

static void Cop_toK_Aop_32()
{
     int    w = Dlength;
     __u32 *D = (__u32*)Aop;

     while (w--) {
          if ((__u32)Dkey != *D)
               *D = (__u32)Cop;

          D++;
     }
}

static GFunc Cop_toK_Aop_PFI[] = {
     Cop_toK_Aop_16,
     Cop_toK_Aop_16,
     Cop_toK_Aop_24,
     Cop_toK_Aop_32,
     Cop_toK_Aop_32,
     Cop_toK_Aop_8,
     NULL,
#ifdef SUPPORT_RGB332
     Cop_toK_Aop_8,
#else
     NULL,
#endif
     NULL,
     NULL,
     NULL
};

/********************************* Bop_PFI_to_Aop_PFI *************************/

static void Bop_8_to_Aop()
{
     dfb_memmove( Aop, Bop, Dlength );
}

static void Bop_16_to_Aop()
{
     dfb_memmove( Aop, Bop, Dlength*2 );
}

static void Bop_24_to_Aop()
{
     dfb_memmove( Aop, Bop, Dlength*3 );
}

static void Bop_32_to_Aop()
{
     dfb_memmove( Aop, Bop, Dlength*4 );
}

static GFunc Bop_PFI_to_Aop_PFI[] = {
     Bop_16_to_Aop,
     Bop_16_to_Aop,
     Bop_24_to_Aop,
     Bop_32_to_Aop,
     Bop_32_to_Aop,
     Bop_8_to_Aop,
     Bop_16_to_Aop,
#ifdef SUPPORT_RGB332
     Bop_8_to_Aop,
#else
     NULL,
#endif
     Bop_16_to_Aop,
     NULL,
     NULL
};

/********************************* Bop_PFI_Kto_Aop_PFI ************************/

static void Bop_rgb15_Kto_Aop()
{
     int    w, l = Dlength;
     __u32 *D = (__u32*)Aop;
     __u32 *S = (__u32*)Bop;

     __u32 DSkey = (Skey << 16) | (Skey & 0x0000FFFF);

     if (((long)D)&2) {         /* align */
          __u16 *tmp = Aop;
          --l;
          if ((*((__u16*)S) & 0x7FFF) != (__u16)Skey)
           *tmp = *((__u16*)S);

          D = (__u32*)((__u16*)D+1);
          S = (__u32*)((__u16*)S+1);
     }

     w = (l >> 1);
     while (w) {
          __u32 dpixel = *S;
          __u16 *tmp = (__u16*)D;

          if (dpixel != DSkey) {
               if ((dpixel & 0x7FFF0000) != (DSkey & 0x7FFF0000)) {
                    if ((dpixel & 0x00007FFF) != (DSkey & 0x00007FFF)) {
                         *D = dpixel;
                    }
                    else {
#if __BYTE_ORDER == __BIG_ENDIAN
                         tmp[0] = (__u16)(dpixel >> 16);
#else
                         tmp[1] = (__u16)(dpixel >> 16);
#endif
                    }
               }
               else {
#if __BYTE_ORDER == __BIG_ENDIAN
                    tmp[1] = (__u16)dpixel;
#else
                    tmp[0] = (__u16)dpixel;
#endif
               }
          }
          ++S;
          ++D;
          --w;
     }

     if (l & 1) {                 /* do the last potential pixel */
          if ((*((__u16*)S) & 0x7FFF) != (__u16)Skey)
               *((__u16*)D) = *((__u16*)S);
     }
}

static void Bop_rgb16_Kto_Aop()
{
     int    w, l = Dlength;
     __u32 *D = (__u32*)Aop;
     __u32 *S = (__u32*)Bop;

     __u32 DSkey = (Skey << 16) | (Skey & 0x0000FFFF);

     if (((long)D)&2) {         /* align */
          __u16 *tmp = Aop;
          --l;
          if (*((__u16*)S) != (__u16)Skey)
           *tmp = *((__u16*)S);

          D = (__u32*)((__u16*)D+1);
          S = (__u32*)((__u16*)S+1);
     }

     w = (l >> 1);
     while (w) {
          __u32 dpixel = *S;
          __u16 *tmp = (__u16*)D;

          if (dpixel != DSkey) {
               if ((dpixel & 0xFFFF0000) != (DSkey & 0xFFFF0000)) {
                    if ((dpixel & 0x0000FFFF) != (DSkey & 0x0000FFFF)) {
                         *D = dpixel;
                    }
                    else {
#if __BYTE_ORDER == __BIG_ENDIAN
                         tmp[0] = (__u16)(dpixel >> 16);
#else
                         tmp[1] = (__u16)(dpixel >> 16);
#endif
                    }
               }
               else {
#if __BYTE_ORDER == __BIG_ENDIAN
                    tmp[1] = (__u16)dpixel;
#else
                    tmp[0] = (__u16)dpixel;
#endif
               }
          }
          ++S;
          ++D;
          --w;
     }

     if (l & 1) {                 /* do the last potential pixel */
          if (*((__u16*)S) != (__u16)Skey)
               *((__u16*)D) = *((__u16*)S);
     }
}

static void Bop_rgb24_Kto_Aop()
{
     int    w = Dlength;
     __u8 *D = (__u8*)Aop;
     __u8 *S = (__u8*)Bop;

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

static void Bop_rgb32_Kto_Aop()
{
     int    w = Dlength;
     __u32 *D = (__u32*)Aop;
     __u32 *S = (__u32*)Bop;

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

static void Bop_argb_Kto_Aop()
{
     int    w = Dlength;
     __u32 *D = (__u32*)Aop;
     __u32 *S = (__u32*)Bop;

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

static void Bop_a8_Kto_Aop()
{
     ONCE( "Bop_a8_Kto_Aop() unimplemented");
}

#ifdef SUPPORT_RGB332
static void Bop_rgb332_Kto_Aop()
{
     int    w = Dlength;
     __u8 *D = (__u8*)Aop;
     __u8 *S = (__u8*)Bop;

     if (Ostep < 0) {
          D+= Dlength - 1;
          S+= Dlength - 1;
     }

     while (w--) {
          __u8 spixel = *S;

          if (spixel != (__u8)Skey)
               *D = spixel;

          S+=Ostep;
          D+=Ostep;
     }
}
#endif

static GFunc Bop_PFI_Kto_Aop_PFI[] = {
     Bop_rgb15_Kto_Aop,
     Bop_rgb16_Kto_Aop,
     Bop_rgb24_Kto_Aop,
     Bop_rgb32_Kto_Aop,
     Bop_argb_Kto_Aop,
     Bop_a8_Kto_Aop,
     NULL,
#ifdef SUPPORT_RGB332
     Bop_rgb332_Kto_Aop,
#else
     NULL,
#endif
     NULL,
     NULL,
     NULL
};

/********************************* Bop_PFI_Sto_Aop ****************************/

static void Bop_16_Sto_Aop()
{
     int    w  = Dlength;
     int    w2;
     int    i = 0;
     __u32 *D = (__u32*)Aop;
     __u16 *S = (__u16*)Bop;

     if (((long)D)&2) {
        *(((__u16*)D)++) = *S;
        i += SperD;
        w--;
     }

     w2 = (w >> 1);
     while (w2--) {
          int SperD2 = (SperD << 1);
#if __BYTE_ORDER == __BIG_ENDIAN
          *D++ =  S[i>>16] << 16 | S[(i+SperD)>>16];
#else
          *D++ = (S[(i+SperD)>>16] << 16) | S[i>>16];
#endif
          i += SperD2;
     }
     if (w&1) {
          *((__u16*)D) = S[i>>16];
     }     
}

static void Bop_24_Sto_Aop()
{
     int    w = Dlength;
     int    i = 0;
     __u8 *D = (__u8*)Aop;
     __u8 *S = (__u8*)Bop;

     while (w--) {
          int pixelstart = (i>>16)*3;

          *D++ = S[pixelstart+0];
          *D++ = S[pixelstart+1];
          *D++ = S[pixelstart+2];

          i += SperD;
     }
}

static void Bop_32_Sto_Aop()
{
     int    w = Dlength;
     int    i = 0;
     __u32 *D = (__u32*)Aop;
     __u32 *S = (__u32*)Bop;

     while (w--) {
          *D++ = S[i>>16];

          i += SperD;
     }
}

static void Bop_8_Sto_Aop()
{
     int    w = Dlength;
     int    i = 0;
     __u8 *D = (__u8*)Aop;
     __u8 *S = (__u8*)Bop;

     while (w--) {
          *D++ = S[i>>16];

          i += SperD;
     }
}

static GFunc Bop_PFI_Sto_Aop[] = {
     Bop_16_Sto_Aop,
     Bop_16_Sto_Aop,
     Bop_24_Sto_Aop,
     Bop_32_Sto_Aop,
     Bop_32_Sto_Aop,
     Bop_8_Sto_Aop,
     NULL,
#ifdef SUPPORT_RGB332
     Bop_8_Sto_Aop,
#else
     NULL,
#endif
     NULL,
     NULL,
     NULL
};

/********************************* Bop_PFI_SKto_Aop ***************************/

static void Bop_rgb15_SKto_Aop()
{
     int    w = Dlength;
     int    i = 0;
     __u16 *D = (__u16*)Aop;
     __u16 *S = (__u16*)Bop;

     while (w--) {
          __u16 s = S[i>>16] & 0x7FFF;

          if (s != Skey)
               *D = s;

          D++;
          i += SperD;
     }
}

static void Bop_rgb16_SKto_Aop()
{
     int    w = Dlength;
     int    i = 0;
     __u16 *D = (__u16*)Aop;
     __u16 *S = (__u16*)Bop;

     while (w--) {
          __u16 s = S[i>>16];

          if (s != Skey)
               *D = s;

          D++;
          i += SperD;
     }
}

static void Bop_rgb24_SKto_Aop()
{
     int    w = Dlength;
     int    i = 0;
     __u8 *D = (__u8*)Aop;
     __u8 *S = (__u8*)Bop;

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

static void Bop_rgb32_SKto_Aop()
{
     int    w = Dlength;
     int    i = 0;
     __u32 *D = (__u32*)Aop;
     __u32 *S = (__u32*)Bop;

     while (w--) {
          __u32 s = S[i>>16] & 0x00FFFFFF;

          if (s != Skey)
               *D = s;

          D++;
          i += SperD;
     }
}

static void Bop_argb_SKto_Aop()
{
     int    w = Dlength;
     int    i = 0;
     __u32 *D = (__u32*)Aop;
     __u32 *S = (__u32*)Bop;

     while (w--) {
          __u32 s = S[i>>16];

          if (s != Skey)
               *D = s;

          D++;
          i += SperD;
     }
}

static void Bop_a8_SKto_Aop()
{
     ONCE( "Bop_a8_SKto_Aop() unimplemented" );
}

#ifdef SUPPORT_RGB332
static void Bop_rgb332_SKto_Aop()
{
     int    w = Dlength;
     int    i = 0;
     __u8 *D = (__u8*)Aop;
     __u8 *S = (__u8*)Bop;

     while (w--) {
          __u8 s = S[i>>16];

          if (s != Skey)
               *D = s;

          D++;
          i += SperD;
     }
}
#endif

static GFunc Bop_PFI_SKto_Aop[] = {
     Bop_rgb15_SKto_Aop,
     Bop_rgb16_SKto_Aop,
     Bop_rgb24_SKto_Aop,
     Bop_rgb32_SKto_Aop,
     Bop_argb_SKto_Aop,
     Bop_a8_SKto_Aop,
     NULL,
#ifdef SUPPORT_RGB332
     Bop_rgb332_SKto_Aop,
#else
     NULL,
#endif
     NULL,
     NULL,
     NULL
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

#ifdef SUPPORT_RGB332
static void Sop_rgb332_Sto_Dacc()
{
     int    w = Dlength;
     int    i = 0;

     Accumulator *D = Dacc;
     __u8        *S = (__u8*)Sop;

     while (w--) {
          __u8 s = S[i>>16];

          D->a = 0xFF;
          D->r = lookup3to8[s >> 5];
          D->g = lookup3to8[(s & 0x1C) >> 2];
          D->b = lookup2to8[s & 0x03];

          i += SperD;

          D++;
     }
}
#endif

static GFunc Sop_PFI_Sto_Dacc[] = {
     Sop_rgb15_Sto_Dacc,
     Sop_rgb16_Sto_Dacc,
     Sop_rgb24_Sto_Dacc,
     Sop_rgb32_Sto_Dacc,
     Sop_argb_Sto_Dacc,
     Sop_a8_Sto_Dacc,
     NULL,
#ifdef SUPPORT_RGB332
     Sop_rgb332_Sto_Dacc,
#else
     NULL,
#endif
     NULL,
     NULL,
     NULL
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
     Sop_a8_SKto_Dacc,
     NULL,
     NULL,     /* FIXME: RGB332 */
     NULL,
     NULL,
     NULL
};

/********************************* Sop_PFI_to_Dacc ****************************/

#ifdef USE_MMX
void Sop_rgb16_to_Dacc_MMX();
void Sop_rgb32_to_Dacc_MMX();
void Sop_argb_to_Dacc_MMX();
#endif


static void Sop_rgb15_to_Dacc()
{
     int       l, w = Dlength;
     Accumulator *D = Dacc;
     __u16       *S = (__u16*)Sop;

     if (((long)S)&2) {
          __u16 spixel = *S;

          D->a = 0xFF;
          D->r = (spixel & 0x7C00) >> 7;
          D->g = (spixel & 0x03E0) >> 2;
          D->b = (spixel & 0x001F) << 3;

          ++S;
          ++D;
          --w;
     }

     l = w >> 1;
     while (l) {
          __u32 spixel2 = *((__u32*)S);

#if __BYTE_ORDER == __BIG_ENDIAN
          D[0].a = 0xFF;
          D[0].r = (spixel2 & 0x7C000000) >> 23;
          D[0].g = (spixel2 & 0x03E00000) >> 18;
          D[0].b = (spixel2 & 0x001F0000) >> 13;

          D[1].a = 0xFF;
          D[1].r = (spixel2 & 0x7C00) >> 7;
          D[1].g = (spixel2 & 0x03E0) >> 2;
          D[1].b = (spixel2 & 0x001F) << 3;
#else
          D[0].a = 0xFF;
          D[0].r = (spixel2 & 0x7C00) >> 7;
          D[0].g = (spixel2 & 0x03E0) >> 2;
          D[0].b = (spixel2 & 0x001F) << 3;

          D[1].a = 0xFF;
          D[1].r = (spixel2 & 0x7C000000) >> 23;
          D[1].g = (spixel2 & 0x03E00000) >> 18;
          D[1].b = (spixel2 & 0x001F0000) >> 13;
#endif

          S += 2;
          D += 2;

          --l;
     }

     if (w&1) {
          __u16 spixel = *S;

          D->a = 0xFF;
          D->r = (spixel & 0x7C00) >> 7;
          D->g = (spixel & 0x03E0) >> 2;
          D->b = (spixel & 0x001F) << 3;
     }
}

static void Sop_rgb16_to_Dacc()
{
     int       l, w = Dlength;
     Accumulator *D = Dacc;
     __u16       *S = (__u16*)Sop;

     if (((long)S)&2) {
          __u16 spixel = *S;

          D->a = 0xFF;
          D->r = (spixel & 0xF800) >> 8;
          D->g = (spixel & 0x07E0) >> 3;
          D->b = (spixel & 0x001F) << 3;

          ++S;
          ++D;
          --w;
     }

     l = w >> 1;
     while (l) {
          __u32 spixel2 = *((__u32*)S);

#if __BYTE_ORDER == __BIG_ENDIAN
          D[0].a = 0xFF;
          D[0].r = (spixel2 & 0xF8000000) >> 24;
          D[0].g = (spixel2 & 0x07E00000) >> 19;
          D[0].b = (spixel2 & 0x001F0000) >> 13;

          D[1].a = 0xFF;
          D[1].r = (spixel2 & 0xF800) >> 8;
          D[1].g = (spixel2 & 0x07E0) >> 3;
          D[1].b = (spixel2 & 0x001F) << 3;
#else
          D[0].a = 0xFF;
          D[0].r = (spixel2 & 0xF800) >> 8;
          D[0].g = (spixel2 & 0x07E0) >> 3;
          D[0].b = (spixel2 & 0x001F) << 3;

          D[1].a = 0xFF;
          D[1].r = (spixel2 & 0xF8000000) >> 24;
          D[1].g = (spixel2 & 0x07E00000) >> 19;
          D[1].b = (spixel2 & 0x001F0000) >> 13;
#endif

          S += 2;
          D += 2;

          --l;
     }

     if (w&1) {
          __u16 spixel = *S;

          D->a = 0xFF;
          D->r = (spixel & 0xF800) >> 8;
          D->g = (spixel & 0x07E0) >> 3;
          D->b = (spixel & 0x001F) << 3;
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

#ifdef SUPPORT_RGB332
static void Sop_rgb332_to_Dacc()
{
     int          w = Dlength;
     Accumulator *D = Dacc;
     __u8        *S = (__u8*)Sop;

     while (w--) {
          __u8 s = *S++;

          D->a = 0xFF;
          D->r = lookup3to8[s >> 5];
          D->g = lookup3to8[(s & 0x1C) >> 2];
          D->b = lookup2to8[s & 0x03];

          D++;
     }
}
#endif

static GFunc Sop_PFI_to_Dacc[] = {
     Sop_rgb15_to_Dacc,
     Sop_rgb16_to_Dacc,
     Sop_rgb24_to_Dacc,
     Sop_rgb32_to_Dacc,
     Sop_argb_to_Dacc,
     Sop_a8_to_Dacc,
     NULL,
#ifdef SUPPORT_RGB332
     Sop_rgb332_to_Dacc,
#else
     NULL,
#endif
     NULL,
     NULL,
     NULL
};

/********************************* Sop_PFI_Kto_Dacc ***************************/

static void Sop_rgb15_Kto_Dacc()
{
     int          w = Dlength;
     Accumulator *D = Dacc;
     __u16       *S = (__u16*)Sop;

     while (w--) {
          __u16 s = *S++ & 0x7FFF;

          if (s != (__u16)Skey) {
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

          if (s != (__u16)Skey) {
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
               D->r = s >> 16;
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

          if ((s & 0xFFFFFF) != Skey) {
               D->a = s >> 24;
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

#ifdef SUPPORT_RGB332
static void Sop_rgb332_Kto_Dacc()
{
     int          w = Dlength;
     Accumulator *D = Dacc;
     __u8        *S = (__u8*)Sop;

     while (w--) {
          __u8 s = *S++;

          if (s != (__u8)Skey) {
               D->a = 0xFF;
               D->r = lookup3to8[s >> 5];
               D->g = lookup3to8[(s & 0x1C) >> 2];
               D->b = lookup2to8[s & 0x03];
          }
          else
               D->a = 0xF000;

          D++;
     }
}

#endif

static GFunc Sop_PFI_Kto_Dacc[] = {
     Sop_rgb15_Kto_Dacc,
     Sop_rgb16_Kto_Dacc,
     Sop_rgb24_Kto_Dacc,
     Sop_rgb32_Kto_Dacc,
     Sop_argb_Kto_Dacc,
     Sop_a8_Kto_Dacc,
     NULL,
#ifdef SUPPORT_RGB332
     Sop_rgb332_Kto_Dacc,
#else
     NULL,
#endif
     NULL,
     NULL,
     NULL
};

/********************************* Sacc_to_Aop_PFI ****************************/

#ifdef USE_MMX
void Sacc_to_Aop_rgb16_MMX();
void Sacc_to_Aop_rgb32_MMX();
#endif

static void Sacc_to_Aop_rgb15()
{
     int          w = Dlength;
     Accumulator *S = Sacc;
     __u16       *D = (__u16*)Aop;

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

static void Sacc_to_Aop_rgb16()
{
     int          w = Dlength;
     Accumulator *S = Sacc;
     __u16       *D = (__u16*)Aop;

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

static void Sacc_to_Aop_rgb24()
{
     int          w = Dlength;
     Accumulator *S = Sacc;
     __u8        *D = (__u8*)Aop;

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

static void Sacc_to_Aop_rgb32()
{
     int          w = Dlength;
     Accumulator *S = Sacc;
     __u32       *D = (__u32*)Aop;

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

static void Sacc_to_Aop_argb()
{
     int          w = Dlength;
     Accumulator *S = Sacc;
     __u32       *D = (__u32*)Aop;

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

static void Sacc_to_Aop_a8()
{
     ONCE( "Sacc_to_Aop_a8() unimplemented" );
}

#ifdef SUPPORT_RGB332
static void Sacc_to_Aop_rgb332()
{
     int          w = Dlength;
     Accumulator *S = Sacc;
     __u8        *D = (__u8*)Aop;

     while (w--) {
          if (!(S->a & 0xF000)) {
               *D = PIXEL_RGB332( (S->r & 0xFF00) ? 0xFF : S->r,
                                  (S->g & 0xFF00) ? 0xFF : S->g,
                                  (S->b & 0xFF00) ? 0xFF : S->b );
          }

          D++;
          S++;
     }
}
#endif

GFunc Sacc_to_Aop_PFI[] = {
     Sacc_to_Aop_rgb15,
     Sacc_to_Aop_rgb16,
     Sacc_to_Aop_rgb24,
     Sacc_to_Aop_rgb32,
     Sacc_to_Aop_argb,
     Sacc_to_Aop_a8,
     NULL,
#ifdef SUPPORT_RGB332
     Sacc_to_Aop_rgb332,
#else
     NULL,
#endif
     NULL,
     NULL,
     NULL
};

/************** Bop_a8_set_alphapixel_Aop_PFI *********************************/

#define SET_ALPHA_PIXEL_RGB15(d,a) \
     switch (a) {\
     case 0xff: *(d) = __rb + __g; \
     case 0: break; \
     default: {\
          __u32 pixel = *(d); \
          __u8  s = ((a)>>3)+1; \
          register __u32 t1,t2; \
          t1 = (pixel&0x7c1f); t2 = (pixel&0x03e0); \
          pixel = ((((__rb-t1)*s+(t1<<5)) & 0x000f83e0) + \
                   ((( __g-t2)*s+(t2<<5)) & 0x00007c00)) >> 5; \
          *(d) = pixel;\
          }\
     }

static void Bop_a8_set_alphapixel_Aop_rgb15()
{
     int    w = Dlength;
     __u8  *S = Bop;
     __u16 *D = (__u16*)Aop;
     __u32 __rb = (((color.r & 0xf8) << 7) | ((color.b & 0xf8) >> 3));
     __u32 __g  =  ((color.g & 0xf8) << 2);

     while (w>4) {
          SET_ALPHA_PIXEL_RGB15( D, *S ); D++, S++;
          SET_ALPHA_PIXEL_RGB15( D, *S ); D++, S++;
          SET_ALPHA_PIXEL_RGB15( D, *S ); D++, S++;
          SET_ALPHA_PIXEL_RGB15( D, *S ); D++, S++;
          w-=4;
     }
     while (w--) {
          SET_ALPHA_PIXEL_RGB15( D, *S ); D++, S++;
     }
}

#define SET_ALPHA_PIXEL_RGB16(d,a) \
     switch (a) {\
     case 0xff: *(d) = __rb + __g; \
     case 0: break; \
     default: {\
          __u32 pixel = *(d);\
          __u8  s = ((a)>>2)+1;\
          register __u32 t1,t2; \
          t1 = (pixel&0xf81f); t2 = (pixel&0x07e0); \
          pixel = ((((__rb-t1)*s+(t1<<6)) & 0x003e07c0) + \
                   ((( __g-t2)*s+(t2<<6)) & 0x0001f800)) >> 6; \
          *(d) = pixel;\
          }\
     }

static void Bop_a8_set_alphapixel_Aop_rgb16()
{
     int    w = Dlength;
     __u8  *S = Bop;
     __u16 *D = (__u16*)Aop;
     __u32 __rb = (((color.r & 0xf8) << 8) | ((color.b & 0xf8) >> 3));
     __u32 __g  =  ((color.g & 0xfc) << 3);

     while (w>4) {
          SET_ALPHA_PIXEL_RGB16( D, *S ); D++, S++;
          SET_ALPHA_PIXEL_RGB16( D, *S ); D++, S++;
          SET_ALPHA_PIXEL_RGB16( D, *S ); D++, S++;
          SET_ALPHA_PIXEL_RGB16( D, *S ); D++, S++;
      w-=4;
     }
     while (w--) {
          SET_ALPHA_PIXEL_RGB16( D, *S ); D++, S++;
     }
}

#define SET_ALPHA_PIXEL_RGB24(d,r,g,b,a)\
     if (a) {\
          __u8 *pixel = (d);\
          \
          if ((a) == 0xff) {\
               *pixel++ = (b);\
               *pixel++ = (g);\
               *pixel++ = (r);\
          }\
          else {\
               __u16 s = (a)+1;\
               \
               *pixel = (((b)-(*pixel)) * s + (*pixel << 8)) >> 8;\
               ++pixel;\
               *pixel = (((g)-(*pixel)) * s + (*pixel << 8)) >> 8;\
               ++pixel;\
               *pixel = (((r)-(*pixel)) * s + (*pixel << 8)) >> 8;\
               ++pixel;\
          }\
     }

static void Bop_a8_set_alphapixel_Aop_rgb24()
{
     int    w = Dlength;
     __u8  *S = Bop;
     __u8  *D = (__u8*)Aop;

     while (w>4) {
          SET_ALPHA_PIXEL_RGB24( D, color.r, color.g, color.b, *S ); D+=3; S++;
          SET_ALPHA_PIXEL_RGB24( D, color.r, color.g, color.b, *S ); D+=3; S++;
          SET_ALPHA_PIXEL_RGB24( D, color.r, color.g, color.b, *S ); D+=3; S++;
          SET_ALPHA_PIXEL_RGB24( D, color.r, color.g, color.b, *S ); D+=3; S++;
      w-=4;
     }
     while (w--) {
          SET_ALPHA_PIXEL_RGB24( D, color.r, color.g, color.b, *S ); D+=3, S++;
     }
}

#define SET_ALPHA_PIXEL_RGB32(d,a)\
     switch (a) {\
     case 0xff: *(d) = (0xff000000 | __rb | __g); \
     case 0: break; \
     default: {\
          __u32 pixel = *(d);\
          __u16  s = (a)+1;\
          register __u32 t1,t2; \
          t1 = (pixel&0x00ff00ff); t2 = (pixel&0x0000ff00); \
          pixel = ((((__rb-t1)*s+(t1<<8)) & 0xff00ff00) + \
                   ((( __g-t2)*s+(t2<<8)) & 0x00ff0000)) >> 8; \
          *(d) = pixel;\
          }\
     }

static void Bop_a8_set_alphapixel_Aop_rgb32()
{
     int    w = Dlength;
     __u8  *S = Bop;
     __u32 *D = (__u32*)Aop;
     __u32 __rb = (((color.r) << 16) | (color.b));
     __u32 __g  =  ((color.g) << 8);

     while (w>4) {
          SET_ALPHA_PIXEL_RGB32( D, *S ); D++; S++;
          SET_ALPHA_PIXEL_RGB32( D, *S ); D++; S++;
          SET_ALPHA_PIXEL_RGB32( D, *S ); D++; S++;
          SET_ALPHA_PIXEL_RGB32( D, *S ); D++; S++;
      w-=4;
     }
     while (w--) {
          SET_ALPHA_PIXEL_RGB32( D, *S ); D++; S++;
     }
}


/* saturating alpha blend */

#define SET_ALPHA_PIXEL_ARGB(d,g,a)\
     switch (a) {\
     case 0xff: *(d) = 0xff000000 | __rb | ((g) << 8);\
     case 0: break;\
     default: {\
          __u32 pixel = *(d);\
          __u16  s = (a)+1;\
          __u16 s1 = 256-s;\
          __u16 sa = (pixel >> 24) + a;\
          if (sa & 0xff00) sa = 0xff;\
          *(d) = (sa << 24) + \
                 (((((pixel & 0x00ff00ff)       * s1) + (__rb * s)) >> 8) & 0x00ff00ff) + \
                 (((((pixel & 0x0000ff00) >> 8) * s1) + ((g)  * s))       & 0x0000ff00);  \
          }\
     }

static void Bop_a8_set_alphapixel_Aop_argb()
{
     int    w = Dlength;
     __u8  *S = Bop;
     __u32 *D = (__u32*)Aop;
     __u32 __rb = (((color.r)<<16) | (color.b));

     while (w>4) {
          SET_ALPHA_PIXEL_ARGB( D, color.g, *S ); D++; S++;
          SET_ALPHA_PIXEL_ARGB( D, color.g, *S ); D++; S++;
          SET_ALPHA_PIXEL_ARGB( D, color.g, *S ); D++; S++;
          SET_ALPHA_PIXEL_ARGB( D, color.g, *S ); D++; S++;
      w-=4;
     }
     while (w--) {
          SET_ALPHA_PIXEL_ARGB( D, color.g, *S ); D++; S++;
     }
}

#define SET_ALPHA_PIXEL_A8(d,a)\
     switch (a) {\
     case 0xff: *(d) = 0xff;\
     case 0: break; \
     default: {\
          __u16 pixel = *(d);\
          __u16 s = (a)+1;\
          __u16 s1 = 256-s;\
          *(d) = (pixel * s1 + s) >> 8;\
          }\
     }

static void Bop_a8_set_alphapixel_Aop_a8()
{
     int    w = Dlength;
     __u8  *S = Bop;
     __u8  *D = Aop;

     while (w>4) {
          SET_ALPHA_PIXEL_A8 (D, *S); D++; S++;
          SET_ALPHA_PIXEL_A8 (D, *S); D++; S++;
          SET_ALPHA_PIXEL_A8 (D, *S); D++; S++;
          SET_ALPHA_PIXEL_A8 (D, *S); D++; S++;
      w -= 4;
     }
     while (w--) {
          SET_ALPHA_PIXEL_A8 (D, *S); D++; S++;
     }
}

#ifdef SUPPORT_RGB332

/* FIXME: implement correctly! */
#define SET_ALPHA_PIXEL_RGB332(d,a) \
     if (a > 127) \
          *(d) = PIXEL_RGB332( color.r, color.g, color.b );


static void Bop_a8_set_alphapixel_Aop_rgb332()
{
     int    w = Dlength;
     __u8  *S = Bop;
     __u8  *D = Aop;

     while (w>4) {
          SET_ALPHA_PIXEL_RGB332 (D, *S); D++; S++;
          SET_ALPHA_PIXEL_RGB332 (D, *S); D++; S++;
          SET_ALPHA_PIXEL_RGB332 (D, *S); D++; S++;
          SET_ALPHA_PIXEL_RGB332 (D, *S); D++; S++;
      w -= 4;
     }
     while (w--) {
          SET_ALPHA_PIXEL_RGB332 (D, *S); D++; S++;
     }
}
#endif

GFunc Bop_a8_set_alphapixel_Aop_PFI[] = {
     Bop_a8_set_alphapixel_Aop_rgb15,
     Bop_a8_set_alphapixel_Aop_rgb16,
     Bop_a8_set_alphapixel_Aop_rgb24,
     Bop_a8_set_alphapixel_Aop_rgb32,
     Bop_a8_set_alphapixel_Aop_argb,
     Bop_a8_set_alphapixel_Aop_a8,
     NULL,
#ifdef SUPPORT_RGB332
     Bop_a8_set_alphapixel_Aop_rgb332,
#else
     NULL,
#endif
     NULL,
     NULL,
     NULL
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



static void Cacc_add_to_Dacc_C()
{
     int          w = Dlength;
     Accumulator *D = Dacc;

     while (w--) {
          if (!(D->a & 0xF000)) {
               D->a += Cacc.a;
               D->r += Cacc.r;
               D->g += Cacc.g;
               D->b += Cacc.b;
          }
          D++;
     }
}

GFunc Cacc_add_to_Dacc = Cacc_add_to_Dacc_C;

static void Sacc_add_to_Dacc_C()
{
     int          w = Dlength;
     Accumulator *S = Sacc;
     Accumulator *D = Dacc;

     while (w--) {
          if (!(D->a & 0xF000)) {
               D->a += S->a;
               D->r += S->r;
               D->g += S->g;
               D->b += S->b;
          }
          D++;
          S++;
     }
}

GFunc Sacc_add_to_Dacc = Sacc_add_to_Dacc_C;

static void Sop_is_Aop() { Sop = Aop;}
static void Sop_is_Bop() { Sop = Bop;}

static void Sacc_is_NULL() { Sacc = NULL;}
static void Sacc_is_Aacc() { Sacc = Aacc;}
static void Sacc_is_Bacc() { Sacc = Bacc;}

static void Dacc_is_Aacc() { Dacc = Aacc;}
static void Dacc_is_Bacc() { Dacc = Bacc;}

static void Xacc_is_Aacc() { Xacc = Aacc;}
static void Xacc_is_Bacc() { Xacc = Bacc;}


/******************************************************************************/

void gGetDriverInfo( GraphicsDriverInfo *info )
{
     snprintf( info->name,
               DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH, "Software Driver" );

#ifdef USE_MMX
     if (dfb_mm_accel() & MM_MMX) {
          if (dfb_config->no_mmx) {
               INITMSG( "MMX detected, but disabled by --no-mmx \n");
          }
          else {
               gInit_MMX();

               snprintf( info->name, DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH,
                         "MMX Software Driver" );

               INITMSG( "MMX detected and enabled\n");
          }
     }
     else {
          INITMSG( "No MMX detected\n" );
     }
#endif

     snprintf( info->vendor, DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
               "convergence integrated media GmbH" );

     info->version.major = 0;
     info->version.minor = 5;
}

void gGetDeviceInfo( GraphicsDeviceInfo *info )
{
     snprintf( info->name, DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH,
               "Software Rasterizer" );

     snprintf( info->vendor, DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH,
               use_mmx ? "MMX" : "Generic" );

     info->caps.accel    = DFXL_NONE;
     info->caps.flags    = 0;
     info->caps.drawing  = DSDRAW_NOFX;
     info->caps.blitting = DSBLIT_NOFX;
}

int gAquire( CardState *state, DFBAccelerationMask accel )
{
     GFunc *funcs = gfuncs;
     int   pindex = DFB_PIXELFORMAT_INDEX(state->destination->format);

     DFBSurfaceLockFlags lock_flags;

     pthread_mutex_lock( &generic_lock );

     dst_bpp = DFB_BYTES_PER_PIXEL( state->destination->format );

     if (DFB_BLITTING_FUNCTION( accel )) {
          src_bpp = DFB_BYTES_PER_PIXEL( state->source->format );

          lock_flags = state->blittingflags & ( DSBLIT_BLEND_ALPHACHANNEL |
                                                DSBLIT_BLEND_COLORALPHA   |
                                                DSBLIT_DST_COLORKEY ) ?
                       DSLF_READ | DSLF_WRITE : DSLF_WRITE;
     }
     else
          lock_flags = state->drawingflags & ( DSDRAW_BLEND |
                                               DSDRAW_DST_COLORKEY ) ?
                       DSLF_READ | DSLF_WRITE : DSLF_WRITE;

     color = state->color;

     switch (state->destination->format) {
#ifdef SUPPORT_RGB332
          case DSPF_RGB332:
               Cop = PIXEL_RGB332( color.r, color.g, color.b );
               break;
#endif
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
          case DSPF_YUY2:
          case DSPF_UYVY:
          case DSPF_I420:
          case DSPF_YV12:
               break;
          default:
               ONCE("unsupported pixelformat");
               return 0;
     }

     /* Debug checks */
     if (!state->destination) {
          BUG("state check: no destination");
          return 0;
     }
     if (!state->source  &&  DFB_BLITTING_FUNCTION( accel )) {
          BUG("state check: no source");
          return 0;
     }
     
     dfb_surfacemanager_lock( dfb_gfxcard_surface_manager() );

     if (DFB_BLITTING_FUNCTION( accel )) {
          if (dfb_surface_software_lock( state->source,
                                         DSLF_READ, &src_org, &src_pitch, 1 )) {
               dfb_surfacemanager_unlock( dfb_gfxcard_surface_manager() );
               pthread_mutex_unlock( &generic_lock );
               return 0;
          }

          state->source_locked = 1;
     }
     else
          state->source_locked = 0;

     if (dfb_surface_software_lock( state->destination,
                                    lock_flags, &dst_org, &dst_pitch, 0 )) {

          if (DFB_BLITTING_FUNCTION( accel ))
                    dfb_surface_unlock( state->source, 1 );

          dfb_surfacemanager_unlock( dfb_gfxcard_surface_manager() );
          pthread_mutex_unlock( &generic_lock );
          return 0;
     }

     dfb_surfacemanager_unlock( dfb_gfxcard_surface_manager() );

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
                              if (state->drawingflags & DSDRAW_DST_COLORKEY) {
                                   Dkey = state->dst_colorkey;
                                   *funcs++ = Cop_toK_Aop_PFI[pindex];
                              }
                              else
                                   *funcs++ = Cop_to_Aop_PFI[pindex];
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
                    *funcs++ = Sacc_to_Aop_PFI[pindex];
               }
               else {
                    if (state->drawingflags & DSDRAW_DST_COLORKEY) {
                         Dkey = state->dst_colorkey;
                         *funcs++ = Cop_toK_Aop_PFI[pindex];
                    }
                    else
                         *funcs++ = Cop_to_Aop_PFI[pindex];
               }
               break;
          case DFXL_BLIT:
               if ((state->source->format == DSPF_A8) &&
                   (state->blittingflags ==
                     (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_COLORIZE)) &&
                   (state->src_blend == DSBF_SRCALPHA) &&
                   (state->dst_blend == DSBF_INVSRCALPHA))
               {
                    *funcs++ = Bop_a8_set_alphapixel_Aop_PFI[pindex];
                    break;
               }
          /* fallthru */
          case DFXL_STRETCHBLIT: {
                    int modulation = state->blittingflags & 0x7;

                    if (modulation) {

                         /* read the destination if needed */
                         if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL |
                                                     DSBLIT_BLEND_COLORALPHA) &&
                             state->dst_blend != DSBF_ZERO)
                         {
                              *funcs++ = Sop_is_Aop;
                              *funcs++ = Dacc_is_Aacc;
                              *funcs++ = Sop_PFI_to_Dacc[pindex];
                         }

                         /* read the source */
                         *funcs++ = Sop_is_Bop;
                         *funcs++ = Dacc_is_Bacc;
                         if (state->blittingflags & DSBLIT_SRC_COLORKEY) {
                              Skey = state->src_colorkey;
                              if (accel == DFXL_BLIT)
                                   *funcs++ = Sop_PFI_Kto_Dacc[DFB_PIXELFORMAT_INDEX(state->source->format)];
                              else
                                   *funcs++ = Sop_PFI_SKto_Dacc[DFB_PIXELFORMAT_INDEX(state->source->format)];
                         }
                         else {
                              if (accel == DFXL_BLIT)
                                   *funcs++ = Sop_PFI_to_Dacc[DFB_PIXELFORMAT_INDEX(state->source->format)];
                              else
                                   *funcs++ = Sop_PFI_Sto_Dacc[DFB_PIXELFORMAT_INDEX(state->source->format)];
                         }

                         /* modulate the source if requested */
                         if (Dacc_modulation[modulation]) {
                              /* modulation source */
                              Cacc.a = color.a + 1;
                              Cacc.r = color.r + 1;
                              Cacc.g = color.g + 1;
                              Cacc.b = color.b + 1;

                              *funcs++ = Dacc_modulation[modulation];
                         }

                         /* do blend functions and combine both accumulators */
                         if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL |
                                                     DSBLIT_BLEND_COLORALPHA))
                         {
                              /* Xacc will be blended and written to while
                                 Sacc and Dacc point to the SRC and DST
                                 as referenced by the blending functions */
                              *funcs++ = Sacc_is_Bacc;
                              *funcs++ = Dacc_is_Aacc;

                              /* blend the destination if needed */
                              if (state->dst_blend != DSBF_ZERO) {
                                   *funcs++ = Xacc_is_Aacc;
                                   *funcs++ = Xacc_blend[state->dst_blend - 1];
                              }

                              /* blend the source */
                              *funcs++ = Xacc_is_Bacc;
                              *funcs++ = Xacc_blend[state->src_blend - 1];

                              /* add the destination to the source */
                              if (state->dst_blend != DSBF_ZERO) {
                                   *funcs++ = Sacc_is_Aacc;
                                   *funcs++ = Dacc_is_Bacc;
                                   *funcs++ = Sacc_add_to_Dacc;
                              }
                         }

                         /* write source to destination */
                         *funcs++ = Sacc_is_Bacc;
                         *funcs++ = Sacc_to_Aop_PFI[pindex];
                    }
                    else if (state->source->format == state->destination->format) {
                         //*funcs++ = Sop_is_Bop;

                         if (accel == DFXL_BLIT) {
                              if (state->blittingflags & DSBLIT_SRC_COLORKEY) {
                                   Skey = state->src_colorkey;
                                   *funcs++ = Bop_PFI_Kto_Aop_PFI[pindex];
                              }
                              else
                                   *funcs++ = Bop_PFI_to_Aop_PFI[pindex];
                         }
                         else {
                              if (state->blittingflags & DSBLIT_SRC_COLORKEY) {
                                   Skey = state->src_colorkey;
                                   *funcs++ = Bop_PFI_SKto_Aop[pindex];
                              }
                              else
                                   *funcs++ = Bop_PFI_Sto_Aop[pindex];
                         }
                    }
                    else {
                         /* slow */
                         Sacc = Dacc = Aacc;

                         *funcs++ = Sop_is_Bop;

                         if (accel == DFXL_BLIT) {
                              if (state->blittingflags & DSBLIT_SRC_COLORKEY ) {
                                   Skey = state->src_colorkey;
                                   *funcs++ = Sop_PFI_Kto_Dacc[DFB_PIXELFORMAT_INDEX(state->source->format)];
                              }
                              else
                                   *funcs++ = Sop_PFI_to_Dacc[DFB_PIXELFORMAT_INDEX(state->source->format)];
                         }
                         else { // DFXL_STRETCHBLIT

                              if (state->blittingflags & DSBLIT_SRC_COLORKEY ) {
                                   Skey = state->src_colorkey;
                                   *funcs++ = Sop_PFI_SKto_Dacc[DFB_PIXELFORMAT_INDEX(state->source->format)];
                              }
                              else
                                   *funcs++ = Sop_PFI_Sto_Dacc[DFB_PIXELFORMAT_INDEX(state->source->format)];

                         }

                         *funcs++ = Sacc_to_Aop_PFI[pindex];
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
     dfb_surface_unlock( state->destination, 0 );

     if (state->source_locked)
          dfb_surface_unlock( state->source, 1 );

     pthread_mutex_unlock( &generic_lock );
}

#define CHECK_PIPELINE()           \
     {                             \
          if (!*gfuncs)            \
               return;             \
     }                             \

#define RUN_PIPELINE()             \
     {                             \
          GFunc *funcs = gfuncs;   \
                                   \
          do {                     \
               (*funcs++)();       \
          } while (*funcs);        \
     }                             \

void gFillRectangle( DFBRectangle *rect )
{
     CHECK_PIPELINE();

     Aop = dst_org  +  rect->y * dst_pitch  +  rect->x * dst_bpp;
     Dlength = rect->w;

     while (rect->h--) {
          RUN_PIPELINE();

          Aop += dst_pitch;
     }
}

void gDrawLine( DFBRegion *line )
{
     int i,dx,dy,sdy,dxabs,dyabs,x,y,px,py;

     CHECK_PIPELINE();

     /* the horizontal distance of the line */
     dx = line->x2 - line->x1;
     dxabs = abs(dx);

     /* the vertical distance of the line */
     dy = line->y2 - line->y1;
     dyabs = abs(dy);

     if (!dx || !dy) {              /* draw horizontal/vertical line */
          DFBRectangle rect = {
               MIN (line->x1, line->x2),
               MIN (line->y1, line->y2),
               dxabs + 1, dyabs + 1 };

          gFillRectangle( &rect );
          return;
     }

     sdy = SIGN(dy) * SIGN(dx) * dst_pitch;
     x   = dyabs >> 1;
     y   = dxabs >> 1;
     if (dx > 0) {
       px  = line->x1 * dst_bpp;
       py  = line->y1 * dst_pitch;
     } else {
       px  = line->x2 * dst_bpp;
       py  = line->y2 * dst_pitch;
     }

     if (dxabs >= dyabs) { /* the line is more horizontal than vertical */

          for (i=0, Dlength=1; i<dxabs; i++, Dlength++) {
               y += dyabs;
               if (y >= dxabs) {
                    Aop = dst_org + py + px;
                    RUN_PIPELINE();
                    px += Dlength * dst_bpp;
                    Dlength = 0;
                    y -= dxabs;
                    py += sdy;
               }
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
                    px += dst_bpp;
               }
               py += sdy;

               Aop = dst_org + py + px;
               RUN_PIPELINE();
          }
     }
}

void gBlit( DFBRectangle *rect, int dx, int dy )
{
     CHECK_PIPELINE();

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

     CHECK_PIPELINE();

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
     use_mmx = 1;

/********************************* Sop_PFI_Sto_Dacc ***************************/
     Sop_PFI_Sto_Dacc[DFB_PIXELFORMAT_INDEX(DSPF_ARGB)] = Sop_argb_Sto_Dacc_MMX;
/********************************* Sop_PFI_to_Dacc ****************************/
     Sop_PFI_to_Dacc[DFB_PIXELFORMAT_INDEX(DSPF_RGB16)] = Sop_rgb16_to_Dacc_MMX;
     Sop_PFI_to_Dacc[DFB_PIXELFORMAT_INDEX(DSPF_RGB32)] = Sop_rgb32_to_Dacc_MMX;
     Sop_PFI_to_Dacc[DFB_PIXELFORMAT_INDEX(DSPF_ARGB )] = Sop_argb_to_Dacc_MMX;
/********************************* Sacc_to_Aop_PFI ****************************/
     Sacc_to_Aop_PFI[DFB_PIXELFORMAT_INDEX(DSPF_RGB16)] = Sacc_to_Aop_rgb16_MMX;
     Sacc_to_Aop_PFI[DFB_PIXELFORMAT_INDEX(DSPF_RGB32)] = Sacc_to_Aop_rgb32_MMX;
/********************************* Xacc_blend *********************************/
     Xacc_blend[DSBF_SRCALPHA-1] = Xacc_blend_srcalpha_MMX;
     Xacc_blend[DSBF_INVSRCALPHA-1] = Xacc_blend_invsrcalpha_MMX;
/********************************* Dacc_modulation ****************************/
     Dacc_modulation[DSBLIT_BLEND_ALPHACHANNEL |
                     DSBLIT_BLEND_COLORALPHA |
                     DSBLIT_COLORIZE] = Dacc_modulate_argb_MMX;
/********************************* misc accumulator operations ****************/
     Cacc_add_to_Dacc = Cacc_add_to_Dacc_MMX;
     Sacc_add_to_Dacc = Sacc_add_to_Dacc_MMX;
}

#endif
