/*
   (c) Copyright 2001-2011  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

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

/*
 * Example:
 * #define RGB_MASK 0x00ffff
 * #define Cop_OP_Aop_PFI( op ) Cop_##op##_Aop_24_16
 * #define Bop_PFI_OP_Aop_PFI( op ) Bop_24_16_##op##_Aop
 * #include "template_colorkey_24.h"
 */

#ifdef WORD_BIGENDIAN
#define READ_PIXEL( s ) \
     (0 \
      | ((s)[0] << 16) \
      | ((s)[1] <<  8) \
      | ((s)[2] <<  0) \
     )
#define WRITE_PIXEL( d, pix ) \
     do { \
          (d)[0] = ((pix) >> 16) & 0xff; \
          (d)[1] = ((pix) >>  8) & 0xff; \
          (d)[2] = ((pix) >>  0) & 0xff; \
     } while (0)
#else
#define READ_PIXEL( s ) \
     (0 \
      | ((s)[2] << 16) \
      | ((s)[1] <<  8) \
      | ((s)[0] <<  0) \
     )
#define WRITE_PIXEL( d, pix ) \
     do { \
          (d)[0] = ((pix) >>  0) & 0xff; \
          (d)[1] = ((pix) >>  8) & 0xff; \
          (d)[2] = ((pix) >> 16) & 0xff; \
     } while (0)
#endif

/********************************* Cop_toK_Aop_PFI ****************************/

static void Cop_OP_Aop_PFI(toK)( GenefxState *gfxs )
{
     int  w    = gfxs->length + 1;
     u8  *D    = gfxs->Aop[0];
     u32  Dkey = gfxs->Dkey;
     u32  Cop  = gfxs->Cop;

     while (--w) {
          u32 pix = READ_PIXEL( D );
          if ((pix & RGB_MASK) == Dkey)
               WRITE_PIXEL( D, Cop );

          D += 3;
     }
}
#undef WRITE_PIXEL

/********************************* Bop_PFI_Kto_Aop_PFI ************************/

static void Bop_PFI_OP_Aop_PFI(Kto)( GenefxState *gfxs )
{
     int  w      = gfxs->length + 1;
     const u8 *S = gfxs->Bop[0];
     u8  *D      = gfxs->Aop[0];
     u32  Skey   = gfxs->Skey;
     int  Ostep  = gfxs->Ostep * 3;

     if (Ostep < 0) {
          S += (gfxs->length - 1) * 3;
          D += (gfxs->length - 1) * 3;
     }

     while (--w) {
          u32 pix = READ_PIXEL( S );
          if ((pix & RGB_MASK) != Skey) {
               D[0] = S[0];
               D[1] = S[1];
               D[2] = S[2];
          }

          S += Ostep;
          D += Ostep;
     }
}

/********************************* Bop_PFI_toK_Aop_PFI ************************/

static void Bop_PFI_OP_Aop_PFI(toK)( GenefxState *gfxs )
{
     int  w      = gfxs->length + 1;
     const u8 *S = gfxs->Bop[0];
     u8  *D      = gfxs->Aop[0];
     u32  Dkey   = gfxs->Dkey;
     int  Ostep  = gfxs->Ostep * 3;

     if (Ostep < 0) {
          S += (gfxs->length - 1) * 3;
          D += (gfxs->length - 1) * 3;
     }

     while (--w) {
          u32 pix = READ_PIXEL( D );
          if ((pix & RGB_MASK) == Dkey) {
               D[0] = S[0];
               D[1] = S[1];
               D[2] = S[2];
          }

          S += Ostep;
          D += Ostep;
     }
}

/********************************* Bop_PFI_KtoK_Aop_PFI ***********************/

static void Bop_PFI_OP_Aop_PFI(KtoK)( GenefxState *gfxs )
{
     int  w      = gfxs->length + 1;
     int  Ostep  = gfxs->Ostep * 3;
     const u8 *S = gfxs->Bop[0];
     u8  *D      = gfxs->Aop[0];
     u32  Skey   = gfxs->Skey;
     u32  Dkey   = gfxs->Dkey;

     if (Ostep < 0) {
          S += (gfxs->length - 1) * 3;
          D += (gfxs->length - 1) * 3;
     }

     while (--w) {
          u32 s = READ_PIXEL( S );
          if ((s & RGB_MASK) != Skey) {
               u32 d = READ_PIXEL( D );
               if ((d & RGB_MASK) == Dkey) {
                    D[0] = S[0];
                    D[1] = S[1];
                    D[2] = S[2];
               }
          }

          S += Ostep;
          D += Ostep;
     }
}

/********************************* Bop_PFI_SKto_Aop_PFI ***********************/

static void Bop_PFI_OP_Aop_PFI(SKto)( GenefxState *gfxs )
{
     int  w      = gfxs->length + 1;
     int  i      = gfxs->Xphase;
     int  SperD  = gfxs->SperD;
     const u8 *S = gfxs->Bop[0];
     u8  *D      = gfxs->Aop[0];
     int  Dstep  = gfxs->Astep * 3;
     u32  Skey   = gfxs->Skey;

     while (--w) {
          int pixelstart = (i >> 16) * 3;
          u32 s = READ_PIXEL( &S[pixelstart] );
          if ((s & RGB_MASK) != Skey) {
               D[0] = S[pixelstart++];
               D[1] = S[pixelstart++];
               D[2] = S[pixelstart];
          }

          D += Dstep;
          i += SperD;
     }
}

/********************************* Bop_PFI_StoK_Aop_PFI ***********************/

static void Bop_PFI_OP_Aop_PFI(StoK)( GenefxState *gfxs )
{
     int  w      = gfxs->length + 1;
     int  i      = gfxs->Xphase;
     int  SperD  = gfxs->SperD;
     const u8 *S = gfxs->Bop[0];
     u8  *D      = gfxs->Aop[0];
     u32  Dkey   = gfxs->Dkey;
     int  Dstep  = gfxs->Astep * 3;

     while (--w) {
          u32 d = READ_PIXEL( D );
          if ((d & RGB_MASK) == Dkey) {
               int pixelstart = (i >> 16) * 3;
               D[0] = S[pixelstart++];
               D[1] = S[pixelstart++];
               D[2] = S[pixelstart];
          }

          D += Dstep;
          i += SperD;
     }
}

/********************************* Bop_PFI_SKtoK_Aop_PFI **********************/

static void Bop_PFI_OP_Aop_PFI(SKtoK)( GenefxState *gfxs )
{
     int  w      = gfxs->length + 1;
     int  i      = gfxs->Xphase;
     int  SperD  = gfxs->SperD;
     const u8 *S = gfxs->Bop[0];
     u8  *D      = gfxs->Aop[0];
     u32  Skey   = gfxs->Skey;
     u32  Dkey   = gfxs->Dkey;

     while (--w) {
          int pixelstart = (i >> 16) * 3;
          u32 s = READ_PIXEL( &S[pixelstart] );

          if ((s & RGB_MASK) != Skey) {
               u32 d = READ_PIXEL( D );
               if ((d & RGB_MASK) == Dkey) {
                    D[0] = S[pixelstart++];
                    D[1] = S[pixelstart++];
                    D[2] = S[pixelstart];
               }
          }

          D += 3;
          i += SperD;
     }
}

/******************************************************************************/

#undef RGB_MASK
#undef Cop_OP_Aop_PFI
#undef Bop_PFI_OP_Aop_PFI

#undef READ_PIXEL
