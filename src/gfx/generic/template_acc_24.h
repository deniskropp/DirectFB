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
 * #define A_SHIFT 16
 * #define R_SHIFT 11
 * #define G_SHIFT 5
 * #define B_SHIFT 0
 * #define A_MASK 0xff0000
 * #define R_MASK 0x00f800
 * #define G_MASK 0x0007e0
 * #define B_MASK 0x00001f
 * #define PIXEL_OUT( a, r, g, b ) PIXEL_ARGB8565( a, r, g, b )
 * #define EXPAND_Ato8( a ) (a)
 * #define EXPAND_Rto8( r ) EXPAND_5to8( r )
 * #define EXPAND_Gto8( g ) EXPAND_6to8( g )
 * #define EXPAND_Bto8( b ) EXPAND_5to8( b )
 * #define Sop_PFI_OP_Dacc( op ) Sop_argb8565_##op##_Dacc
 * #define Sacc_OP_Aop_PFI( op ) Sacc_##op##_Aop_argb8565
 * #include "template_acc_24.h"
 */

#define RGB_MASK (R_MASK | G_MASK | B_MASK)

#if RGB_MASK == 0xffffff
#define MASK_RGB( p ) (p)
#else
#define MASK_RGB( p ) ((p) & RGB_MASK)
#endif

#define PIXEL( x ) PIXEL_OUT( ((x).RGB.a & 0xFF00) ? 0xFF : (x).RGB.a, \
                              ((x).RGB.r & 0xFF00) ? 0xFF : (x).RGB.r, \
                              ((x).RGB.g & 0xFF00) ? 0xFF : (x).RGB.g, \
                              ((x).RGB.b & 0xFF00) ? 0xFF : (x).RGB.b )

#define EXPAND( d, s ) do { \
     (d).RGB.a = EXPAND_Ato8( (s & A_MASK) >> A_SHIFT ); \
     (d).RGB.r = EXPAND_Rto8( (s & R_MASK) >> R_SHIFT ); \
     (d).RGB.g = EXPAND_Gto8( (s & G_MASK) >> G_SHIFT ); \
     (d).RGB.b = EXPAND_Bto8( (s & B_MASK) >> B_SHIFT ); \
} while (0)

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

/********************************* Sop_PFI_Sto_Dacc ***************************/

static void Sop_PFI_OP_Dacc(Sto)( GenefxState *gfxs )
{
     int w     = gfxs->length + 1;
     int i     = gfxs->Xphase;
     int SperD = gfxs->SperD;

     GenefxAccumulator *D = gfxs->Dacc;
     const u8          *S = gfxs->Sop[0];

     int Ostep = gfxs->Ostep;

     if (Ostep != 1)
          D_UNIMPLEMENTED();

     while (--w) {
          int pixelstart = (i >> 16) * 3;

          u32 s = READ_PIXEL( &S[pixelstart] );

          EXPAND( *D, s );

          ++D;
          i += SperD;
     }
}

/********************************* Sop_PFI_SKto_Dacc **************************/

static void Sop_PFI_OP_Dacc(SKto)( GenefxState *gfxs )
{
     int w     = gfxs->length + 1;
     int i     = gfxs->Xphase;
     int SperD = gfxs->SperD;
     u32 Skey  = gfxs->Skey;

     GenefxAccumulator *D = gfxs->Dacc;
     const u8          *S = gfxs->Sop[0];

     int Ostep = gfxs->Ostep;

     if (Ostep != 1)
          D_UNIMPLEMENTED();

     while (--w) {
          int pixelstart = (i >> 16) * 3;

          u32 s = READ_PIXEL( &S[pixelstart] );

          if (MASK_RGB( s ) != Skey)
               EXPAND( *D, s );
          else
               D->RGB.a = 0xF000;

          ++D;
          i += SperD;
     }
}

/********************************* Sop_PFI_to_Dacc ****************************/

static void Sop_PFI_OP_Dacc(to)( GenefxState *gfxs )
{
     int  w      = gfxs->length + 1;
     const u8 *S = gfxs->Sop[0];
     int  Ostep  = gfxs->Ostep * 3;

     GenefxAccumulator *D = gfxs->Dacc;

     while (--w) {
          u32 s = READ_PIXEL( S );

          EXPAND( *D, s );

          ++D;
          S += Ostep;
     }
}

/********************************* Sop_PFI_Kto_Dacc ***************************/

static void Sop_PFI_OP_Dacc(Kto)( GenefxState *gfxs )
{
     int  w      = gfxs->length + 1;
     const u8 *S = gfxs->Sop[0];
     u32  Skey   = gfxs->Skey;
     GenefxAccumulator *D = gfxs->Dacc;

     int Ostep = gfxs->Ostep * 3;

     while (--w) {
          u32 s = READ_PIXEL( S );

          if (MASK_RGB( s ) != Skey)
               EXPAND( *D, s );
          else
               D->RGB.a = 0xF000;

          ++D;
          S += Ostep;
     }
}

/********************************* Sacc_to_Aop_PFI ****************************/

static void Sacc_OP_Aop_PFI(to)( GenefxState *gfxs )
{
     int l = gfxs->length + 1;
     const GenefxAccumulator *S = gfxs->Sacc;
     u8  *D     = gfxs->Aop[0];
     int  Dstep = gfxs->Astep * 3;

     while (--l) {
          if (!(S->RGB.a & 0xF000)) {
               u32 pix = PIXEL( *S );
               WRITE_PIXEL( D, pix );
          }

          ++S;
          D += Dstep;
     }
}

/********************************* Sacc_Sto_Aop_PFI ***************************/

static void Sacc_OP_Aop_PFI(Sto)( GenefxState *gfxs )
{
     int                w     = gfxs->length + 1;
     int                i     = gfxs->Xphase;
     int                SperD = gfxs->SperD;
     const GenefxAccumulator *Sacc = gfxs->Sacc;
     u8                *D     = gfxs->Aop[0];
     int                Dstep = gfxs->Astep * 3;

     while (--w) {
          const GenefxAccumulator *S = &Sacc[i>>16];

          if (!(S->RGB.a & 0xF000)) {
               u32 pix = PIXEL( *S );
               WRITE_PIXEL( D, pix );
          }

          D += Dstep;
          i += SperD;
     }
}

/********************************* Sacc_toK_Aop_PFI ***************************/

static void Sacc_OP_Aop_PFI(toK)( GenefxState *gfxs )
{
     int  w     = gfxs->length + 1;
     u8  *D     = gfxs->Aop[0];
     u32  Dkey  = gfxs->Dkey;
     int  Dstep = gfxs->Astep * 3;
     const GenefxAccumulator *S = gfxs->Sacc;

     while (--w) {
          if (!(S->RGB.a & 0xF000)) {
               u32 pix = READ_PIXEL( D );
               if (MASK_RGB( pix ) == Dkey) {
                    pix = PIXEL( *S );
                    WRITE_PIXEL( D, pix );
               }
          }

          S++;
          D += Dstep;
     }
}

/********************************* Sacc_StoK_Aop_PFI **************************/

static void Sacc_OP_Aop_PFI(StoK)( GenefxState *gfxs )
{
     int  w     = gfxs->length + 1;
     int  i     = gfxs->Xphase;
     int  SperD = gfxs->SperD;
     u8  *D     = gfxs->Aop[0];
     int  Dstep = gfxs->Astep * 3;
     u32  Dkey  = gfxs->Dkey;
     const GenefxAccumulator *Sacc  = gfxs->Sacc;

     while (--w) {
          const GenefxAccumulator *S = &Sacc[i>>16];

          if (!(S->RGB.a & 0xF000)) {
               u32 pix = READ_PIXEL( D );
               if (MASK_RGB( pix ) == Dkey) {
                    pix = PIXEL( *S );
                    WRITE_PIXEL( D, pix );
               }
          }

          D += Dstep;
          i += SperD;
     }
}

/********************************* Sop_PFI_TEX_to_Dacc ***************************/

static void Sop_PFI_OP_Dacc(TEX_to)( GenefxState *gfxs )
{
     int                w     = gfxs->length + 1;
     int                s     = gfxs->s;
     int                t     = gfxs->t;
     int                SperD = gfxs->SperD;
     int                TperD = gfxs->TperD;
     u8                *S     = gfxs->Sop[0];
     int                Ostep = gfxs->Ostep * 3;
     GenefxAccumulator *D     = gfxs->Dacc;
     int                sp3   = gfxs->src_pitch / 3;

     if (Ostep != 1)
          D_UNIMPLEMENTED();

     while (--w) {
          int pixelstart = ((s>>16) + (t>>16) * sp3) * 3;
          u32 p = READ_PIXEL( &S[pixelstart] );

          EXPAND( *D, p );

          D++;
          s += SperD;
          t += TperD;
     }
}

/********************************* Sop_PFI_TEX_Kto_Dacc ***************************/

static void Sop_PFI_OP_Dacc(TEX_Kto)( GenefxState *gfxs )
{
     int                w     = gfxs->length + 1;
     int                s     = gfxs->s;
     int                t     = gfxs->t;
     int                SperD = gfxs->SperD;
     int                TperD = gfxs->TperD;
     const u8          *S     = gfxs->Sop[0];
     u32                Skey  = gfxs->Skey;
     int                Ostep = gfxs->Ostep * 3;
     GenefxAccumulator *D     = gfxs->Dacc;
     int                sp3   = gfxs->src_pitch / 3;

     if (Ostep != 1)
          D_UNIMPLEMENTED();

     while (--w) {
          int pixelstart = ((s>>16) + (t>>16) * sp3) * 3;
          u32 p = READ_PIXEL( &S[pixelstart] );

          if (MASK_RGB( p ) != Skey)
               EXPAND( *D, p );
          else
               D->RGB.a = 0xF000;

          D++;
          s += SperD;
          t += TperD;
     }
}

/******************************************************************************/

#undef RGB_MASK
#undef MASK_RGB
#undef PIXEL
#undef EXPAND

#undef A_SHIFT
#undef R_SHIFT
#undef G_SHIFT
#undef B_SHIFT
#undef A_MASK
#undef R_MASK
#undef G_MASK
#undef B_MASK
#undef PIXEL_OUT
#undef EXPAND_Ato8
#undef EXPAND_Rto8
#undef EXPAND_Gto8
#undef EXPAND_Bto8
#undef Sop_PFI_OP_Dacc
#undef Sacc_OP_Aop_PFI

#undef READ_PIXEL
#undef WRITE_PIXEL
