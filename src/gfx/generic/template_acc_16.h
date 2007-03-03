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
 * #define A_SHIFT 15
 * #define R_SHIFT 10
 * #define G_SHIFT 5
 * #define B_SHIFT 0
 * #define A_MASK 0x8000
 * #define R_MASK 0x7c00
 * #define G_MASK 0x03e0
 * #define B_MASK 0x001f
 * #define PIXEL_OUT( a, r, g, b ) PIXEL_ARGB1555( a, r, g, b )
 * #define EXPAND_Ato8( a ) EXPAND_1to8( a )
 * #define EXPAND_Rto8( r ) EXPAND_5to8( r )
 * #define EXPAND_Gto8( g ) EXPAND_5to8( g )
 * #define EXPAND_Bto8( b ) EXPAND_5to8( b )
 * #define Sop_PFI_OP_Dacc( op ) Sop_argb1555_##op##_Dacc
 * #define Sacc_OP_Aop_PFI( op ) Sacc_##op##_Aop_argb1555
 * #include "template_acc_16.h"
 */

#define RGB_MASK (R_MASK | G_MASK | B_MASK)

#if RGB_MASK == 0xffff
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

/********************************* Sop_PFI_Sto_Dacc ***************************/

static void Sop_PFI_OP_Dacc(Sto)( GenefxState *gfxs )
{
     int                l     = gfxs->length;
     int                i     = gfxs->Xphase;
     int                SperD = gfxs->SperD;
     u16               *S     = gfxs->Sop[0];
     GenefxAccumulator *D     = gfxs->Dacc;

     while (l--) {
          u16 s = S[i>>16];

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
     u16               *S     = gfxs->Sop[0];
     GenefxAccumulator *D     = gfxs->Dacc;
     u16                Skey  = gfxs->Skey;

     while (l--) {
          u16 s = S[i>>16];

          if (MASK_RGB( s ) != Skey)
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
     int             w, l = gfxs->length;
     u16               *S = gfxs->Sop[0];
     GenefxAccumulator *D = gfxs->Dacc;

     if ((long)S & 2) {
          u16 s = *S++;

          EXPAND( *D, s );

          D++;
          l--;
     }

     w = l >> 1;
     while (w--) {
          u32 s = *(u32 *) S;

#ifdef WORDS_BIGENDIAN
          EXPAND( D[0], s >> 16 );
          EXPAND( D[1], s );
#else
          EXPAND( D[0], s );
          EXPAND( D[1], s >> 16);
#endif

          S += 2;
          D += 2;
     }

     if (l & 1) {
          u16 s = *S;

          EXPAND( *D, s );
     }
}

/********************************* Sop_PFI_Kto_Dacc ***************************/

static void Sop_PFI_OP_Dacc(Kto)( GenefxState *gfxs )
{
     int                l    = gfxs->length;
     u16               *S    = gfxs->Sop[0];
     GenefxAccumulator *D    = gfxs->Dacc;
     u16                Skey = gfxs->Skey;

     while (l--) {
          u16 s = *S++;

          if (MASK_RGB( s ) != Skey)
               EXPAND( *D, s );
          else
               D->RGB.a = 0xF000;

          D++;
     }
}

/********************************* Sacc_to_Aop_PFI ****************************/

static void Sacc_OP_Aop_PFI(to)( GenefxState *gfxs )
{
     int             w, l = gfxs->length;
     GenefxAccumulator *S = gfxs->Sacc;
     u16               *D = gfxs->Aop[0];

     if ((long)D & 2) {
          if (!(S->RGB.a & 0xF000))
               *D = PIXEL( *S );

          S++;
          D++;
          l--;
     }

     w = l >> 1;
     while (w--) {
          u32 *D2 = (u32 *) D;

          if (!(S[0].RGB.a & 0xF000) && !(S[1].RGB.a & 0xF000)) {
#ifdef WORDS_BIGENDIAN
               *D2 = PIXEL( S[1] ) | PIXEL( S[0] ) << 16;
#else
               *D2 = PIXEL( S[0] ) | PIXEL( S[1] ) << 16;
#endif
          } else {
               if (!(S[0].RGB.a & 0xF000))
                    D[0] = PIXEL( S[0] );
               else if (!(S[1].RGB.a & 0xF000))
                    D[1] = PIXEL( S[1] );
          }

          S += 2;
          D += 2;
     }

     if (l & 1) {
          if (!(S->RGB.a & 0xF000))
               *D = PIXEL( *S );
     }
}

/********************************* Sacc_Sto_Aop_PFI ***************************/

static void Sacc_OP_Aop_PFI(Sto)( GenefxState *gfxs )
{
     int             w, l     = gfxs->length;
     int                i     = gfxs->Xphase;
     int                SperD = gfxs->SperD;
     GenefxAccumulator *Sacc  = gfxs->Sacc;
     u16               *D     = gfxs->Aop[0];

     if ((long)D & 2) {
          GenefxAccumulator *S = Sacc;

          if (!(S->RGB.a & 0xF000))
               *D = PIXEL( *S );

          D++;
          l--;
          i += SperD;
     }

     w = l >> 1;
     while (w--) {
          GenefxAccumulator *S0 = &Sacc[i>>16];
          GenefxAccumulator *S1 = &Sacc[(i+SperD)>>16];
          u32               *D2 = (u32 *) D;

          if (!(S0->RGB.a & 0xF000) && !(S1->RGB.a & 0xF000)) {
#ifdef WORDS_BIGENDIAN
               *D2 = PIXEL( *S1 ) | PIXEL( *S0 ) << 16;
#else
               *D2 = PIXEL( *S0 ) | PIXEL( *S1 ) << 16;
#endif
          } else {
               if (!(S0->RGB.a & 0xF000))
                    D[0] = PIXEL( *S0 );
               else if (!(S1->RGB.a & 0xF000))
                    D[1] = PIXEL( *S1 );
          }

          D += 2;
          i += SperD << 1;
     }

     if (l & 1) {
          GenefxAccumulator *S = &Sacc[i>>16];

          if (!(S->RGB.a & 0xF000))
               *D = PIXEL( *S );
     }
}

/********************************* Sacc_toK_Aop_PFI ***************************/

static void Sacc_OP_Aop_PFI(toK)( GenefxState *gfxs )
{
     int                l    = gfxs->length;
     GenefxAccumulator *S    = gfxs->Sacc;
     u16               *D    = gfxs->Aop[0];
     u16                Dkey = gfxs->Dkey;

     while (l--) {
          if (!(S->RGB.a & 0xF000) && MASK_RGB( *D ) == Dkey)
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
     u16               *D     = gfxs->Aop[0];
     u16                Dkey  = gfxs->Dkey;

     while (l--) {
          GenefxAccumulator *S = &Sacc[i>>16];

          if (!(S->RGB.a & 0xF000) && MASK_RGB( *D ) == Dkey)
               *D = PIXEL( *S );

          D++;
          i += SperD;
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
