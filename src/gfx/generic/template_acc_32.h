/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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
 * #define A_SHIFT 24
 * #define R_SHIFT 16
 * #define G_SHIFT 8
 * #define B_SHIFT 0
 * #define A_MASK 0xff000000
 * #define R_MASK 0x00ff0000
 * #define G_MASK 0x0000ff00
 * #define B_MASK 0x000000ff
 * #define PIXEL_OUT( a, r, g, b ) PIXEL_AiRGB( a, r, g, b )
 * #define EXPAND_Ato8( a ) ((a) ^ 0xff)
 * #define EXPAND_Rto8( r ) (r)
 * #define EXPAND_Gto8( g ) (g)
 * #define EXPAND_Bto8( b ) (b)
 * #define Sop_PFI_OP_Dacc( op ) Sop_airgb_##op##_Dacc
 * #define Sacc_OP_Aop_PFI( op ) Sacc_##op##_Aop_airgb
 * #include "template_acc_32.h"
 */

#define RGB_MASK (R_MASK | G_MASK | B_MASK)

#define PIXEL( s ) PIXEL_OUT( ((s).RGB.a & 0xFF00) ? 0xFF : (s).RGB.a, \
                              ((s).RGB.r & 0xFF00) ? 0xFF : (s).RGB.r, \
                              ((s).RGB.g & 0xFF00) ? 0xFF : (s).RGB.g, \
                              ((s).RGB.b & 0xFF00) ? 0xFF : (s).RGB.b )

#define EXPAND( d, s ) \
do { \
     (d).RGB.a = EXPAND_Ato8( (s & A_MASK) >> A_SHIFT ); \
     (d).RGB.r = EXPAND_Rto8( (s & R_MASK) >> R_SHIFT ); \
     (d).RGB.g = EXPAND_Gto8( (s & G_MASK) >> G_SHIFT ); \
     (d).RGB.b = EXPAND_Bto8( (s & B_MASK) >> B_SHIFT ); \
} while (0)

/********************************* Sop_PFI_Sto_Dacc ***************************/

static void Sop_PFI_OP_Dacc(Sto)( GenefxState *gfxs )
{
     int                l     = gfxs->length;
     int                i     = gfxs->Xphase;
     int                SperD = gfxs->SperD;
     u32               *S     = gfxs->Sop[0];
     GenefxAccumulator *D     = gfxs->Dacc;

     while (l--) {
          u32 s = S[i>>16];

          EXPAND( *D, s );
          
          D++;
          i += SperD;
     }
}

/********************************* Sop_PFI_SKto_Dacc **************************/

static void Sop_PFI_OP_Dacc(SKto)( GenefxState *gfxs )
{
     int                l     = gfxs->length;
     int                i     = gfxs->Xphase;
     int                SperD = gfxs->SperD;
     u32               *S     = gfxs->Sop[0];
     GenefxAccumulator *D     = gfxs->Dacc;
     u32                Skey  = gfxs->Skey;

     while (l--) {
          u32 s = S[i>>16];

          if ((s & RGB_MASK) != Skey)
               EXPAND( *D, s );
          else
               D->RGB.a = 0xF000;

          D++;
          i += SperD;
     }
}

/********************************* Sop_PFI_to_Dacc ****************************/

static void Sop_PFI_OP_Dacc(to)( GenefxState *gfxs )
{
     int                l = gfxs->length;
     u32               *S = gfxs->Sop[0];
     GenefxAccumulator *D = gfxs->Dacc;

     while (l--) {
          u32 s = *S++;

          EXPAND( *D, s );

          D++;
     }
}

/********************************* Sop_PFI_Kto_Dacc ***************************/

static void Sop_PFI_OP_Dacc(Kto)( GenefxState *gfxs )
{
     int                l    = gfxs->length;
     u32               *S    = gfxs->Sop[0];
     GenefxAccumulator *D    = gfxs->Dacc;
     u32                Skey = gfxs->Skey;

     while (l--) {
          u32 s = *S++;

          if ((s & RGB_MASK) != Skey)
               EXPAND( *D, s );
          else
               D->RGB.a = 0xF000;

          D++;
     }
}

/********************************* Sacc_to_Aop_PFI ****************************/

static void Sacc_OP_Aop_PFI(to)( GenefxState *gfxs )
{
     int                l = gfxs->length;
     GenefxAccumulator *S = gfxs->Sacc;
     u32               *D = gfxs->Aop[0];

     while (l--) {
          if (!(S->RGB.a & 0xF000))
               *D = PIXEL( *S );

          S++;
          D++;
     }
}

/********************************* Sacc_Sto_Aop_PFI ***************************/

static void Sacc_OP_Aop_PFI(Sto)( GenefxState *gfxs )
{
     int                l     = gfxs->length;
     int                i     = gfxs->Xphase;
     int                SperD = gfxs->SperD;
     GenefxAccumulator *Sacc  = gfxs->Sacc;
     u32               *D     = gfxs->Aop[0];

     while (l--) {
          GenefxAccumulator *S = &Sacc[i>>16];

          if (!(S->RGB.a & 0xF000))
               *D = PIXEL( *S );

          D++;
          i += SperD;
     }
}

/********************************* Sacc_toK_Aop_PFI ***************************/

static void Sacc_OP_Aop_PFI(toK)( GenefxState *gfxs )
{
     int                l    = gfxs->length;
     GenefxAccumulator *S    = gfxs->Sacc;
     u32               *D    = gfxs->Aop[0];
     u32                Dkey = gfxs->Dkey;

     while (l--) {
          if (!(S->RGB.a & 0xF000) && (*D & RGB_MASK) == Dkey)
               *D = PIXEL( *S );

          S++;
          D++;
     }
}

/********************************* Sacc_StoK_Aop_PFI **************************/

static void Sacc_OP_Aop_PFI(StoK)( GenefxState *gfxs )
{
     int                l     = gfxs->length;
     int                i     = gfxs->Xphase;
     int                SperD = gfxs->SperD;
     GenefxAccumulator *Sacc  = gfxs->Sacc;
     u32               *D     = gfxs->Aop[0];
     u32                Dkey  = gfxs->Dkey;

     while (l--) {
          GenefxAccumulator *S = &Sacc[i>>16];

          if (!(S->RGB.a & 0xF000) && (*D & RGB_MASK) == Dkey)
               *D = PIXEL( *S );

          D++;
          i += SperD;
     }
}

/******************************************************************************/

#undef RGB_MASK
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
