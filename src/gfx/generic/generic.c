/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dfb_types.h>

#include <pthread.h>

#include <directfb.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/gfxcard.h>
#include <core/state.h>
#include <core/surfacemanager.h>
#include <core/palette.h>

#include <misc/gfx_util.h>
#include <misc/util.h>
#include <misc/conf.h>
#include <misc/mem.h>
#include <misc/memcpy.h>
#include <misc/cpu_accel.h>

#include <gfx/convert.h>
#include <gfx/util.h>

#include "generic.h"

/* lookup tables for 2/3bit to 8bit color conversion */
static const __u8 lookup3to8[] = { 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff};
static const __u8 lookup2to8[] = { 0x00, 0x55, 0xaa, 0xff};

static int use_mmx = 0;

#ifdef USE_MMX
static void gInit_MMX();
#endif

/********************************* Cop_to_Aop_PFI *****************************/

static void Cop_to_Aop_8( GenefxState *gfxs )
{
     memset( gfxs->Aop, gfxs->Cop, gfxs->length );
}

static void Cop_to_Aop_16( GenefxState *gfxs )
{
     int    w;
     int    l   = gfxs->length;
     __u32 *D   = gfxs->Aop;
     __u32  Cop = gfxs->Cop;

     __u32 DCop = ((Cop << 16) | Cop);

     if (((long)D)&2) {         /* align */
          __u16* tmp = (__u16*) D;
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

static void Cop_to_Aop_24( GenefxState *gfxs )
{
     int   w = gfxs->length;
     __u8 *D = gfxs->Aop;

     while (w) {
          D[0] = gfxs->color.b;
          D[1] = gfxs->color.g;
          D[2] = gfxs->color.r;

          D += 3;
          --w;
     }
}

static void Cop_to_Aop_32( GenefxState *gfxs )
{
     int    w   = gfxs->length;
     __u32 *D   = gfxs->Aop;
     __u32  Cop = gfxs->Cop;

     while (w--)
          *D++ = Cop;
}

static GenefxFunc Cop_to_Aop_PFI[DFB_NUM_PIXELFORMATS] = {
     Cop_to_Aop_16,
     Cop_to_Aop_16,
     Cop_to_Aop_24,
     Cop_to_Aop_32,
     Cop_to_Aop_32,
     Cop_to_Aop_8,
     Cop_to_Aop_32,
     Cop_to_Aop_8,
     Cop_to_Aop_32,
     Cop_to_Aop_8,
     Cop_to_Aop_8,
     Cop_to_Aop_8,
     Cop_to_Aop_8,
     Cop_to_Aop_32
};

/********************************* Cop_toK_Aop_PFI ****************************/

static void Cop_toK_Aop_8( GenefxState *gfxs )
{
     int    w    = gfxs->length;
     __u8  *D    = gfxs->Aop;
     __u32  Cop  = gfxs->Cop;
     __u32  Dkey = gfxs->Dkey;

     while (w--) {
          if (Dkey == *D)
               *D = Cop;

          D++;
     }
}

static void Cop_toK_Aop_16( GenefxState *gfxs )
{
     int    w    = gfxs->length;
     __u16 *D    = gfxs->Aop;
     __u32  Cop  = gfxs->Cop;
     __u32  Dkey = gfxs->Dkey;

     while (w--) {
          if (Dkey == *D)
               *D = Cop;

          D++;
     }
}

static void Cop_toK_Aop_24( GenefxState *gfxs )
{
     ONCE("Cop_toK_Aop_24() unimplemented");
}

static void Cop_toK_Aop_32( GenefxState *gfxs )
{
     int    w    = gfxs->length;
     __u32 *D    = gfxs->Aop;
     __u32  Cop  = gfxs->Cop;
     __u32  Dkey = gfxs->Dkey;

     while (w--) {
          if (Dkey == (*D & 0xffffff))
               *D = Cop;

          D++;
     }
}

static void Cop_toK_Aop_alut44( GenefxState *gfxs )
{
     int    w    = gfxs->length;
     __u8  *D    = gfxs->Aop;
     __u32  Cop  = gfxs->Cop;
     __u32  Dkey = gfxs->Dkey;

     while (w--) {
          if (Dkey == (*D & 0x0F))
               *D = Cop;

          D++;
     }
}

static GenefxFunc Cop_toK_Aop_PFI[DFB_NUM_PIXELFORMATS] = {
     Cop_toK_Aop_16,
     Cop_toK_Aop_16,
     Cop_toK_Aop_24,
     Cop_toK_Aop_32,
     Cop_toK_Aop_32,
     Cop_toK_Aop_8,
     NULL,
     Cop_toK_Aop_8,
     NULL,
     NULL,
     NULL,
     Cop_toK_Aop_8,
     Cop_toK_Aop_alut44,
     Cop_toK_Aop_32
};

/********************************* Bop_PFI_to_Aop_PFI *************************/

static void Bop_8_to_Aop( GenefxState *gfxs )
{
     dfb_memmove( gfxs->Aop, gfxs->Bop, gfxs->length );
}

static void Bop_16_to_Aop( GenefxState *gfxs )
{
     dfb_memmove( gfxs->Aop, gfxs->Bop, gfxs->length*2 );
}

static void Bop_24_to_Aop( GenefxState *gfxs )
{
     dfb_memmove( gfxs->Aop, gfxs->Bop, gfxs->length*3 );
}

static void Bop_32_to_Aop( GenefxState *gfxs )
{
     dfb_memmove( gfxs->Aop, gfxs->Bop, gfxs->length*4 );
}

static GenefxFunc Bop_PFI_to_Aop_PFI[DFB_NUM_PIXELFORMATS] = {
     Bop_16_to_Aop,      /* DSPF_ARGB1555 */
     Bop_16_to_Aop,      /* DSPF_RGB16 */
     Bop_24_to_Aop,      /* DSPF_RGB24 */
     Bop_32_to_Aop,      /* DSPF_RGB32 */
     Bop_32_to_Aop,      /* DSPF_ARGB */
     Bop_8_to_Aop,       /* DSPF_A8 */
     Bop_16_to_Aop,      /* DSPF_YUY2 */
     Bop_8_to_Aop,       /* DSPF_RGB332 */
     Bop_16_to_Aop,      /* DSPF_UYVY */
     Bop_8_to_Aop,       /* DSPF_I420 */
     Bop_8_to_Aop,       /* DSPF_YV12 */
     Bop_8_to_Aop,       /* DSPF_LUT8 */
     Bop_8_to_Aop,       /* DSPF_ALUT44 */
     Bop_32_to_Aop       /* DSPF_AiRGB */
};

/********************************* Bop_PFI_Kto_Aop_PFI ************************/

/* FIXME: take care of gfxs->Ostep! */
static void Bop_rgb15_Kto_Aop( GenefxState *gfxs )
{
     int    w;
     int    l    = gfxs->length;
     __u32 *D    = gfxs->Aop;
     __u32 *S    = gfxs->Bop;
     __u32  Skey = gfxs->Skey;

     __u32 DSkey = (Skey << 16) | Skey;

     if (((long)D)&2) {         /* align */
          __u16 *tmp = gfxs->Aop;
          --l;
          if ((*((__u16*)S) & 0x7FFF) != Skey)
               *tmp = *((__u16*)S);

          D = (__u32*)((__u16*)D+1);
          S = (__u32*)((__u16*)S+1);
     }

     w = (l >> 1);
     while (w) {
          __u32 dpixel = *S;
          __u16 *tmp = (__u16*)D;

          if ((dpixel & 0x7FFF7FFF) != DSkey) {
               if ((dpixel & 0x7FFF0000) != (DSkey & 0x7FFF0000)) {
                    if ((dpixel & 0x00007FFF) != (DSkey & 0x00007FFF)) {
                         *D = dpixel;
                    }
                    else {
#ifdef WORDS_BIGENDIAN
                         tmp[0] = (__u16)(dpixel >> 16);
#else
                         tmp[1] = (__u16)(dpixel >> 16);
#endif
                    }
               }
               else {
#ifdef WORDS_BIGENDIAN
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
          if ((*((__u16*)S) & 0x7FFF) != Skey)
               *((__u16*)D) = *((__u16*)S);
     }
}

/* FIXME: take care of gfxs->Ostep! */
static void Bop_rgb16_Kto_Aop( GenefxState *gfxs )
{
     int    w, l = gfxs->length;
     __u32 *D    = gfxs->Aop;
     __u32 *S    = gfxs->Bop;
     __u32  Skey = gfxs->Skey;

     __u32 DSkey = (Skey << 16) | Skey;

     if (((long)D)&2) {         /* align */
          __u16 *tmp = gfxs->Aop;
          --l;
          if (*((__u16*)S) != Skey)
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
#ifdef WORDS_BIGENDIAN
                         tmp[0] = (__u16)(dpixel >> 16);
#else
                         tmp[1] = (__u16)(dpixel >> 16);
#endif
                    }
               }
               else {
#ifdef WORDS_BIGENDIAN
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
          if (*((__u16*)S) != Skey)
               *((__u16*)D) = *((__u16*)S);
     }
}

static void Bop_rgb24_Kto_Aop( GenefxState *gfxs )
{
     int    w     = gfxs->length;
     __u8  *D     = gfxs->Aop;
     __u8  *S     = gfxs->Bop;
     __u32  Skey  = gfxs->Skey;
     int    Ostep = gfxs->Ostep;

     if (Ostep < 0) {
          D += (gfxs->length - 1) * 3;
          S += (gfxs->length - 1) * 3;
     }

     while (w--) {
          __u8 b = *S;
          __u8 g = *(S+1);
          __u8 r = *(S+2);

          if (Skey != (__u32)(r<<16 | g<<8 | b )) {
               *D     = b;
               *(D+1) = g;
               *(D+2) = r;
          }

          S += Ostep * 3;
          D += Ostep * 3;
     }
}

static void Bop_rgb32_Kto_Aop( GenefxState *gfxs )
{
     int    w     = gfxs->length;
     __u32 *D     = gfxs->Aop;
     __u32 *S     = gfxs->Bop;
     __u32  Skey  = gfxs->Skey;
     int    Ostep = gfxs->Ostep;

     if (Ostep < 0) {
          D += gfxs->length - 1;
          S += gfxs->length - 1;
     }

     while (w--) {
          __u32 spixel = *S;

          if ((spixel & 0xffffff) != Skey)
               *D = spixel;

          S += Ostep;
          D += Ostep;
     }
}

static void Bop_a8_Kto_Aop( GenefxState *gfxs )
{
     /* no color to key */
     dfb_memmove( gfxs->Aop, gfxs->Bop, gfxs->length );
}

static void Bop_8_Kto_Aop( GenefxState *gfxs )
{
     int    i;
     int    w    = gfxs->length;
     __u8  *D    = gfxs->Aop;
     __u8  *S    = gfxs->Bop;
     __u32  Skey = gfxs->Skey;

     if (gfxs->Ostep < 0) {
          for (i=w-1; i>=0; --i) {
               register __u32 pixel = S[i];

               if (pixel != Skey)
                    D[i] = pixel;
          }
     }
#ifndef ARCH_X86
     else if (((long) S & 3) == ((long) D & 3)) {
          __u32 *D32;
          __u32 *S32;
          __u32  Skey32;
          int    n = 0;

          /* Align source & destination to 32 bit. */
          if ((long) D & 3) {
               n = 4 - ((long) D & 3);

               for (i=0; i<n; i++) {
                    register __u32 pixel = S[i];

                    if (pixel != Skey)
                         D[i] = pixel;
               }

               w -= n;
          }

          D32 = gfxs->Aop + n;
          S32 = gfxs->Bop + n;

          /* Fill 32 bit with 8 bit key. */
          Skey32 = (Skey << 24) | (Skey << 16) | (Skey << 8) | Skey;

          /* Compare four pixels at once. */
          n = w >> 2;
          for (i=0; i<n; i++) {
               __u32 P32 = S32[i];
               __u32 X32 = P32 ^ Skey32;

               if (X32) {
                    /* Check for common case. */
                    if ((X32 & 0xff000000) &&
                        (X32 & 0x00ff0000) &&
                        (X32 & 0x0000ff00) &&
                        (X32 & 0x000000ff))
                    {
                         /* Copy all four pixels. */
                         D32[i] = P32;
                    }
                    else {
                         /* Copy one by one. */
                         D = (__u8*) (D32 + i);

#ifdef WORDS_BIGENDIAN
                         if (X32 & 0xff000000)
                              D[0] = (__u8)P32 >> 24;

                         if (X32 & 0x00ff0000)
                              D[1] = P32 >> 16;

                         if (X32 & 0x0000ff00)
                              D[2] = P32 >> 8;

                         if (X32 & 0x000000ff)
                              D[3] = P32;

#else

                         if (X32 & 0x000000ff)
                              D[0] = P32;

                         if (X32 & 0x0000ff00)
                              D[1] = P32 >> 8;

                         if (X32 & 0x00ff0000)
                              D[2] = P32 >> 16;

                         if (X32 & 0xff000000)
                              D[3] = P32 >> 24;
#endif
                    }
               }
          }

          /* Copy remaining pixels one by one. */
          if (w & 3) {
               D = (__u8*) (D32 + n);
               S = (__u8*) (S32 + n);

               n = w & 3;

               for (i=0; i<n; i++) {
                    register __u32 pixel = S[i];

                    if (pixel != Skey)
                         D[i] = pixel;
               }
          }
     }
     else {
          register __u32 out = 0;     /* Collect output pixels. */
          register int   num = 0;     /* Count output pixels. */

          /* Align destination to 32 bit. */
          if (((long) D & 3) && w > 4) {
               int n = 4 - ((long) D & 3);

               for (i=0; i<n; i++) {
                    register __u32 pixel = S[i];

                    if (pixel != Skey)
                         D[i] = pixel;
               }

               w -= n;
               D += n;
               S += n;
          }

          /* Write up to 32 bit at once. */
          for (i=0; i<w; ++i) {
               register __u32 pixel = S[i];

               if (pixel != Skey) {
#ifdef WORDS_BIGENDIAN
                    out = (out << 8) | pixel;
#else
                    out = (out >> 8) | (pixel << 24);
#endif
                    if (++num == 4) {
                         *(__u32*)(D+i-3) = out;

                         num = 0;
                    }
                    else if ((i & 3) == 3) {
                         /* Flush 'out' to realign destination to 32 bit. */
                         switch (num) {
                              case 3:
                                   D[i-2] = out >> 16;
                              case 2:
                                   D[i-1] = out >> 8;
                              case 1:
                                   D[i] = out;
                         }

                         num = 0;
                    }
               }
               else while (num) {
                    /* Flush 'out' due to a keyed pixel. */
                    switch (num) {
                         case 3:
                              D[i-3] = out >> 16;
                         case 2:
                              D[i-2] = out >> 8;
                         case 1:
                              D[i-1] = out;
                    }

                    num = 0;
               }
          }

          /* Flush 'out'. */
          if (num) {
               switch (num) {
                    case 3:
                         D[i-3] = out >> 16;
                    case 2:
                         D[i-2] = out >> 8;
                    case 1:
                         D[i-1] = out;
               }
          }
     }
#else
     else {
          for (i=0; i<w; ++i) {
               register __u32 pixel = S[i];

               if (pixel != Skey)
                    D[i] = pixel;
          }
     }
#endif
}

static void Bop_alut44_Kto_Aop( GenefxState *gfxs )
{
     int    w     = gfxs->length;
     __u8  *D     = gfxs->Aop;
     __u8  *S     = gfxs->Bop;
     __u32  Skey  = gfxs->Skey;
     int    Ostep = gfxs->Ostep;

     if (Ostep < 0) {
          D += gfxs->length - 1;
          S += gfxs->length - 1;
     }

     while (w--) {
          __u8 spixel = *S;

          if ((spixel & 0x0F) != Skey)
               *D = spixel;

          S += Ostep;
          D += Ostep;
     }
}

static GenefxFunc Bop_PFI_Kto_Aop_PFI[DFB_NUM_PIXELFORMATS] = {
     Bop_rgb15_Kto_Aop,
     Bop_rgb16_Kto_Aop,
     Bop_rgb24_Kto_Aop,
     Bop_rgb32_Kto_Aop,
     Bop_rgb32_Kto_Aop,
     Bop_a8_Kto_Aop,
     NULL,
     Bop_8_Kto_Aop,
     NULL,
     NULL,
     NULL,
     Bop_8_Kto_Aop,
     Bop_alut44_Kto_Aop,
     Bop_rgb32_Kto_Aop
};

/********************************* Bop_PFI_Sto_Aop_PFI ****************************/

static void Bop_16_Sto_Aop( GenefxState *gfxs )
{
     int    w2;
     int    w      = gfxs->length;
     int    i      = 0;
     __u32 *D      = gfxs->Aop;
     __u16 *S      = gfxs->Bop;
     int    SperD  = gfxs->SperD;
     int    SperD2 = SperD << 1;

     if (((long)D)&2) {
          *(__u16*)D = *S;
          i += SperD;
          w--;
          D = gfxs->Aop + 2;
     }

     w2 = (w >> 1);
     while (w2--) {
#ifdef WORDS_BIGENDIAN
          *D++ =  S[i>>16] << 16 | S[(i+SperD)>>16];
#else
          *D++ = (S[(i+SperD)>>16] << 16) | S[i>>16];
#endif
          i += SperD2;
     }
     if (w&1) {
          *(__u16*)D = S[i>>16];
     }
}

static void Bop_24_Sto_Aop( GenefxState *gfxs )
{
     int   w     = gfxs->length;
     int   i     = 0;
     __u8 *D     = gfxs->Aop;
     __u8 *S     = gfxs->Bop;
     int   SperD = gfxs->SperD;

     while (w--) {
          int pixelstart = (i>>16)*3;

          *D++ = S[pixelstart+0];
          *D++ = S[pixelstart+1];
          *D++ = S[pixelstart+2];

          i += SperD;
     }
}

static void Bop_32_Sto_Aop( GenefxState *gfxs )
{
     int    w     = gfxs->length;
     int    i     = 0;
     __u32 *D     = gfxs->Aop;
     __u32 *S     = gfxs->Bop;
     int    SperD = gfxs->SperD;

     while (w--) {
          *D++ = S[i>>16];

          i += SperD;
     }
}

static void Bop_8_Sto_Aop( GenefxState *gfxs )
{
     int   w     = gfxs->length;
     int   i     = 0;
     __u8 *D     = gfxs->Aop;
     __u8 *S     = gfxs->Bop;
     int   SperD = gfxs->SperD;

     while (w--) {
          *D++ = S[i>>16];

          i += SperD;
     }
}

static GenefxFunc Bop_PFI_Sto_Aop_PFI[DFB_NUM_PIXELFORMATS] = {
     Bop_16_Sto_Aop,
     Bop_16_Sto_Aop,
     Bop_24_Sto_Aop,
     Bop_32_Sto_Aop,
     Bop_32_Sto_Aop,
     Bop_8_Sto_Aop,
     NULL,
     Bop_8_Sto_Aop,
     NULL,
     NULL,
     NULL,
     Bop_8_Sto_Aop,
     Bop_8_Sto_Aop,
     Bop_32_Sto_Aop
};

/********************************* Bop_PFI_SKto_Aop_PFI ***************************/

static void Bop_rgb15_SKto_Aop( GenefxState *gfxs )
{
     int    w     = gfxs->length;
     int    i     = 0;
     __u16 *D     = gfxs->Aop;
     __u16 *S     = gfxs->Bop;
     __u32  Skey  = gfxs->Skey;
     int    SperD = gfxs->SperD;

     while (w--) {
          __u16 s = S[i>>16] & 0x7FFF;

          if (s != Skey)
               *D = s;

          D++;
          i += SperD;
     }
}

static void Bop_rgb16_SKto_Aop( GenefxState *gfxs )
{
     int    w     = gfxs->length;
     int    i     = 0;
     __u16 *D     = gfxs->Aop;
     __u16 *S     = gfxs->Bop;
     __u32  Skey  = gfxs->Skey;
     int    SperD = gfxs->SperD;

     while (w--) {
          __u16 s = S[i>>16];

          if (s != Skey)
               *D = s;

          D++;
          i += SperD;
     }
}

static void Bop_rgb24_SKto_Aop( GenefxState *gfxs )
{
     int    w     = gfxs->length;
     int    i     = 0;
     __u8  *D     = gfxs->Aop;
     __u8  *S     = gfxs->Bop;
     __u32  Skey  = gfxs->Skey;
     int    SperD = gfxs->SperD;

     while (w--) {
          int pixelstart = (i>>16)*3;

          __u8 b = S[pixelstart+0];
          __u8 g = S[pixelstart+1];
          __u8 r = S[pixelstart+2];

          if (Skey != (__u32)(r<<16 | g<<8 | b )) {
               *D     = b;
               *(D+1) = g;
               *(D+2) = r;
          }

          D += 3;
          i += SperD;
     }
}

static void Bop_rgb32_SKto_Aop( GenefxState *gfxs )
{
     int    w     = gfxs->length;
     int    i     = 0;
     __u32 *D     = gfxs->Aop;
     __u32 *S     = gfxs->Bop;
     __u32  Skey  = gfxs->Skey;
     int    SperD = gfxs->SperD;

     while (w--) {
          __u32 s = S[i>>16];

          if ((s & 0xffffff) != Skey)
               *D = s;

          D++;
          i += SperD;
     }
}

static void Bop_a8_SKto_Aop( GenefxState *gfxs )
{
     int   w     = gfxs->length;
     int   i     = 0;
     __u8 *D     = gfxs->Aop;
     __u8 *S     = gfxs->Bop;
     int   SperD = gfxs->SperD;

     /* no color to key */
     while (w--) {
          *D++ = S[i>>16];

          i += SperD;
     }
}

static void Bop_8_SKto_Aop( GenefxState *gfxs )
{
     int    w     = gfxs->length;
     int    i     = 0;
     __u8  *D     = gfxs->Aop;
     __u8  *S     = gfxs->Bop;
     __u32  Skey  = gfxs->Skey;
     int    SperD = gfxs->SperD;

     while (w--) {
          __u8 s = S[i>>16];

          if (s != Skey)
               *D = s;

          D++;
          i += SperD;
     }
}

static void Bop_alut44_SKto_Aop( GenefxState *gfxs )
{
     int    w     = gfxs->length;
     int    i     = 0;
     __u8  *D     = gfxs->Aop;
     __u8  *S     = gfxs->Bop;
     __u32  Skey  = gfxs->Skey;
     int    SperD = gfxs->SperD;

     while (w--) {
          __u8 s = S[i>>16];

          if ((s & 0x0f) != Skey)
               *D = s;

          D++;
          i += SperD;
     }
}

static GenefxFunc Bop_PFI_SKto_Aop_PFI[DFB_NUM_PIXELFORMATS] = {
     Bop_rgb15_SKto_Aop,
     Bop_rgb16_SKto_Aop,
     Bop_rgb24_SKto_Aop,
     Bop_rgb32_SKto_Aop,
     Bop_rgb32_SKto_Aop,
     Bop_a8_SKto_Aop,
     NULL,
     Bop_8_SKto_Aop,
     NULL,
     NULL,
     NULL,
     Bop_8_SKto_Aop,
     Bop_alut44_SKto_Aop,
     Bop_rgb32_SKto_Aop
};

/********************************* Sop_PFI_Sto_Dacc ***************************/

static void Sop_argb1555_Sto_Dacc( GenefxState *gfxs )
{
     int w     = gfxs->length;
     int i     = 0;
     int SperD = gfxs->SperD;

     GenefxAccumulator *D = gfxs->Dacc;
     __u16             *S = gfxs->Sop;

     while (w--) {
          __u16 s = S[i>>16];

          D->a = (s & 0x8000) ? 0xff : 0;
          D->r = (s & 0x7C00) >> 7;
          D->g = (s & 0x03E0) >> 2;
          D->b = (s & 0x001F) << 3;

          i += SperD;

          D++;
     }
}

static void Sop_rgb16_Sto_Dacc( GenefxState *gfxs )
{
     int w     = gfxs->length;
     int i     = 0;
     int SperD = gfxs->SperD;

     GenefxAccumulator *D = gfxs->Dacc;
     __u16             *S = gfxs->Sop;

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

static void Sop_rgb24_Sto_Dacc( GenefxState *gfxs )
{
     int w     = gfxs->length;
     int i     = 0;
     int SperD = gfxs->SperD;

     GenefxAccumulator *D = gfxs->Dacc;
     __u8              *S = gfxs->Sop;

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

static void Sop_rgb32_Sto_Dacc( GenefxState *gfxs )
{
     int w     = gfxs->length;
     int i     = 0;
     int SperD = gfxs->SperD;

     GenefxAccumulator *D = gfxs->Dacc;
     __u32             *S = gfxs->Sop;

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


static void Sop_argb_Sto_Dacc( GenefxState *gfxs )
{
     int w     = gfxs->length;
     int i     = 0;
     int SperD = gfxs->SperD;

     GenefxAccumulator *D = gfxs->Dacc;
     __u32             *S = gfxs->Sop;

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

static void Sop_airgb_Sto_Dacc( GenefxState *gfxs )
{
     int w     = gfxs->length;
     int i     = 0;
     int SperD = gfxs->SperD;

     GenefxAccumulator *D = gfxs->Dacc;
     __u32             *S = gfxs->Sop;

     while (w--) {
          __u32 s = S[i>>16];

          D->a = ((s & 0xFF000000) >> 24) ^ 0xff;
          D->r = (s & 0x00FF0000) >> 16;
          D->g = (s & 0x0000FF00) >>  8;
          D->b = (s & 0x000000FF);

          i += SperD;

          D++;
     }
}

static void Sop_a8_Sto_Dacc( GenefxState *gfxs )
{
     int w     = gfxs->length;
     int i     = 0;
     int SperD = gfxs->SperD;

     GenefxAccumulator *D = gfxs->Dacc;
     __u8              *S = gfxs->Sop;

     while (w--) {
          __u8 s = S[i>>16];

          D->a = s;
          D->r = 0xFF;
          D->g = 0xFF;
          D->b = 0xFF;

          i += SperD;

          D++;
     }
}

static void Sop_rgb332_Sto_Dacc( GenefxState *gfxs )
{
     int w     = gfxs->length;
     int i     = 0;
     int SperD = gfxs->SperD;

     GenefxAccumulator *D = gfxs->Dacc;
     __u8              *S = gfxs->Sop;

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

static void Sop_lut8_Sto_Dacc( GenefxState *gfxs )
{
     int w     = gfxs->length;
     int i     = 0;
     int SperD = gfxs->SperD;

     GenefxAccumulator *D = gfxs->Dacc;
     __u8              *S = gfxs->Sop;

     DFBColor *entries = gfxs->Slut->entries;

     while (w--) {
          __u8 s = S[i>>16];

          D->a = entries[s].a;
          D->r = entries[s].r;
          D->g = entries[s].g;
          D->b = entries[s].b;

          i += SperD;

          D++;
     }
}

static void Sop_alut44_Sto_Dacc( GenefxState *gfxs )
{
     int w     = gfxs->length;
     int i     = 0;
     int SperD = gfxs->SperD;

     GenefxAccumulator *D = gfxs->Dacc;
     __u8              *S = gfxs->Sop;

     DFBColor *entries = gfxs->Slut->entries;

     while (w--) {
          __u8 s = S[i>>16];

          D->a = s & 0xF0;
          s &= 0x0F;
          D->r = entries[s].r;
          D->g = entries[s].g;
          D->b = entries[s].b;

          i += SperD;

          D++;
     }
}

static GenefxFunc Sop_PFI_Sto_Dacc[DFB_NUM_PIXELFORMATS] = {
     Sop_argb1555_Sto_Dacc,
     Sop_rgb16_Sto_Dacc,
     Sop_rgb24_Sto_Dacc,
     Sop_rgb32_Sto_Dacc,
     Sop_argb_Sto_Dacc,
     Sop_a8_Sto_Dacc,
     NULL,
     Sop_rgb332_Sto_Dacc,
     NULL,
     NULL,
     NULL,
     Sop_lut8_Sto_Dacc,
     Sop_alut44_Sto_Dacc,
     Sop_airgb_Sto_Dacc
};

/********************************* Sop_PFI_SKto_Dacc **************************/

static void Sop_argb1555_SKto_Dacc( GenefxState *gfxs )
{
     int w     = gfxs->length;
     int i     = 0;
     int SperD = gfxs->SperD;

     GenefxAccumulator *D    = gfxs->Dacc;
     __u16             *S    = gfxs->Sop;
     __u32              Skey = gfxs->Skey;

     while (w--) {
          __u16 s = S[i>>16];

          if ((s & 0x7fff) != Skey) {
               D->a = (s & 0x8000) ? 0xff : 0;
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

static void Sop_rgb16_SKto_Dacc( GenefxState *gfxs )
{
     int w     = gfxs->length;
     int i     = 0;
     int SperD = gfxs->SperD;

     GenefxAccumulator *D    = gfxs->Dacc;
     __u16             *S    = gfxs->Sop;
     __u32              Skey = gfxs->Skey;

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

static void Sop_rgb24_SKto_Dacc( GenefxState *gfxs )
{
     int w     = gfxs->length;
     int i     = 0;
     int SperD = gfxs->SperD;

     GenefxAccumulator *D    = gfxs->Dacc;
     __u8              *S    = gfxs->Sop;
     __u32              Skey = gfxs->Skey;

     while (w--) {
          int pixelstart = (i>>16)*3;

          __u8 b = S[pixelstart+0];
          __u8 g = S[pixelstart+1];
          __u8 r = S[pixelstart+2];

          if (Skey != (__u32)(r<<16 | g<<8 | b )) {
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

static void Sop_rgb32_SKto_Dacc( GenefxState *gfxs )
{
     int w     = gfxs->length;
     int i     = 0;
     int SperD = gfxs->SperD;

     GenefxAccumulator *D    = gfxs->Dacc;
     __u32             *S    = gfxs->Sop;
     __u32              Skey = gfxs->Skey;

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


static void Sop_argb_SKto_Dacc( GenefxState *gfxs )
{
     int w     = gfxs->length;
     int i     = 0;
     int SperD = gfxs->SperD;

     GenefxAccumulator *D    = gfxs->Dacc;
     __u32             *S    = gfxs->Sop;
     __u32              Skey = gfxs->Skey;

     while (w--) {
          __u32 s = S[i>>16];

          if ((s & 0xffffff) != Skey) {
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

static void Sop_airgb_SKto_Dacc( GenefxState *gfxs )
{
     int w     = gfxs->length;
     int i     = 0;
     int SperD = gfxs->SperD;

     GenefxAccumulator *D    = gfxs->Dacc;
     __u32             *S    = gfxs->Sop;
     __u32              Skey = gfxs->Skey;

     while (w--) {
          __u32 s = S[i>>16];

          if ((s & 0xffffff) != Skey) {
               D->a = ((s & 0xFF000000) >> 24) ^ 0xff;
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

static void Sop_a8_SKto_Dacc( GenefxState *gfxs )
{
     int w     = gfxs->length;
     int i     = 0;
     int SperD = gfxs->SperD;

     GenefxAccumulator *D = gfxs->Dacc;
     __u8              *S = gfxs->Sop;

     /* no color to key */
     while (w--) {
          __u8 s = S[i>>16];

          D->a = s;
          D->r = 0xFF;
          D->g = 0xFF;
          D->b = 0xFF;

          i += SperD;

          D++;
     }
}

static void Sop_lut8_SKto_Dacc( GenefxState *gfxs )
{
     int w     = gfxs->length;
     int i     = 0;
     int SperD = gfxs->SperD;

     GenefxAccumulator *D    = gfxs->Dacc;
     __u8              *S    = gfxs->Sop;
     __u32              Skey = gfxs->Skey;

     DFBColor *entries = gfxs->Slut->entries;

     while (w--) {
          __u8 s = S[i>>16];

          if (s != Skey) {
               D->a = entries[s].a;
               D->r = entries[s].r;
               D->g = entries[s].g;
               D->b = entries[s].b;
          }
          else
               D->a = 0xF000;

          i += SperD;

          D++;
     }
}

static void Sop_alut44_SKto_Dacc( GenefxState *gfxs )
{
     int w     = gfxs->length;
     int i     = 0;
     int SperD = gfxs->SperD;

     GenefxAccumulator *D    = gfxs->Dacc;
     __u8              *S    = gfxs->Sop;
     __u32              Skey = gfxs->Skey;

     DFBColor *entries = gfxs->Slut->entries;

     while (w--) {
          __u8 s = S[i>>16];

          if ((s & 0x0F) != Skey) {
               D->a = ((s & 0xF0) >> 4) | (s & 0xF0);
               s &= 0x0F;
               D->r = entries[s].r;
               D->g = entries[s].g;
               D->b = entries[s].b;
          }
          else
               D->a = 0xF000;

          i += SperD;

          D++;
     }
}

static GenefxFunc Sop_PFI_SKto_Dacc[DFB_NUM_PIXELFORMATS] = {
     Sop_argb1555_SKto_Dacc,
     Sop_rgb16_SKto_Dacc,
     Sop_rgb24_SKto_Dacc,
     Sop_rgb32_SKto_Dacc,
     Sop_argb_SKto_Dacc,
     Sop_a8_SKto_Dacc,
     NULL,
     NULL,     /* FIXME: RGB332 */
     NULL,
     NULL,
     NULL,
     Sop_lut8_SKto_Dacc,
     Sop_alut44_SKto_Dacc,
     Sop_airgb_SKto_Dacc
};

/********************************* Sop_PFI_to_Dacc ****************************/

static void Sop_argb1555_to_Dacc( GenefxState *gfxs )
{
     int                l;
     int                w = gfxs->length;
     GenefxAccumulator *D = gfxs->Dacc;
     __u16             *S = gfxs->Sop;

     if (((long)S)&2) {
          __u16 spixel = *S;

          D->a = (spixel & 0x8000) ? 0xff : 0;
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

#ifdef WORDS_BIGENDIAN
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

static void Sop_rgb16_to_Dacc( GenefxState *gfxs )
{
     int                l;
     int                w = gfxs->length;
     GenefxAccumulator *D = gfxs->Dacc;
     __u16             *S = gfxs->Sop;

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

#ifdef WORDS_BIGENDIAN
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

static void Sop_rgb24_to_Dacc( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *D = gfxs->Dacc;
     __u8              *S = gfxs->Sop;

     while (w--) {
          D->a = 0xFF;
          D->b = *S++;
          D->g = *S++;
          D->r = *S++;

          D++;
     }
}

static void Sop_a8_to_Dacc( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *D = gfxs->Dacc;
     __u8              *S = gfxs->Sop;

     while (w--) {
          D->a = *S++;
          D->r = 0xFF;
          D->g = 0xFF;
          D->b = 0xFF;

          D++;
     }
}

static void Sop_rgb32_to_Dacc( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *D = gfxs->Dacc;
     __u32             *S = gfxs->Sop;

     while (w--) {
          __u32 s = *S++;

          D->a = 0xFF;
          D->r = (s & 0xFF0000) >> 16;
          D->g = (s & 0x00FF00) >>  8;
          D->b = (s & 0x0000FF);

          D++;
     }
}

static void Sop_argb_to_Dacc( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *D = gfxs->Dacc;
     __u32             *S = gfxs->Sop;

     while (w--) {
          __u32 s = *S++;

          D->a = (s & 0xFF000000) >> 24;
          D->r = (s & 0x00FF0000) >> 16;
          D->g = (s & 0x0000FF00) >>  8;
          D->b = (s & 0x000000FF);

          D++;
     }
}

static void Sop_airgb_to_Dacc( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *D = gfxs->Dacc;
     __u32             *S = gfxs->Sop;

     while (w--) {
          __u32 s = *S++;

          D->a = ((s & 0xFF000000) >> 24) ^ 0xff;
          D->r = (s & 0x00FF0000) >> 16;
          D->g = (s & 0x0000FF00) >>  8;
          D->b = (s & 0x000000FF);

          D++;
     }
}

static void Sop_rgb332_to_Dacc( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *D = gfxs->Dacc;
     __u8              *S = gfxs->Sop;

     while (w--) {
          __u8 s = *S++;

          D->a = 0xFF;
          D->r = lookup3to8[s >> 5];
          D->g = lookup3to8[(s & 0x1C) >> 2];
          D->b = lookup2to8[s & 0x03];

          D++;
     }
}

static void Sop_lut8_to_Dacc( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *D = gfxs->Dacc;
     __u8              *S = gfxs->Sop;

     DFBColor *entries = gfxs->Slut->entries;

     while (w--) {
          __u8 s = *S++;

          D->a = entries[s].a;
          D->r = entries[s].r;
          D->g = entries[s].g;
          D->b = entries[s].b;

          D++;
     }
}

static void Sop_alut44_to_Dacc( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *D = gfxs->Dacc;
     __u8              *S = gfxs->Sop;

     DFBColor *entries = gfxs->Slut->entries;

     while (w--) {
          __u8 s = *S++;

          D->a = s & 0xF0;
          s &= 0x0F;
          D->r = entries[s].r;
          D->g = entries[s].g;
          D->b = entries[s].b;

          D++;
     }
}

static GenefxFunc Sop_PFI_to_Dacc[DFB_NUM_PIXELFORMATS] = {
     Sop_argb1555_to_Dacc,
     Sop_rgb16_to_Dacc,
     Sop_rgb24_to_Dacc,
     Sop_rgb32_to_Dacc,
     Sop_argb_to_Dacc,
     Sop_a8_to_Dacc,
     NULL,
     Sop_rgb332_to_Dacc,
     NULL,
     NULL,
     NULL,
     Sop_lut8_to_Dacc,
     Sop_alut44_to_Dacc,
     Sop_airgb_to_Dacc
};

/********************************* Sop_PFI_Kto_Dacc ***************************/

static void Sop_argb1555_Kto_Dacc( GenefxState *gfxs )
{
     int                w    = gfxs->length;
     GenefxAccumulator *D    = gfxs->Dacc;
     __u16             *S    = gfxs->Sop;
     __u32              Skey = gfxs->Skey;

     while (w--) {
          __u16 s = *S++;

          if ((s & 0x7fff) != Skey) {
               D->a = (s & 0x8000) ? 0xff : 0;
               D->r = (s & 0x7C00) >> 7;
               D->g = (s & 0x03E0) >> 2;
               D->b = (s & 0x001F) << 3;
          }
          else
               D->a = 0xF000;

          D++;
     }
}

static void Sop_rgb16_Kto_Dacc( GenefxState *gfxs )
{
     int                w    = gfxs->length;
     GenefxAccumulator *D    = gfxs->Dacc;
     __u16             *S    = gfxs->Sop;
     __u32              Skey = gfxs->Skey;

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

static void Sop_rgb24_Kto_Dacc( GenefxState *gfxs )
{
     int                w    = gfxs->length;
     GenefxAccumulator *D    = gfxs->Dacc;
     __u8              *S    = gfxs->Sop;
     __u32              Skey = gfxs->Skey;

     while (w--) {
          __u8 b = *S++;
          __u8 g = *S++;
          __u8 r = *S++;

          if (Skey != (__u32)(r<<16 | g<<8 | b )) {
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

static void Sop_rgb32_Kto_Dacc( GenefxState *gfxs )
{
     int                w    = gfxs->length;
     GenefxAccumulator *D    = gfxs->Dacc;
     __u32             *S    = gfxs->Sop;
     __u32              Skey = gfxs->Skey;

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

static void Sop_argb_Kto_Dacc( GenefxState *gfxs )
{
     int                w    = gfxs->length;
     GenefxAccumulator *D    = gfxs->Dacc;
     __u32             *S    = gfxs->Sop;
     __u32              Skey = gfxs->Skey;

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

static void Sop_airgb_Kto_Dacc( GenefxState *gfxs )
{
     int                w    = gfxs->length;
     GenefxAccumulator *D    = gfxs->Dacc;
     __u32             *S    = gfxs->Sop;
     __u32              Skey = gfxs->Skey;

     while (w--) {
          __u32 s = *S++;

          if ((s & 0xFFFFFF) != Skey) {
               D->a = (s >> 24) ^ 0xff;
               D->r = (s & 0x00FF0000) >> 16;
               D->g = (s & 0x0000FF00) >>  8;
               D->b = (s & 0x000000FF);
          }
          else
               D->a = 0xF000;

          D++;
     }
}

static void Sop_a8_Kto_Dacc( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *D = gfxs->Dacc;
     __u8              *S = gfxs->Sop;

     /* no color to key */
     while (w--) {
          D->a = *S++;
          D->r = 0xFF;
          D->g = 0xFF;
          D->b = 0xFF;

          D++;
     }
}

static void Sop_rgb332_Kto_Dacc( GenefxState *gfxs )
{
     int                w    = gfxs->length;
     GenefxAccumulator *D    = gfxs->Dacc;
     __u8              *S    = gfxs->Sop;
     __u32              Skey = gfxs->Skey;

     while (w--) {
          __u8 s = *S++;

          if (s != Skey) {
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

static void Sop_lut8_Kto_Dacc( GenefxState *gfxs )
{
     int                w    = gfxs->length;
     GenefxAccumulator *D    = gfxs->Dacc;
     __u8              *S    = gfxs->Sop;
     __u32              Skey = gfxs->Skey;

     DFBColor *entries = gfxs->Slut->entries;

     while (w--) {
          __u8 s = *S++;

          if (s != Skey) {
               D->a = entries[s].a;
               D->r = entries[s].r;
               D->g = entries[s].g;
               D->b = entries[s].b;
          }
          else
               D->a = 0xF000;

          D++;
     }
}

static void Sop_alut44_Kto_Dacc( GenefxState *gfxs )
{
     int                w    = gfxs->length;
     GenefxAccumulator *D    = gfxs->Dacc;
     __u8              *S    = gfxs->Sop;
     __u32              Skey = gfxs->Skey;

     DFBColor *entries = gfxs->Slut->entries;

     while (w--) {
          __u8 s = *S++;

          if ((s & 0x0F) != Skey) {
               D->a = ((s & 0xF0) >> 4) | (s & 0xF0);
               s &= 0x0F;
               D->r = entries[s].r;
               D->g = entries[s].g;
               D->b = entries[s].b;
          }
          else
               D->a = 0xF000;

          D++;
     }
}

static GenefxFunc Sop_PFI_Kto_Dacc[DFB_NUM_PIXELFORMATS] = {
     Sop_argb1555_Kto_Dacc,
     Sop_rgb16_Kto_Dacc,
     Sop_rgb24_Kto_Dacc,
     Sop_rgb32_Kto_Dacc,
     Sop_argb_Kto_Dacc,
     Sop_a8_Kto_Dacc,
     NULL,
     Sop_rgb332_Kto_Dacc,
     NULL,
     NULL,
     NULL,
     Sop_lut8_Kto_Dacc,
     Sop_alut44_Kto_Dacc,
     Sop_airgb_Kto_Dacc
};

/********************************* Sacc_to_Aop_PFI ****************************/

static void Sacc_to_Aop_argb1555( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *S = gfxs->Sacc;
     __u16             *D = gfxs->Aop;

     while (w--) {
          if (!(S->a & 0xF000)) {
               *D = PIXEL_ARGB1555( (S->a & 0xFF00) ? 0xFF : S->a,
                                    (S->r & 0xFF00) ? 0xFF : S->r,
                                    (S->g & 0xFF00) ? 0xFF : S->g,
                                    (S->b & 0xFF00) ? 0xFF : S->b );
          }

          D++;
          S++;
     }
}

static void Sacc_to_Aop_rgb16( GenefxState *gfxs )
{
     int                l;
     int                w = gfxs->length;
     GenefxAccumulator *S = gfxs->Sacc;
     __u16             *D = gfxs->Aop;

     if ((long) D & 2) {
          if (!(S->a & 0xF000)) {
               *D = PIXEL_RGB16( (S->r & 0xFF00) ? 0xFF : S->r,
                                 (S->g & 0xFF00) ? 0xFF : S->g,
                                 (S->b & 0xFF00) ? 0xFF : S->b );
          }

          ++S;
          ++D;
          --w;
     }

     l = w >> 1;
     while (l) {
          __u32 *D2 = (__u32*) D;

          if (!(S[0].a & 0xF000) && !(S[1].a & 0xF000)) {
#ifdef WORDS_BIGENDIAN
               *D2 = PIXEL_RGB16( (S[1].r & 0xFF00) ? 0xFF : S[1].r,
                                  (S[1].g & 0xFF00) ? 0xFF : S[1].g,
                                  (S[1].b & 0xFF00) ? 0xFF : S[1].b ) |
                     PIXEL_RGB16( (S[0].r & 0xFF00) ? 0xFF : S[0].r,
                                  (S[0].g & 0xFF00) ? 0xFF : S[0].g,
                                  (S[0].b & 0xFF00) ? 0xFF : S[0].b ) << 16;
#else
               *D2 = PIXEL_RGB16( (S[0].r & 0xFF00) ? 0xFF : S[0].r,
                                  (S[0].g & 0xFF00) ? 0xFF : S[0].g,
                                  (S[0].b & 0xFF00) ? 0xFF : S[0].b ) |
                     PIXEL_RGB16( (S[1].r & 0xFF00) ? 0xFF : S[1].r,
                                  (S[1].g & 0xFF00) ? 0xFF : S[1].g,
                                  (S[1].b & 0xFF00) ? 0xFF : S[1].b ) << 16;
#endif
          }
          else {
               if (!(S[0].a & 0xF000)) {
                    D[0] = PIXEL_RGB16( (S[0].r & 0xFF00) ? 0xFF : S[0].r,
                                        (S[0].g & 0xFF00) ? 0xFF : S[0].g,
                                        (S[0].b & 0xFF00) ? 0xFF : S[0].b );
               }
               else
                    if (!(S[1].a & 0xF000)) {
                    D[1] = PIXEL_RGB16( (S[1].r & 0xFF00) ? 0xFF : S[1].r,
                                        (S[1].g & 0xFF00) ? 0xFF : S[1].g,
                                        (S[1].b & 0xFF00) ? 0xFF : S[1].b );
               }
          }

          S += 2;
          D += 2;

          --l;
     }

     if (w & 1) {
          if (!(S->a & 0xF000)) {
               *D = PIXEL_RGB16( (S->r & 0xFF00) ? 0xFF : S->r,
                                 (S->g & 0xFF00) ? 0xFF : S->g,
                                 (S->b & 0xFF00) ? 0xFF : S->b );
          }
     }
}

static void Sacc_to_Aop_rgb24( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *S = gfxs->Sacc;
     __u8              *D = gfxs->Aop;

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

static void Sacc_to_Aop_rgb32( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *S = gfxs->Sacc;
     __u32             *D = gfxs->Aop;

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

static void Sacc_to_Aop_argb( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *S = gfxs->Sacc;
     __u32             *D = gfxs->Aop;

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

static void Sacc_to_Aop_airgb( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *S = gfxs->Sacc;
     __u32             *D = gfxs->Aop;

     while (w--) {
          if (!(S->a & 0xF000)) {
               *D = PIXEL_AiRGB( (S->a & 0xFF00) ? 0xFF : S->a,
                                 (S->r & 0xFF00) ? 0xFF : S->r,
                                 (S->g & 0xFF00) ? 0xFF : S->g,
                                 (S->b & 0xFF00) ? 0xFF : S->b );
          }

          D++;
          S++;
     }
}

static void Sacc_to_Aop_a8( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *S = gfxs->Sacc;
     __u8              *D = gfxs->Aop;

     while (w--) {
          if (!(S->a & 0xF000))
               *D = (S->a & 0xFF00) ? 0xFF : S->a;

          D++;
          S++;
     }
}

static void Sacc_to_Aop_rgb332( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *S = gfxs->Sacc;
     __u8              *D = gfxs->Aop;

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

static void Sacc_to_Aop_lut8( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *S = gfxs->Sacc;
     __u8              *D = gfxs->Aop;

     while (w--) {
          if (!(S->a & 0xF000)) {
               *D = dfb_palette_search( gfxs->Alut,
                                        (S->r & 0xFF00) ? 0xFF : S->r,
                                        (S->g & 0xFF00) ? 0xFF : S->g,
                                        (S->b & 0xFF00) ? 0xFF : S->b,
                                        (S->a & 0xFF00) ? 0xFF : S->a );
          }

          D++;
          S++;
     }
}

static void Sacc_to_Aop_alut44( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *S = gfxs->Sacc;
     __u8              *D = gfxs->Aop;

     while (w--) {
          if (!(S->a & 0xF000)) {
               *D = (S->a & 0xFF00) ? 0xF0 : (S->a & 0xF0) +
                    dfb_palette_search( gfxs->Alut,
                                        (S->r & 0xFF00) ? 0xFF : S->r,
                                        (S->g & 0xFF00) ? 0xFF : S->g,
                                        (S->b & 0xFF00) ? 0xFF : S->b,
                                        0x80 );
          }

          D++;
          S++;
     }
}

static GenefxFunc Sacc_to_Aop_PFI[DFB_NUM_PIXELFORMATS] = {
     Sacc_to_Aop_argb1555,
     Sacc_to_Aop_rgb16,
     Sacc_to_Aop_rgb24,
     Sacc_to_Aop_rgb32,
     Sacc_to_Aop_argb,
     Sacc_to_Aop_a8,
     NULL,
     Sacc_to_Aop_rgb332,
     NULL,
     NULL,
     NULL,
     Sacc_to_Aop_lut8,
     Sacc_to_Aop_alut44,
     Sacc_to_Aop_airgb
};

/******************************** Sacc_toK_Aop_PFI ****************************/

static void Sacc_toK_Aop_argb1555( GenefxState *gfxs )
{
     int                w    = gfxs->length;
     GenefxAccumulator *S    = gfxs->Sacc;
     __u16             *D    = gfxs->Aop;
     __u32              Dkey = gfxs->Dkey;

     while (w--) {
          if (!(S->a & 0xF000) && ((*D & 0x7fff) == Dkey)) {
               *D = PIXEL_ARGB1555( (S->a & 0xFF00) ? 0xFF : S->a,
                                    (S->r & 0xFF00) ? 0xFF : S->r,
                                    (S->g & 0xFF00) ? 0xFF : S->g,
                                    (S->b & 0xFF00) ? 0xFF : S->b );
          }

          D++;
          S++;
     }
}

static void Sacc_toK_Aop_rgb16( GenefxState *gfxs )
{
     int                w    = gfxs->length;
     GenefxAccumulator *S    = gfxs->Sacc;
     __u16             *D    = gfxs->Aop;
     __u32              Dkey = gfxs->Dkey;

     while (w--) {
          if (!(S->a & 0xF000) && (*D == Dkey)) {
               *D = PIXEL_RGB16( (S->r & 0xFF00) ? 0xFF : S->r,
                                 (S->g & 0xFF00) ? 0xFF : S->g,
                                 (S->b & 0xFF00) ? 0xFF : S->b );
          }

          D++;
          S++;
     }
}

static void Sacc_toK_Aop_rgb24( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *S = gfxs->Sacc;
     __u8              *D = gfxs->Aop;

     /* FIXME: implement keying */
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

static void Sacc_toK_Aop_rgb32( GenefxState *gfxs )
{
     int                w    = gfxs->length;
     GenefxAccumulator *S    = gfxs->Sacc;
     __u32             *D    = gfxs->Aop;
     __u32              Dkey = gfxs->Dkey;

     while (w--) {
          if (!(S->a & 0xF000) && ((*D & 0xffffff) == Dkey)) {
               *D = PIXEL_RGB32( (S->r & 0xFF00) ? 0xFF : S->r,
                                 (S->g & 0xFF00) ? 0xFF : S->g,
                                 (S->b & 0xFF00) ? 0xFF : S->b );
          }

          D++;
          S++;
     }
}

static void Sacc_toK_Aop_argb( GenefxState *gfxs )
{
     int                w    = gfxs->length;
     GenefxAccumulator *S    = gfxs->Sacc;
     __u32             *D    = gfxs->Aop;
     __u32              Dkey = gfxs->Dkey;

     while (w--) {
          if (!(S->a & 0xF000) && ((*D & 0xffffff) == Dkey)) {
               *D = PIXEL_ARGB( (S->a & 0xFF00) ? 0xFF : S->a,
                                (S->r & 0xFF00) ? 0xFF : S->r,
                                (S->g & 0xFF00) ? 0xFF : S->g,
                                (S->b & 0xFF00) ? 0xFF : S->b );
          }

          D++;
          S++;
     }
}

static void Sacc_toK_Aop_airgb( GenefxState *gfxs )
{
     int                w    = gfxs->length;
     GenefxAccumulator *S    = gfxs->Sacc;
     __u32             *D    = gfxs->Aop;
     __u32              Dkey = gfxs->Dkey;

     while (w--) {
          if (!(S->a & 0xF000) && ((*D & 0xffffff) == Dkey)) {
               *D = PIXEL_AiRGB( (S->a & 0xFF00) ? 0xFF : S->a,
                                 (S->r & 0xFF00) ? 0xFF : S->r,
                                 (S->g & 0xFF00) ? 0xFF : S->g,
                                 (S->b & 0xFF00) ? 0xFF : S->b );
          }

          D++;
          S++;
     }
}

static void Sacc_toK_Aop_a8( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *S = gfxs->Sacc;
     __u8              *D = gfxs->Aop;

     /* FIXME: do all or do none? */
     while (w--) {
          if (!(S->a & 0xF000))
               *D = (S->a & 0xFF00) ? 0xFF : S->a;

          D++;
          S++;
     }
}

static void Sacc_toK_Aop_rgb332( GenefxState *gfxs )
{
     int                w    = gfxs->length;
     GenefxAccumulator *S    = gfxs->Sacc;
     __u8              *D    = gfxs->Aop;
     __u32              Dkey = gfxs->Dkey;

     while (w--) {
          if (!(S->a & 0xF000) && (*D == Dkey)) {
               *D = PIXEL_RGB332( (S->r & 0xFF00) ? 0xFF : S->r,
                                  (S->g & 0xFF00) ? 0xFF : S->g,
                                  (S->b & 0xFF00) ? 0xFF : S->b );
          }

          D++;
          S++;
     }
}

static void Sacc_toK_Aop_lut8( GenefxState *gfxs )
{
     int                w    = gfxs->length;
     GenefxAccumulator *S    = gfxs->Sacc;
     __u8              *D    = gfxs->Aop;
     __u32              Dkey = gfxs->Dkey;

     while (w--) {
          if (!(S->a & 0xF000) && (*D == Dkey)) {
               *D = dfb_palette_search( gfxs->Alut,
                                        (S->r & 0xFF00) ? 0xFF : S->r,
                                        (S->g & 0xFF00) ? 0xFF : S->g,
                                        (S->b & 0xFF00) ? 0xFF : S->b,
                                        (S->a & 0xFF00) ? 0xFF : S->a );
          }

          D++;
          S++;
     }
}

static void Sacc_toK_Aop_alut44( GenefxState *gfxs )
{
     int                w    = gfxs->length;
     GenefxAccumulator *S    = gfxs->Sacc;
     __u8              *D    = gfxs->Aop;
     __u32              Dkey = gfxs->Dkey;

     while (w--) {
          if (!(S->a & 0xF000) && ((*D & 0x0F) == Dkey)) {
               *D = (S->a & 0xFF00) ? 0xF0 : (S->a & 0xF0) +
                    dfb_palette_search( gfxs->Alut,
                                        (S->r & 0xFF00) ? 0xFF : S->r,
                                        (S->g & 0xFF00) ? 0xFF : S->g,
                                        (S->b & 0xFF00) ? 0xFF : S->b,
                                        0x80 );
          }

          D++;
          S++;
     }
}

static GenefxFunc Sacc_toK_Aop_PFI[DFB_NUM_PIXELFORMATS] = {
     Sacc_toK_Aop_argb1555,
     Sacc_toK_Aop_rgb16,
     Sacc_toK_Aop_rgb24,
     Sacc_toK_Aop_rgb32,
     Sacc_toK_Aop_argb,
     Sacc_toK_Aop_a8,
     NULL,
     Sacc_toK_Aop_rgb332,
     NULL,
     NULL,
     NULL,
     Sacc_toK_Aop_lut8,
     Sacc_toK_Aop_alut44,
     Sacc_toK_Aop_airgb
};

/************** Bop_a8_set_alphapixel_Aop_PFI *********************************/

#define DUFF_1(format) \
               case 1:\
                    SET_ALPHA_PIXEL_##format( D[0], S[0] );

#define DUFF_2(format) \
               case 3:\
                    SET_ALPHA_PIXEL_##format( D[2], S[2] );\
               case 2:\
                    SET_ALPHA_PIXEL_##format( D[1], S[1] );\
               DUFF_1(format)

#define DUFF_3(format) \
               case 7:\
                    SET_ALPHA_PIXEL_##format( D[6], S[6] );\
               case 6:\
                    SET_ALPHA_PIXEL_##format( D[5], S[5] );\
               case 5:\
                    SET_ALPHA_PIXEL_##format( D[4], S[4] );\
               case 4:\
                    SET_ALPHA_PIXEL_##format( D[3], S[3] );\
               DUFF_2(format)

#define DUFF_4(format) \
               case 15:\
                    SET_ALPHA_PIXEL_##format( D[14], S[14] );\
               case 14:\
                    SET_ALPHA_PIXEL_##format( D[13], S[13] );\
               case 13:\
                    SET_ALPHA_PIXEL_##format( D[12], S[12] );\
               case 12:\
                    SET_ALPHA_PIXEL_##format( D[11], S[11] );\
               case 11:\
                    SET_ALPHA_PIXEL_##format( D[10], S[10] );\
               case 10:\
                    SET_ALPHA_PIXEL_##format( D[9], S[9] );\
               case 9:\
                    SET_ALPHA_PIXEL_##format( D[8], S[8] );\
               case 8:\
                    SET_ALPHA_PIXEL_##format( D[7], S[7] );\
               DUFF_3(format)

#define SET_ALPHA_PIXEL_DUFFS_DEVICE_N(D, S, w, format, n) \
     while (w) {\
          register int l = w & ((1 << n) - 1);\
          switch (l) {\
               default:\
                    l = (1 << n);\
                    SET_ALPHA_PIXEL_##format( D[(1 << n)-1], S[(1 << n)-1] );\
               DUFF_##n(format)\
          }\
          D += l;\
          S += l;\
          w -= l;\
     }

/* change the last value to adjust the size of the device (1-4) */
#define SET_ALPHA_PIXEL_DUFFS_DEVICE(D, S, w, format) \
          SET_ALPHA_PIXEL_DUFFS_DEVICE_N(D, S, w, format, 3)


static void Bop_a8_set_alphapixel_Aop_argb1555( GenefxState *gfxs )
{
     int    w   = gfxs->length;
     __u8  *S   = gfxs->Bop;
     __u16 *D   = gfxs->Aop;
     __u32  Cop = gfxs->Cop;
     __u32  rb  = Cop & 0x7c1f;
     __u32  g   = Cop & 0x03e0;

#define SET_ALPHA_PIXEL_ARGB1555(d,a) \
     switch (a) {\
          case 0xff: d = Cop;\
          case 0: break;\
          default: {\
               register __u32  s = (a>>3)+1;\
               register __u32 t1 = (d & 0x7c1f);\
               register __u32 t2 = (d & 0x03e0);\
               d = ((a & 0x80) << 8) | \
                   ((((rb-t1)*s+(t1<<5)) & 0x000f83e0) + \
                    ((( g-t2)*s+(t2<<5)) & 0x00007c00)) >> 5;\
          }\
     }

     SET_ALPHA_PIXEL_DUFFS_DEVICE( D, S, w, ARGB1555 );

#undef SET_ALPHA_PIXEL_ARGB1555
}


static void Bop_a8_set_alphapixel_Aop_rgb16( GenefxState *gfxs )
{
     int    w   = gfxs->length;
     __u8  *S   = gfxs->Bop;
     __u16 *D   = gfxs->Aop;
     __u32  Cop = gfxs->Cop;
     __u32  rb  = gfxs->Cop & 0xf81f;
     __u32  g   = gfxs->Cop & 0x07e0;

#define SET_ALPHA_PIXEL_RGB16(d,a)\
     switch (a) {\
          case 0xff: d = Cop;\
          case 0: break;\
          default: {\
               register __u32  s = (a>>2)+1;\
               register __u32 t1 = (d & 0xf81f);\
               register __u32 t2 = (d & 0x07e0);\
               d  = ((((rb-t1)*s+(t1<<6)) & 0x003e07c0) + \
                     ((( g-t2)*s+(t2<<6)) & 0x0001f800)) >> 6;\
          }\
     }

     SET_ALPHA_PIXEL_DUFFS_DEVICE( D, S, w, RGB16 );

#undef SET_ALPHA_PIXEL_RGB16
}

static void Bop_a8_set_alphapixel_Aop_rgb24( GenefxState *gfxs )
{
     int       w     = gfxs->length;
     __u8     *S     = gfxs->Bop;
     __u8     *D     = gfxs->Aop;
     DFBColor  color = gfxs->color;

#define SET_ALPHA_PIXEL_RGB24(d,r,g,b,a)\
     switch (a) {\
         case 0xff:\
               d[0] = b;\
               d[1] = g;\
               d[2] = r;\
          case 0: break;\
          default: {\
               register __u16 s = a+1;\
               d[0] = ((b-d[0]) * s + (d[0] << 8)) >> 8;\
               d[1] = ((g-d[1]) * s + (d[1] << 8)) >> 8;\
               d[2] = ((r-d[2]) * s + (d[2] << 8)) >> 8;\
          }\
     }

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

#undef SET_ALPHA_PIXEL_RGB24
}

static void Bop_a8_set_alphapixel_Aop_rgb32( GenefxState *gfxs )
{
     int    w   = gfxs->length;
     __u8  *S   = gfxs->Bop;
     __u32 *D   = gfxs->Aop;
     __u32  Cop = gfxs->Cop;
     __u32  rb  = Cop & 0xff00ff;
     __u32  g   = Cop & 0x00ff00;

#define SET_ALPHA_PIXEL_RGB32(d,a)\
     switch (a) {\
          case 0xff: d = Cop;\
          case 0: break;\
          default: {\
               register __u32  s = a+1;\
               register __u32 t1 = (d & 0x00ff00ff);\
               register __u32 t2 = (d & 0x0000ff00);\
               d = ((((rb-t1)*s+(t1<<8)) & 0xff00ff00) + \
                    ((( g-t2)*s+(t2<<8)) & 0x00ff0000)) >> 8;\
          }\
     }

     SET_ALPHA_PIXEL_DUFFS_DEVICE( D, S, w, RGB32 );

#undef SET_ALPHA_PIXEL_RGB32
}


/* saturating alpha blend */

static void Bop_a8_set_alphapixel_Aop_argb( GenefxState *gfxs )
{
     int    w   = gfxs->length;
     __u8  *S   = gfxs->Bop;
     __u32 *D   = gfxs->Aop;
     __u32  Cop = gfxs->Cop | 0xff000000;
     __u32  rb  = Cop & 0x00ff00ff;
     __u32  g   = gfxs->color.g;

#define SET_ALPHA_PIXEL_ARGB(d,a)\
     switch (a) {\
          case 0xff: d = Cop;\
          case 0: break;\
          default: {\
               register __u32  s = a+1;\
               register __u32 s1 = 256-s;\
               register __u32 sa = (d >> 24) + a;\
               if (sa & 0xff00) sa = 0xff;\
               d = (sa << 24) + \
                    (((((d & 0x00ff00ff)       * s1) + (rb  * s)) >> 8) & 0x00ff00ff) + \
                    (((((d & 0x0000ff00) >> 8) * s1) + ((g) * s))       & 0x0000ff00);  \
          }\
     }

     SET_ALPHA_PIXEL_DUFFS_DEVICE( D, S, w, ARGB );

#undef SET_ALPHA_PIXEL_ARGB
}

static void Bop_a8_set_alphapixel_Aop_airgb( GenefxState *gfxs )
{
     int    w   = gfxs->length;
     __u8  *S   = gfxs->Bop;
     __u32 *D   = gfxs->Aop;
     __u32  Cop = gfxs->Cop | 0xff000000;
     __u32  rb  = Cop & 0x00ff00ff;
     __u32  g   = gfxs->color.g;

#define SET_ALPHA_PIXEL_ARGB(d,a)\
     switch (a) {\
          case 0xff: d = Cop;\
          case 0: break;\
          default: {\
               register __u32  s = a+1;\
               register __u32 s1 = 256-s;\
               register __s32 sa = (d >> 24) - a;\
               if (sa < 0) sa = 0;\
               d = (sa << 24) + \
                    (((((d & 0x00ff00ff)       * s1) + (rb  * s)) >> 8) & 0x00ff00ff) + \
                    (((((d & 0x0000ff00) >> 8) * s1) + ((g) * s))       & 0x0000ff00);  \
          }\
     }

     SET_ALPHA_PIXEL_DUFFS_DEVICE( D, S, w, ARGB );

#undef SET_ALPHA_PIXEL_ARGB
}

static void Bop_a8_set_alphapixel_Aop_a8( GenefxState *gfxs )
{
     int    w = gfxs->length;
     __u8  *S = gfxs->Bop;
     __u8  *D = gfxs->Aop;

#define SET_ALPHA_PIXEL_A8(d,a)\
     switch (a) {\
          case 0xff: d = 0xff;\
          case 0: break; \
          default: {\
               register __u16 s  = (a)+1;\
               register __u16 s1 = 256-s;\
               d = (d * s1 + s) >> 8;\
          }\
     }

     SET_ALPHA_PIXEL_DUFFS_DEVICE( D, S, w, A8 );

#undef SET_ALPHA_PIXEL_A8
}

static void Bop_a8_set_alphapixel_Aop_rgb332( GenefxState *gfxs )
{
     int    w   = gfxs->length;
     __u8  *S   = gfxs->Bop;
     __u8  *D   = gfxs->Aop;
     __u32  Cop = gfxs->Cop;

/* FIXME: implement correctly! */
#define SET_ALPHA_PIXEL_RGB332(d,a) \
     if (a & 0x80) \
          d = Cop;

     SET_ALPHA_PIXEL_DUFFS_DEVICE( D, S, w, RGB332 );
#undef SET_ALPHA_PIXEL_RGB332
}

static void Bop_a8_set_alphapixel_Aop_lut8( GenefxState *gfxs )
{
     int    w   = gfxs->length;
     __u8  *S   = gfxs->Bop;
     __u8  *D   = gfxs->Aop;
     __u32  Cop = gfxs->Cop;

#if 0
     DFBColor  color   = gfxs->color;
     DFBColor *entries = gfxs->Alut->entries;

# define SET_ALPHA_PIXEL_LUT8(d,alpha) \
     switch (alpha) {\
          case 0xff: d = Cop;\
          case 0: break; \
          default: {\
               register __u16 s = alpha+1;\
               DFBColor      dc = entries[d];\
               __u16         sa = alpha + dc.a;\
               dc.r = ((color.r - dc.r) * s + (dc.r << 8)) >> 8;\
               dc.g = ((color.g - dc.g) * s + (dc.g << 8)) >> 8;\
               dc.b = ((color.b - dc.b) * s + (dc.b << 8)) >> 8;\
               d = dfb_palette_search( gfxs->Alut, dc.r, dc.g, dc.b,\
                                       sa & 0xff00 ? 0xff : sa );\
          }\
     }
#else
# define SET_ALPHA_PIXEL_LUT8(d,a) \
     if (a & 0x80) \
          d = Cop;
#endif

     SET_ALPHA_PIXEL_DUFFS_DEVICE( D, S, w, LUT8 );
#undef SET_ALPHA_PIXEL_LUT8
}

static void Bop_a8_set_alphapixel_Aop_alut44( GenefxState *gfxs )
{
     int    w   = gfxs->length;
     __u8  *S   = gfxs->Bop;
     __u8  *D   = gfxs->Aop;
     __u32  Cop = gfxs->Cop;

     DFBColor  color   = gfxs->color;
     DFBColor *entries = gfxs->Alut->entries;

#define SET_ALPHA_PIXEL_ALUT44(d,alpha) \
     switch (alpha) {\
          case 0xff: d = Cop;\
          case 0: break; \
          default: {\
               register __u16 s = alpha+1;\
               DFBColor      dc = entries[d & 0x0f];\
               __u16         sa = (d & 0xf0) + alpha;\
               dc.r = ((color.r - dc.r) * s + (dc.r << 8)) >> 8;\
               dc.g = ((color.g - dc.g) * s + (dc.g << 8)) >> 8;\
               dc.b = ((color.b - dc.b) * s + (dc.b << 8)) >> 8;\
               if (sa & 0xff00) sa = 0xf0;\
               d = (sa & 0xf0) + \
                    dfb_palette_search( gfxs->Alut, dc.r, dc.g, dc.b, 0x80 );\
          }\
     }

     while (w--) {
          SET_ALPHA_PIXEL_ALUT44( *D, *S );
          D++, S++;
     }

#undef SET_ALPHA_PIXEL_ALUT44
}

static GenefxFunc Bop_a8_set_alphapixel_Aop_PFI[DFB_NUM_PIXELFORMATS] = {
     Bop_a8_set_alphapixel_Aop_argb1555,
     Bop_a8_set_alphapixel_Aop_rgb16,
     Bop_a8_set_alphapixel_Aop_rgb24,
     Bop_a8_set_alphapixel_Aop_rgb32,
     Bop_a8_set_alphapixel_Aop_argb,
     Bop_a8_set_alphapixel_Aop_a8,
     NULL,
     Bop_a8_set_alphapixel_Aop_rgb332,
     NULL,
     NULL,
     NULL,
     Bop_a8_set_alphapixel_Aop_lut8,
     Bop_a8_set_alphapixel_Aop_alut44,
     Bop_a8_set_alphapixel_Aop_airgb
};

/************** Bop_a1_set_alphapixel_Aop_PFI *********************************/

static void Bop_a1_set_alphapixel_Aop_argb1555( GenefxState *gfxs )
{
     int    i;
     int    w   = gfxs->length;
     __u8  *S   = gfxs->Bop;
     __u16 *D   = gfxs->Aop;
     __u16  Cop = gfxs->Cop | 0x8000;

     for (i=0; i<w; i++) {
          if (S[i>>3] & (0x80 >> (i&7)))
               D[i] = Cop;
     }
}


static void Bop_a1_set_alphapixel_Aop_rgb16( GenefxState *gfxs )
{
     int    i;
     int    w   = gfxs->length;
     __u8  *S   = gfxs->Bop;
     __u16 *D   = gfxs->Aop;
     __u16  Cop = gfxs->Cop;

     for (i=0; i<w; i++) {
          if (S[i>>3] & (0x80 >> (i&7)))
               D[i] = Cop;
     }
}

static void Bop_a1_set_alphapixel_Aop_rgb24( GenefxState *gfxs )
{
     int       i;
     int       w     = gfxs->length;
     __u8     *S     = gfxs->Bop;
     __u8     *D     = gfxs->Aop;
     DFBColor  color = gfxs->color;

     for (i=0; i<w; i++) {
          if (S[i>>3] & (0x80 >> (i&7))) {
               D[0] = color.b;
               D[1] = color.g;
               D[2] = color.r;
          }

          D += 3;
     }
}

static void Bop_a1_set_alphapixel_Aop_rgb32( GenefxState *gfxs )
{
     int    i;
     int    w   = gfxs->length;
     __u8  *S   = gfxs->Bop;
     __u32 *D   = gfxs->Aop;
     __u32  Cop = gfxs->Cop;

     for (i=0; i<w; i++) {
          if (S[i>>3] & (0x80 >> (i&7)))
               D[i] = Cop;
     }
}

static void Bop_a1_set_alphapixel_Aop_argb( GenefxState *gfxs )
{
     int    i;
     int    w   = gfxs->length;
     __u8  *S   = gfxs->Bop;
     __u32 *D   = gfxs->Aop;
     __u32  Cop = gfxs->Cop | 0xFF000000;

     for (i=0; i<w; i++) {
          if (S[i>>3] & (0x80 >> (i&7)))
               D[i] = Cop;
     }
}

static void Bop_a1_set_alphapixel_Aop_airgb( GenefxState *gfxs )
{
     int    i;
     int    w   = gfxs->length;
     __u8  *S   = gfxs->Bop;
     __u32 *D   = gfxs->Aop;
     __u32  Cop = gfxs->Cop & 0x00FFFFFF;

     for (i=0; i<w; i++) {
          if (S[i>>3] & (0x80 >> (i&7)))
               D[i] = Cop;
     }
}

static void Bop_a1_set_alphapixel_Aop_a8( GenefxState *gfxs )
{
     int   i;
     int   w = gfxs->length;
     __u8 *S = gfxs->Bop;
     __u8 *D = gfxs->Aop;

     for (i=0; i<w; i++) {
          if (S[i>>3] & (0x80 >> (i&7)))
               D[i] = 0xff;
     }
}

static void Bop_a1_set_alphapixel_Aop_rgb332( GenefxState *gfxs )
{
     int   i;
     int   w   = gfxs->length;
     __u8 *S   = gfxs->Bop;
     __u8 *D   = gfxs->Aop;
     __u8  Cop = gfxs->Cop;

     for (i=0; i<w; i++) {
          if (S[i>>3] & (0x80 >> (i&7)))
               D[i] = Cop;
     }
}

static void Bop_a1_set_alphapixel_Aop_lut8( GenefxState *gfxs )
{
     int   i;
     int   w   = gfxs->length;
     __u8 *S   = gfxs->Bop;
     __u8 *D   = gfxs->Aop;
     __u8  Cop = gfxs->Cop;

     for (i=0; i<w; i++) {
          if (S[i>>3] & (0x80 >> (i&7)))
               D[i] = Cop;
     }
}

static void Bop_a1_set_alphapixel_Aop_alut44( GenefxState *gfxs )
{
     int   i;
     int   w   = gfxs->length;
     __u8 *S   = gfxs->Bop;
     __u8 *D   = gfxs->Aop;
     __u8  Cop = gfxs->Cop;

     for (i=0; i<w; i++) {
          if (S[i>>3] & (0x80 >> (i&7)))
               D[i] = Cop;
     }
}

static GenefxFunc Bop_a1_set_alphapixel_Aop_PFI[DFB_NUM_PIXELFORMATS] = {
     Bop_a1_set_alphapixel_Aop_argb1555,
     Bop_a1_set_alphapixel_Aop_rgb16,
     Bop_a1_set_alphapixel_Aop_rgb24,
     Bop_a1_set_alphapixel_Aop_rgb32,
     Bop_a1_set_alphapixel_Aop_argb,
     Bop_a1_set_alphapixel_Aop_a8,
     NULL,
     Bop_a1_set_alphapixel_Aop_rgb332,
     NULL,
     NULL,
     NULL,
     Bop_a1_set_alphapixel_Aop_lut8,
     Bop_a1_set_alphapixel_Aop_alut44,
     Bop_a1_set_alphapixel_Aop_airgb
};


/********************************* Xacc_blend *********************************/

static void Xacc_blend_zero( GenefxState *gfxs )
{
     int                i;
     int                w = gfxs->length;
     GenefxAccumulator *X = gfxs->Xacc;

     for (i=0; i<w; i++) {
          if (!(X[i].a & 0xF000))
               X[i].a = X[i].r = X[i].g = X[i].b = 0;
     }
}

static void Xacc_blend_one( GenefxState *gfxs )
{
}

static void Xacc_blend_srccolor( GenefxState *gfxs )
{
     ONCE( "Xacc_blend_srccolor() unimplemented" );
}

static void Xacc_blend_invsrccolor( GenefxState *gfxs )
{
     ONCE( "Xacc_blend_invsrccolor() unimplemented" );
}

static void Xacc_blend_srcalpha( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *X = gfxs->Xacc;

     if (gfxs->Sacc) {
          GenefxAccumulator *S = gfxs->Sacc;

          while (w--) {
               if (!(X->a & 0xF000)) {
                    register __u16 Sa = S->a + 1;

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
          register __u16 Sa = gfxs->color.a + 1;

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



static void Xacc_blend_invsrcalpha( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *X = gfxs->Xacc;

     if (gfxs->Sacc) {
          GenefxAccumulator *S = gfxs->Sacc;

          while (w--) {
               if (!(X->a & 0xF000)) {
                    register __u16 Sa = 0x100 - S->a;

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
          register __u16 Sa = 0x100 - gfxs->color.a;

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

static void Xacc_blend_dstalpha( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *X = gfxs->Xacc;
     GenefxAccumulator *D = gfxs->Dacc;

     while (w--) {
          if (!(X->a & 0xF000)) {
               register __u16 Da = D->a + 1;

               X->r = (Da * X->r) >> 8;
               X->g = (Da * X->g) >> 8;
               X->b = (Da * X->b) >> 8;
               X->a = (Da * X->a) >> 8;
          }

          X++;
          D++;
     }
}

static void Xacc_blend_invdstalpha( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *X = gfxs->Xacc;
     GenefxAccumulator *D = gfxs->Dacc;

     while (w--) {
          if (!(X->a & 0xF000)) {
               register __u16 Da = 0x100 - D->a;

               X->r = (Da * X->r) >> 8;
               X->g = (Da * X->g) >> 8;
               X->b = (Da * X->b) >> 8;
               X->a = (Da * X->a) >> 8;
          }

          X++;
          D++;
     }
}

static void Xacc_blend_destcolor( GenefxState *gfxs )
{
     ONCE( "Xacc_blend_destcolor() unimplemented" );
}

static void Xacc_blend_invdestcolor( GenefxState *gfxs )
{
     ONCE( "Xacc_blend_invdestcolor() unimplemented" );
}

static void Xacc_blend_srcalphasat( GenefxState *gfxs )
{
     ONCE( "Xacc_blend_srcalphasat() unimplemented" );
}

static GenefxFunc Xacc_blend[] = {
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

static void Dacc_set_alpha( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *D = gfxs->Dacc;
     int                a = gfxs->color.a;

     while (w--) {
          if (!(D->a & 0xF000)) {
               D->a = a;
          }

          D++;
     }
}

static void Dacc_modulate_alpha( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *D = gfxs->Dacc;
     int                a = gfxs->Cacc.a;

     while (w--) {
          if (!(D->a & 0xF000)) {
               D->a = (a * D->a) >> 8;
          }

          D++;
     }
}

static void Dacc_modulate_rgb( GenefxState *gfxs )
{
     int                w    = gfxs->length;
     GenefxAccumulator *D    = gfxs->Dacc;
     GenefxAccumulator  Cacc = gfxs->Cacc;

     while (w--) {
          if (!(D->a & 0xF000)) {
               D->r = (Cacc.r * D->r) >> 8;
               D->g = (Cacc.g * D->g) >> 8;
               D->b = (Cacc.b * D->b) >> 8;
          }

          D++;
     }
}

static void Dacc_modulate_rgb_set_alpha( GenefxState *gfxs )
{
     int                w    = gfxs->length;
     GenefxAccumulator *D    = gfxs->Dacc;
     GenefxAccumulator  Cacc = gfxs->Cacc;
     int                a    = gfxs->color.a;

     while (w--) {
          if (!(D->a & 0xF000)) {
               D->a = a;
               D->r = (Cacc.r * D->r) >> 8;
               D->g = (Cacc.g * D->g) >> 8;
               D->b = (Cacc.b * D->b) >> 8;
          }

          D++;
     }
}

static void Dacc_modulate_argb( GenefxState *gfxs )
{
     int                w    = gfxs->length;
     GenefxAccumulator *D    = gfxs->Dacc;
     GenefxAccumulator  Cacc = gfxs->Cacc;

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

static GenefxFunc Dacc_modulation[] = {
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

static void Dacc_premultiply( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *D = gfxs->Dacc;

     while (w--) {
          if (!(D->a & 0xF000)) {
               register __u16 Da = D->a + 1;

               D->r = (Da * D->r) >> 8;
               D->g = (Da * D->g) >> 8;
               D->b = (Da * D->b) >> 8;
          }

          D++;
     }
}

static void Dacc_demultiply( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *D = gfxs->Dacc;

     while (w--) {
          if (!(D->a & 0xF000)) {
               register __u16 Da = D->a + 1;

               D->r = (D->r << 8) / Da;
               D->g = (D->g << 8) / Da;
               D->b = (D->b << 8) / Da;
          }

          D++;
     }
}

static void Dacc_xor( GenefxState *gfxs )
{
     int                w     = gfxs->length;
     GenefxAccumulator *D     = gfxs->Dacc;
     DFBColor           color = gfxs->color;

     while (w--) {
          if (!(D->a & 0xF000)) {
               D->a ^= color.a;
               D->r ^= color.r;
               D->g ^= color.g;
               D->b ^= color.b;
          }

          D++;
     }
}

static void Cacc_to_Dacc( GenefxState *gfxs )
{
     int                w    = gfxs->length;
     GenefxAccumulator *D    = gfxs->Dacc;
     GenefxAccumulator  Cacc = gfxs->Cacc;

     while (w--)
          *D++ = Cacc;
}



static void Cacc_add_to_Dacc_C( GenefxState *gfxs )
{
     int                w    = gfxs->length;
     GenefxAccumulator *D    = gfxs->Dacc;
     GenefxAccumulator  Cacc = gfxs->Cacc;

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

static GenefxFunc Cacc_add_to_Dacc = Cacc_add_to_Dacc_C;

static void Sacc_add_to_Dacc_C( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *S = gfxs->Sacc;
     GenefxAccumulator *D = gfxs->Dacc;

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

static GenefxFunc Sacc_add_to_Dacc = Sacc_add_to_Dacc_C;

static void Sop_is_Aop( GenefxState *gfxs ) { gfxs->Sop = gfxs->Aop;}
static void Sop_is_Bop( GenefxState *gfxs ) { gfxs->Sop = gfxs->Bop;}

static void Slut_is_Alut( GenefxState *gfxs ) { gfxs->Slut = gfxs->Alut;}
static void Slut_is_Blut( GenefxState *gfxs ) { gfxs->Slut = gfxs->Blut;}

static void Sacc_is_NULL( GenefxState *gfxs ) { gfxs->Sacc = NULL;}
static void Sacc_is_Aacc( GenefxState *gfxs ) { gfxs->Sacc = gfxs->Aacc;}
static void Sacc_is_Bacc( GenefxState *gfxs ) { gfxs->Sacc = gfxs->Bacc;}

static void Dacc_is_Aacc( GenefxState *gfxs ) { gfxs->Dacc = gfxs->Aacc;}
static void Dacc_is_Bacc( GenefxState *gfxs ) { gfxs->Dacc = gfxs->Bacc;}

static void Xacc_is_Aacc( GenefxState *gfxs ) { gfxs->Xacc = gfxs->Aacc;}
static void Xacc_is_Bacc( GenefxState *gfxs ) { gfxs->Xacc = gfxs->Bacc;}


/******************************************************************************/

void gGetDriverInfo( GraphicsDriverInfo *info )
{
     snprintf( info->name,
               DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH, "Software Driver" );

#ifdef USE_MMX
     if (dfb_mm_accel() & MM_MMX) {
          if (!dfb_config->mmx) {
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
     info->version.minor = 6;
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

#define MODULATION_FLAGS (DSBLIT_BLEND_ALPHACHANNEL |  \
                          DSBLIT_BLEND_COLORALPHA |    \
                          DSBLIT_COLORIZE |            \
                          DSBLIT_DST_PREMULTIPLY |     \
                          DSBLIT_SRC_PREMULTIPLY |     \
                          DSBLIT_DEMULTIPLY)

bool gAquire( CardState *state, DFBAccelerationMask accel )
{
     GenefxState *gfxs;
     GenefxFunc  *funcs;
     int          dst_pfi;
     int          src_pfi     = 0;
     CoreSurface *destination = state->destination;
     CoreSurface *source      = state->source;
     DFBColor     color       = state->color;

     DFBSurfaceLockFlags lock_flags;

     if (!state->gfxs) {
          gfxs = DFBCALLOC( 1, sizeof(GenefxState) );
          if (!gfxs) {
               ERRORMSG( "DirectFB/Genefx: Couldn't allocate state struct!\n" );
               return false;
          }

          state->gfxs = gfxs;
     }

     gfxs  = state->gfxs;
     funcs = gfxs->funcs;

     /* Destination may have been destroyed. */
     if (!destination)
          return false;

     /* Source may have been destroyed. */
     if (DFB_BLITTING_FUNCTION( accel ) && !source)
          return false;


     gfxs->dst_caps   = destination->caps;
     gfxs->dst_height = destination->height;
     gfxs->dst_format = destination->format;
     gfxs->dst_bpp    = DFB_BYTES_PER_PIXEL( gfxs->dst_format );
     dst_pfi          = DFB_PIXELFORMAT_INDEX( gfxs->dst_format );

     if (DFB_BLITTING_FUNCTION( accel )) {
          gfxs->src_caps   = source->caps;
          gfxs->src_height = source->height;
          gfxs->src_format = source->format;
          gfxs->src_bpp    = DFB_BYTES_PER_PIXEL( gfxs->src_format );
          src_pfi          = DFB_PIXELFORMAT_INDEX( gfxs->src_format );

          lock_flags = state->blittingflags & ( DSBLIT_BLEND_ALPHACHANNEL |
                                                DSBLIT_BLEND_COLORALPHA   |
                                                DSBLIT_DST_COLORKEY ) ?
                       DSLF_READ | DSLF_WRITE : DSLF_WRITE;
     }
     else
          lock_flags = state->drawingflags & ( DSDRAW_BLEND |
                                               DSDRAW_DST_COLORKEY ) ?
                       DSLF_READ | DSLF_WRITE : DSLF_WRITE;

     gfxs->color = color;


     switch (gfxs->dst_format) {
          case DSPF_ARGB1555:
               gfxs->Cop = PIXEL_ARGB1555( color.a, color.r, color.g, color.b );
               break;
          case DSPF_RGB16:
               gfxs->Cop = PIXEL_RGB16( color.r, color.g, color.b );
               break;
          case DSPF_RGB24:
               gfxs->Cop = PIXEL_RGB32( color.r, color.g, color.b );
               break;
          case DSPF_RGB32:
               gfxs->Cop = PIXEL_RGB32( color.r, color.g, color.b );
               break;
          case DSPF_ARGB:
               gfxs->Cop = PIXEL_ARGB( color.a, color.r, color.g, color.b );
               break;
          case DSPF_AiRGB:
               gfxs->Cop = PIXEL_AiRGB( color.a, color.r, color.g, color.b );
               break;
          case DSPF_A1:
               gfxs->Cop = color.a >> 7;
               break;
          case DSPF_A8:
               gfxs->Cop = color.a;
               break;
          case DSPF_YUY2:
               gfxs->Cop   =  Y_FROM_RGB( color.r, color.g, color.b );
               gfxs->CbCop = CB_FROM_RGB( color.r, color.g, color.b );
               gfxs->CrCop = CR_FROM_RGB( color.r, color.g, color.b );
               gfxs->Cop   = PIXEL_YUY2( gfxs->Cop, gfxs->CbCop, gfxs->CrCop );
               break;
          case DSPF_RGB332:
               gfxs->Cop = PIXEL_RGB332( color.r, color.g, color.b );
               break;
          case DSPF_UYVY:
               gfxs->Cop   =  Y_FROM_RGB( color.r, color.g, color.b );
               gfxs->CbCop = CB_FROM_RGB( color.r, color.g, color.b );
               gfxs->CrCop = CR_FROM_RGB( color.r, color.g, color.b );
               gfxs->Cop   = PIXEL_UYVY( gfxs->Cop, gfxs->CbCop, gfxs->CrCop );
               break;
          case DSPF_I420:
               gfxs->Cop   =  Y_FROM_RGB( color.r, color.g, color.b );
               gfxs->CbCop = CB_FROM_RGB( color.r, color.g, color.b );
               gfxs->CrCop = CR_FROM_RGB( color.r, color.g, color.b );
               break;
          case DSPF_YV12:
               gfxs->Cop   =  Y_FROM_RGB( color.r, color.g, color.b );
               gfxs->CbCop = CR_FROM_RGB( color.r, color.g, color.b );
               gfxs->CrCop = CB_FROM_RGB( color.r, color.g, color.b );
               break;
          case DSPF_LUT8:
               gfxs->Cop  = state->color_index;
               gfxs->Alut = destination->palette;
               break;
          case DSPF_ALUT44:
               gfxs->Cop  = (color.a & 0xF0) + state->color_index;
               gfxs->Alut = destination->palette;
               break;
          default:
               ONCE("unsupported destination format");
               return false;
     }

     if (DFB_BLITTING_FUNCTION( accel )) {
          switch (gfxs->src_format) {
               case DSPF_ARGB1555:
               case DSPF_RGB16:
               case DSPF_RGB24:
               case DSPF_RGB32:
               case DSPF_ARGB:
               case DSPF_AiRGB:
               case DSPF_A1:
               case DSPF_A8:
               case DSPF_RGB332:
                    break;
               case DSPF_YUY2:
               case DSPF_UYVY:
               case DSPF_I420:
               case DSPF_YV12:
                    if (accel != DFXL_BLIT ||
                        gfxs->src_format != gfxs->dst_format ||
                        state->blittingflags != DSBLIT_NOFX) {
                         ONCE("only copying blits supported for YUV in software");
                         return false;
                    }
                    break;
               case DSPF_LUT8:
               case DSPF_ALUT44:
                    gfxs->Blut = source->palette;
                    break;
               default:
                    ONCE("unsupported source format");
                    return false;
          }
     }


     dfb_surfacemanager_lock( dfb_gfxcard_surface_manager() );

     if (DFB_BLITTING_FUNCTION( accel )) {
          if (dfb_surface_software_lock( source, DSLF_READ, &gfxs->src_org,
                                         &gfxs->src_pitch, 1 )) {
               dfb_surfacemanager_unlock( dfb_gfxcard_surface_manager() );
               return false;
          }

          gfxs->src_field_offset = gfxs->src_height/2 * gfxs->src_pitch;

          state->source_locked = 1;
     }
     else
          state->source_locked = 0;

     if (dfb_surface_software_lock( state->destination, lock_flags,
                                    &gfxs->dst_org, &gfxs->dst_pitch, 0 )) {

          if (state->source_locked)
               dfb_surface_unlock( source, 1 );

          dfb_surfacemanager_unlock( dfb_gfxcard_surface_manager() );
          return false;
     }

     gfxs->dst_field_offset = gfxs->dst_height/2 * gfxs->dst_pitch;

     dfb_surfacemanager_unlock( dfb_gfxcard_surface_manager() );


     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_DRAWLINE:
          case DFXL_FILLTRIANGLE:
               if (state->drawingflags & (DSDRAW_BLEND | DSDRAW_XOR)) {
                    GenefxAccumulator Cacc;

                    /* not yet completed optimizing checks */
                    if (state->src_blend == DSBF_ZERO) {
                         if (state->dst_blend == DSBF_ZERO) {
                              gfxs->Cop = 0;
                              if (state->drawingflags & DSDRAW_DST_COLORKEY) {
                                   gfxs->Dkey = state->dst_colorkey;
                                   *funcs++ = Cop_toK_Aop_PFI[dst_pfi];
                              }
                              else
                                   *funcs++ = Cop_to_Aop_PFI[dst_pfi];
                              break;
                         }
                         else if (state->dst_blend == DSBF_ONE) {
                              break;
                         }
                    }

                    /* load from destination */
                    *funcs++ = Sop_is_Aop;
                    if (DFB_PIXELFORMAT_IS_INDEXED(gfxs->dst_format))
                         *funcs++ = Slut_is_Alut;
                    *funcs++ = Dacc_is_Aacc;
                    *funcs++ = Sop_PFI_to_Dacc[dst_pfi];

                    /* premultiply destination */
                    if (state->drawingflags & DSDRAW_DST_PREMULTIPLY)
                         *funcs++ = Dacc_premultiply;

                    /* xor destination */
                    if (state->drawingflags & DSDRAW_XOR)
                         *funcs++ = Dacc_xor;

                    /* load source (color) */
                    Cacc.a = color.a;
                    Cacc.r = color.r;
                    Cacc.g = color.g;
                    Cacc.b = color.b;

                    /* premultiply source (color) */
                    if (state->drawingflags & DSDRAW_SRC_PREMULTIPLY) {
                         __u16 ca = color.a + 1;

                         Cacc.r = (Cacc.r * ca) >> 8;
                         Cacc.g = (Cacc.g * ca) >> 8;
                         Cacc.b = (Cacc.b * ca) >> 8;
                    }

                    if (state->drawingflags & DSDRAW_BLEND) {
                         /* source blending */
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
                    }

                    /* demultiply result */
                    if (state->blittingflags & DSDRAW_DEMULTIPLY)
                         *funcs++ = Dacc_demultiply;

                    /* write to destination */
                    *funcs++ = Sacc_is_Aacc;
                    if (state->drawingflags & DSDRAW_DST_COLORKEY) {
                         gfxs->Dkey = state->dst_colorkey;
                         *funcs++ = Sacc_toK_Aop_PFI[dst_pfi];
                    }
                    else
                         *funcs++ = Sacc_to_Aop_PFI[dst_pfi];

                    /* store computed Cacc */
                    gfxs->Cacc = Cacc;
               }
               else {
                    if (state->drawingflags & DSDRAW_DST_COLORKEY) {
                         gfxs->Dkey = state->dst_colorkey;
                         *funcs++ = Cop_toK_Aop_PFI[dst_pfi];
                    }
                    else
                         *funcs++ = Cop_to_Aop_PFI[dst_pfi];
               }
               break;
          case DFXL_BLIT:
               if (state->blittingflags == (DSBLIT_COLORIZE |
                                            DSBLIT_BLEND_ALPHACHANNEL) &&
                   state->src_blend == DSBF_SRCALPHA &&
                   state->dst_blend == DSBF_INVSRCALPHA)
               {
                    if (gfxs->src_format == DSPF_A8 &&
                        Bop_a8_set_alphapixel_Aop_PFI[dst_pfi])
                    {
                         *funcs++ = Bop_a8_set_alphapixel_Aop_PFI[dst_pfi];
                         break;
                    }
                    if (gfxs->src_format == DSPF_A1 &&
                        Bop_a1_set_alphapixel_Aop_PFI[dst_pfi])
                    {
                         *funcs++ = Bop_a1_set_alphapixel_Aop_PFI[dst_pfi];
                         break;
                    }
               }
               /* fallthru */
          case DFXL_STRETCHBLIT: {
                    int modulation = state->blittingflags & MODULATION_FLAGS;

                    if (modulation) {
                         bool read_destination = false;
                         bool source_needs_destination = false;

                         /* check if destination has to be read */
                         if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL |
                                                     DSBLIT_BLEND_COLORALPHA)) {
                              switch (state->src_blend) {
                                   case DSBF_DESTALPHA:
                                   case DSBF_DESTCOLOR:
                                   case DSBF_INVDESTALPHA:
                                   case DSBF_INVDESTCOLOR:
                                        source_needs_destination = true;
                                   default:
                                        ;
                              }

                              read_destination = source_needs_destination ||
                                                 (state->dst_blend != DSBF_ZERO);
                         }

                         /* read the destination if needed */
                         if (read_destination) {
                              *funcs++ = Sop_is_Aop;
                              if (DFB_PIXELFORMAT_IS_INDEXED(gfxs->dst_format))
                                   *funcs++ = Slut_is_Alut;
                              *funcs++ = Dacc_is_Aacc;
                              *funcs++ = Sop_PFI_to_Dacc[dst_pfi];

                              if (state->blittingflags & DSBLIT_DST_PREMULTIPLY)
                                   *funcs++ = Dacc_premultiply;
                         }

                         /* read the source */
                         *funcs++ = Sop_is_Bop;
                         if (DFB_PIXELFORMAT_IS_INDEXED(gfxs->src_format))
                              *funcs++ = Slut_is_Blut;
                         *funcs++ = Dacc_is_Bacc;
                         if (state->blittingflags & DSBLIT_SRC_COLORKEY) {
                              gfxs->Skey = state->src_colorkey;
                              if (accel == DFXL_BLIT)
                                   *funcs++ = Sop_PFI_Kto_Dacc[src_pfi];
                              else
                                   *funcs++ = Sop_PFI_SKto_Dacc[src_pfi];
                         }
                         else {
                              if (accel == DFXL_BLIT)
                                   *funcs++ = Sop_PFI_to_Dacc[src_pfi];
                              else
                                   *funcs++ = Sop_PFI_Sto_Dacc[src_pfi];
                         }

                         /* modulate the source if requested */
                         if (Dacc_modulation[modulation & 0x7]) {
                              /* modulation source */
                              gfxs->Cacc.a = color.a + 1;
                              gfxs->Cacc.r = color.r + 1;
                              gfxs->Cacc.g = color.g + 1;
                              gfxs->Cacc.b = color.b + 1;

                              *funcs++ = Dacc_modulation[modulation & 0x7];
                         }

                         if (state->blittingflags & DSBLIT_SRC_PREMULTIPLY)
                              *funcs++ = Dacc_premultiply;

                         /* do blend functions and combine both accumulators */
                         if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL |
                                                     DSBLIT_BLEND_COLORALPHA)) {
                              /* Xacc will be blended and written to while
                                 Sacc and Dacc point to the SRC and DST
                                 as referenced by the blending functions */
                              *funcs++ = Sacc_is_Bacc;
                              *funcs++ = Dacc_is_Aacc;

                              if (source_needs_destination &&
                                  state->dst_blend != DSBF_ONE) {
                                   /* blend the source */
                                   *funcs++ = Xacc_is_Bacc;
                                   *funcs++ = Xacc_blend[state->src_blend - 1];

                                   /* blend the destination */
                                   *funcs++ = Xacc_is_Aacc;
                                   *funcs++ = Xacc_blend[state->dst_blend - 1];
                              }
                              else {
                                   /* blend the destination if needed */
                                   if (read_destination) {
                                        *funcs++ = Xacc_is_Aacc;
                                        *funcs++ = Xacc_blend[state->dst_blend - 1];
                                   }

                                   /* blend the source */
                                   *funcs++ = Xacc_is_Bacc;
                                   *funcs++ = Xacc_blend[state->src_blend - 1];
                              }

                              /* add the destination to the source */
                              if (read_destination) {
                                   *funcs++ = Sacc_is_Aacc;
                                   *funcs++ = Dacc_is_Bacc;
                                   *funcs++ = Sacc_add_to_Dacc;
                              }
                         }

                         if (state->blittingflags & DSBLIT_DEMULTIPLY) {
                              *funcs++ = Dacc_is_Bacc;
                              *funcs++ = Dacc_demultiply;
                         }

                         /* write source to destination */
                         *funcs++ = Sacc_is_Bacc;
                         *funcs++ = Sacc_to_Aop_PFI[dst_pfi];
                    }
                    else if (gfxs->src_format == gfxs->dst_format/* &&
                             (!DFB_PIXELFORMAT_IS_INDEXED(src_format) ||
                              Alut == Blut)*/) {
                         if (accel == DFXL_BLIT) {
                              if (state->blittingflags & DSBLIT_SRC_COLORKEY) {
                                   gfxs->Skey = state->src_colorkey;
                                   *funcs++ = Bop_PFI_Kto_Aop_PFI[dst_pfi];
                              }
                              else
                                   *funcs++ = Bop_PFI_to_Aop_PFI[dst_pfi];
                         }
                         else {
                              if (state->blittingflags & DSBLIT_SRC_COLORKEY) {
                                   gfxs->Skey = state->src_colorkey;
                                   *funcs++ = Bop_PFI_SKto_Aop_PFI[dst_pfi];
                              }
                              else
                                   *funcs++ = Bop_PFI_Sto_Aop_PFI[dst_pfi];
                         }
                    }
                    else {
                         /* slow */
                         gfxs->Sacc = gfxs->Dacc = gfxs->Aacc;

                         *funcs++ = Sop_is_Bop;
                         if (DFB_PIXELFORMAT_IS_INDEXED(gfxs->src_format))
                              *funcs++ = Slut_is_Blut;

                         if (accel == DFXL_BLIT) {
                              if (state->blittingflags & DSBLIT_SRC_COLORKEY ) {
                                   gfxs->Skey = state->src_colorkey;
                                   *funcs++ = Sop_PFI_Kto_Dacc[src_pfi];
                              }
                              else
                                   *funcs++ = Sop_PFI_to_Dacc[src_pfi];
                         }
                         else { /* DFXL_STRETCHBLIT */

                              if (state->blittingflags & DSBLIT_SRC_COLORKEY ) {
                                   gfxs->Skey = state->src_colorkey;
                                   *funcs++ = Sop_PFI_SKto_Dacc[src_pfi];
                              }
                              else
                                   *funcs++ = Sop_PFI_Sto_Dacc[src_pfi];

                         }

                         *funcs++ = Sacc_to_Aop_PFI[dst_pfi];
                    }
                    break;
               }
          default:
               ONCE("unimplemented drawing/blitting function");
               gRelease( state );
               return false;
     }

     *funcs = NULL;

     return true;
}

void gRelease( CardState *state )
{
     dfb_surface_unlock( state->destination, 0 );

     if (state->source_locked)
          dfb_surface_unlock( state->source, 1 );
}

#define CHECK_PIPELINE()           \
     {                             \
          if (!gfxs->funcs[0])     \
               return;             \
     }

#define RUN_PIPELINE()                     \
     {                                     \
          int         i;                   \
          GenefxFunc *funcs = gfxs->funcs; \
                                           \
          for (i=0; funcs[i]; ++i)         \
               funcs[i]( gfxs );           \
     }


static inline void Aop_xy( GenefxState *gfxs,
                           void *org, int x, int y, int pitch )
{
     gfxs->Aop = org;

     if (gfxs->dst_caps & DSCAPS_SEPARATED) {
          gfxs->Aop_field = y & 1;
          if (gfxs->Aop_field)
               gfxs->Aop += gfxs->dst_field_offset;

          y /= 2;
     }

     DFB_ASSUME( !(x & DFB_PIXELFORMAT_ALIGNMENT(gfxs->dst_format)) );

     gfxs->Aop += y * pitch  +  DFB_BYTES_PER_LINE( gfxs->dst_format, x );
}

static inline void Aop_next( GenefxState *gfxs, int pitch )
{
     if (gfxs->dst_caps & DSCAPS_SEPARATED) {
          gfxs->Aop_field = !gfxs->Aop_field;

          if (gfxs->Aop_field)
               gfxs->Aop += gfxs->dst_field_offset;
          else
               gfxs->Aop += pitch - gfxs->dst_field_offset;
     }
     else
          gfxs->Aop += pitch;
}

static inline void Aop_prev( GenefxState *gfxs, int pitch )
{
     if (gfxs->dst_caps & DSCAPS_SEPARATED) {
          gfxs->Aop_field = !gfxs->Aop_field;

          if (gfxs->Aop_field)
               gfxs->Aop += gfxs->dst_field_offset - pitch;
          else
               gfxs->Aop -= gfxs->dst_field_offset;
     }
     else
          gfxs->Aop -= pitch;
}


static inline void Bop_xy( GenefxState *gfxs,
                           void *org, int x, int y, int pitch )
{
     gfxs->Bop = org;

     if (gfxs->src_caps & DSCAPS_SEPARATED) {
          gfxs->Bop_field = y & 1;
          if (gfxs->Bop_field)
               gfxs->Bop += gfxs->src_field_offset;

          y /= 2;
     }

     DFB_ASSUME( !(x & DFB_PIXELFORMAT_ALIGNMENT(gfxs->src_format)) );

     gfxs->Bop += y * pitch  +  DFB_BYTES_PER_LINE( gfxs->src_format, x );
}

static inline void Bop_next( GenefxState *gfxs, int pitch )
{
     if (gfxs->src_caps & DSCAPS_SEPARATED) {
          gfxs->Bop_field = !gfxs->Bop_field;

          if (gfxs->Bop_field)
               gfxs->Bop += gfxs->src_field_offset;
          else
               gfxs->Bop += pitch - gfxs->src_field_offset;
     }
     else
          gfxs->Bop += pitch;
}

static inline void Bop_prev( GenefxState *gfxs, int pitch )
{
     if (gfxs->src_caps & DSCAPS_SEPARATED) {
          gfxs->Bop_field = !gfxs->Bop_field;

          if (gfxs->Bop_field)
               gfxs->Bop += gfxs->src_field_offset - pitch;
          else
               gfxs->Bop -= gfxs->src_field_offset;
     }
     else
          gfxs->Bop -= pitch;
}


void gFillRectangle( CardState *state, DFBRectangle *rect )
{
     int          h;
     GenefxState *gfxs = state->gfxs;

     DFB_ASSERT( gfxs != NULL );

     CHECK_PIPELINE();

     gfxs->length = rect->w;

     if (gfxs->dst_format == DSPF_YUY2 || gfxs->dst_format == DSPF_UYVY)
          gfxs->length /= 2;

     Aop_xy( gfxs, gfxs->dst_org, rect->x, rect->y, gfxs->dst_pitch );

     h = rect->h;
     while (h--) {
          RUN_PIPELINE();

          Aop_next( gfxs, gfxs->dst_pitch );
     }

     if (gfxs->dst_format == DSPF_I420 || gfxs->dst_format == DSPF_YV12) {
          int dst_field_offset_save = gfxs->dst_field_offset;

          gfxs->dst_field_offset /= 4;

          rect->x /= 2;
          rect->y /= 2;
          rect->w /= 2;
          rect->h /= 2;

          gfxs->length = rect->w;

          gfxs->Cop = gfxs->CbCop;
          Aop_xy( gfxs, gfxs->dst_org + gfxs->dst_height * gfxs->dst_pitch,
                  rect->x, rect->y, gfxs->dst_pitch/2 );
          h = rect->h;
          while (h--) {
               RUN_PIPELINE();

               Aop_next( gfxs, gfxs->dst_pitch/2 );
          }

          gfxs->Cop = gfxs->CrCop;
          Aop_xy( gfxs, gfxs->dst_org + gfxs->dst_height * gfxs->dst_pitch +
                  gfxs->dst_height * gfxs->dst_pitch/4,
                  rect->x, rect->y, gfxs->dst_pitch/2 );
          h = rect->h;
          while (h--) {
               RUN_PIPELINE();

               Aop_next( gfxs, gfxs->dst_pitch/2 );
          }

          gfxs->dst_field_offset = dst_field_offset_save;
     }
}

void gDrawLine( CardState *state, DFBRegion *line )
{
     GenefxState *gfxs = state->gfxs;

     int i,dx,dy,sdy,dxabs,dyabs,x,y,px,py;

     DFB_ASSERT( gfxs != NULL );

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
               dxabs + 1, dyabs + 1};

          gFillRectangle( state, &rect );
          return;
     }

     sdy = SIGN(dy) * SIGN(dx);
     x = dyabs >> 1;
     y = dxabs >> 1;

     if (dx > 0) {
          px  = line->x1;
          py  = line->y1;
     }
     else {
          px  = line->x2;
          py  = line->y2;
     }

     if (dxabs >= dyabs) { /* the line is more horizontal than vertical */

          for (i=0, gfxs->length=1; i<dxabs; i++, gfxs->length++) {
               y += dyabs;
               if (y >= dxabs) {
                    Aop_xy( gfxs, gfxs->dst_org, px, py, gfxs->dst_pitch );
                    RUN_PIPELINE();
                    px += gfxs->length;
                    gfxs->length = 0;
                    y -= dxabs;
                    py += sdy;
               }
          }
          Aop_xy( gfxs, gfxs->dst_org, px, py, gfxs->dst_pitch );
          RUN_PIPELINE();
     }
     else { /* the line is more vertical than horizontal */

          gfxs->length = 1;
          Aop_xy( gfxs, gfxs->dst_org, px, py, gfxs->dst_pitch );
          RUN_PIPELINE();

          for (i=0; i<dyabs; i++) {
               x += dxabs;
               if (x >= dyabs) {
                    x -= dyabs;
                    px++;
               }
               py += sdy;

               Aop_xy( gfxs, gfxs->dst_org, px, py, gfxs->dst_pitch );
               RUN_PIPELINE();
          }
     }
}

static void gDoBlit( GenefxState *gfxs,
                     int sx,     int sy,
                     int width,  int height,
                     int dx,     int dy,
                     int spitch, int dpitch,
                     void *sorg, void *dorg )
{
     if (sorg == dorg && dy > sy) {
          /* we must blit from bottom to top */
          gfxs->length = width;

          Aop_xy( gfxs, dorg, dx, dy + height - 1, dpitch );
          Bop_xy( gfxs, sorg, sx, sy + height - 1, spitch );

          while (height--) {
               RUN_PIPELINE();

               Aop_prev( gfxs, dpitch );
               Bop_prev( gfxs, spitch );
          }
     }
     else {
          /* we must blit from top to bottom */
          gfxs->length = width;

          Aop_xy( gfxs, dorg, dx, dy, dpitch );
          Bop_xy( gfxs, sorg, sx, sy, spitch );

          while (height--) {
               RUN_PIPELINE();

               Aop_next( gfxs, dpitch );
               Bop_next( gfxs, spitch );
          }
     }
}

void gBlit( CardState *state, DFBRectangle *rect, int dx, int dy )
{
     GenefxState *gfxs = state->gfxs;

     DFB_ASSERT( gfxs != NULL );

     CHECK_PIPELINE();

     if (state->blittingflags & DSBLIT_DEINTERLACE) {
          int i;
          int height = rect->h;

          gfxs->length = rect->w;

          Aop_xy( gfxs, gfxs->dst_org, dx, dy, gfxs->dst_pitch );
          Bop_xy( gfxs, gfxs->src_org, rect->x, rect->y, gfxs->src_pitch );

          if (state->source->field) {
               Aop_next( gfxs, gfxs->dst_pitch );
               Bop_next( gfxs, gfxs->src_pitch );
               height--;
          }

          height >>= 1;

          for (i=0; i<height; i++) {
               RUN_PIPELINE();

               Aop_next( gfxs, gfxs->dst_pitch );

               RUN_PIPELINE();

               Aop_next( gfxs, gfxs->dst_pitch );

               Bop_next( gfxs, gfxs->src_pitch );
               Bop_next( gfxs, gfxs->src_pitch );
          }

          return;
     }

     if (gfxs->src_org == gfxs->dst_org && dx > rect->x)
          /* we must blit from right to left */
          gfxs->Ostep = -1;
     else
          /* we must blit from left to right*/
          gfxs->Ostep = 1;

     gDoBlit( gfxs, rect->x, rect->y, rect->w, rect->h, dx, dy,
              gfxs->src_pitch, gfxs->dst_pitch, gfxs->src_org, gfxs->dst_org );

     /* do other planes */
     if (gfxs->src_format == DSPF_I420 || gfxs->src_format == DSPF_YV12) {
          void *sorg = gfxs->src_org + gfxs->src_height * gfxs->src_pitch;
          void *dorg = gfxs->dst_org + gfxs->dst_height * gfxs->dst_pitch;
          int dst_field_offset_save = gfxs->dst_field_offset;
          int src_field_offset_save = gfxs->src_field_offset;

          gfxs->dst_field_offset /= 4;
          gfxs->src_field_offset /= 4;

          gDoBlit( gfxs, rect->x/2, rect->y/2, rect->w/2, rect->h/2, dx/2, dy/2,
                   gfxs->src_pitch/2, gfxs->dst_pitch/2, sorg, dorg );

          sorg += gfxs->src_height * gfxs->src_pitch / 4;
          dorg += gfxs->dst_height * gfxs->dst_pitch / 4;

          gDoBlit( gfxs, rect->x/2, rect->y/2, rect->w/2, rect->h/2, dx/2, dy/2,
                   gfxs->src_pitch/2, gfxs->dst_pitch/2, sorg, dorg );

          gfxs->dst_field_offset = dst_field_offset_save;
          gfxs->src_field_offset = src_field_offset_save;
     }
}

void gStretchBlit( CardState *state, DFBRectangle *srect, DFBRectangle *drect )
{
     GenefxState *gfxs = state->gfxs;

     int f;
     int i = 0;

     DFB_ASSERT( gfxs != NULL );

     CHECK_PIPELINE();

     gfxs->length = drect->w;
     gfxs->SperD  = (srect->w << 16) / drect->w;

     f = (srect->h << 16) / drect->h;

     Aop_xy( gfxs, gfxs->dst_org, drect->x, drect->y, gfxs->dst_pitch );
     Bop_xy( gfxs, gfxs->src_org, srect->x, srect->y, gfxs->src_pitch );

     while (drect->h--) {
          RUN_PIPELINE();

          Aop_next( gfxs, gfxs->dst_pitch );

          i += f;

          while (i > 0xFFFF) {
               i -= 0x10000;
               Bop_next( gfxs, gfxs->src_pitch );
          }
     }
}


#ifdef USE_MMX

#include "generic_mmx.h"

/*
 * patches function pointers to MMX functions
 */
static void gInit_MMX()
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

