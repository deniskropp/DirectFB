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
 * #define RGB_MASK 0x7fff
 * #define Cop_OP_Aop_PFI( op ) Cop_##op##_Aop_15
 * #define Bop_PFI_OP_Aop_PFI( op ) Bop_15_##op##_Aop_15
 * #include "template_ckey_16.h"
 */

#if RGB_MASK == 0xffff
#define MASK_RGB( p ) (p)
#else
#define MASK_RGB( p ) ((p) & RGB_MASK)
#endif

#define MASK_RGB_L( p ) ((p) & RGB_MASK)
#define MASK_RGB_H( p ) ((p) & (RGB_MASK << 16))
#define MASK_RGB_32( p ) ((p) & (RGB_MASK << 16 | RGB_MASK))

/********************************* Cop_toK_Aop_PFI ****************************/

static void Cop_OP_Aop_PFI(toK)( GenefxState *gfxs )
{
     int  l    = gfxs->length;
     u16 *D    = gfxs->Aop[0];
     u16  Dkey = gfxs->Dkey;
     u16  Cop  = gfxs->Cop;

     while (l--) {
          if (MASK_RGB( *D ) == Dkey)
               *D = Cop;

          D++;
     }
}

/********************************* Bop_PFI_Kto_Aop_PFI ************************/

static void Bop_PFI_OP_Aop_PFI(Kto)( GenefxState *gfxs )
{
     int  w, l  = gfxs->length;
     int  Ostep = gfxs->Ostep;
     u16 *S     = gfxs->Bop[0];
     u16 *D     = gfxs->Aop[0];
     u16  Skey  = gfxs->Skey;
     u32  SkeyH = gfxs->Skey << 16;

     /* blit direction */
     if (Ostep < 0) {
          S += gfxs->length - 1;
          D += gfxs->length - 1;
     }

     if (((long)S & 2) != ((long)D & 2)) {
          /* source and destination misaligned */
          while (l--) {
               u16 s = *S;

               if (MASK_RGB( s ) != Skey)
                    *D = s;

               S += Ostep;
               D += Ostep;
          }

          return;
     }

     /* fix alignment */
     if (Ostep > 0) {
          if ((long)D & 2) {
               /* align / leftmost pixel */
               u16 s = *S;

               if (MASK_RGB( s ) != Skey)
                    *D = s;

               S++;
               D++;
               l--;
          }
     } else { /* Ostep < 0 */
          if ((long)D & 2) {
               /* align */
               S--;
               D--;
          } else {
               /* rightmost pixel */
               u16 s = *S;

               if (MASK_RGB( s ) != Skey)
                    *D = s;

               S -= 2;
               D -= 2;
               l--;
          }
     }

     /* blit */
     Ostep <<= 1;
     w = l >> 1;
     while (w--) {
          u32 s = *(u32 *) S;

          if (MASK_RGB_L( s ) != Skey) {
               if (MASK_RGB_H( s ) != SkeyH) {
                    *(u32 *) D = s;
               } else {
#ifdef WORDS_BIGENDIAN
                    D[0] = (u16) s;
#else
                    D[1] = (u16) s;
#endif
               }
          } else if (MASK_RGB_H( s ) != SkeyH) {
#ifdef WORDS_BIGENDIAN
               D[1] = (u16) (s >> 16);
#else
               D[0] = (u16) (s >> 16);
#endif
          }

          S += Ostep;
          D += Ostep;
     }

     /* last potential pixel */
     if (l & 1) {
          u16 s;

          if (Ostep < 0) {
               S++;
               D++;
          }

          s = *S;
          if (MASK_RGB( s ) != Skey)
               *D = s;
     }
}

/********************************* Bop_PFI_toK_Aop_PFI ************************/

static void Bop_PFI_OP_Aop_PFI(toK)( GenefxState *gfxs )
{
     int  w, l  = gfxs->length;
     int  Ostep = gfxs->Ostep;
     u16 *S     = gfxs->Bop[0];
     u16 *D     = gfxs->Aop[0];
     u16  Dkey  = gfxs->Dkey;
     u32  DkeyH = gfxs->Dkey << 16;

     /* blit direction */
     if (Ostep < 0) {
          S += gfxs->length - 1;
          D += gfxs->length - 1;
     }

     if (((long)S & 2) != ((long)D & 2)) {
          /* source and destination misaligned */
          while (l--) {
               if (MASK_RGB( *D ) == Dkey)
                    *D = *S;

               S += Ostep;
               D += Ostep;
          }

          return;
     }

     /* fix alignment */
     if ((Ostep > 0)) {
          if ((long)D & 2) {
               /* align / leftmost pixel */
               if (MASK_RGB( *D ) == Dkey)
                    *D = *S;

               S++;
               D++;
               l--;
          }
     } else { /* Ostep < 0 */
          if ((long)D & 2) {
               /* align */
               S--;
               D--;
          } else {
               /* rightmost pixel */
               if (MASK_RGB( *D ) == Dkey)
                    *D = *S;

               S -= 2;
               D -= 2;
               l--;
          }
     }

     /* blit */
     Ostep <<= 1;
     w = l >> 1;
     while (w--) {
          u32 d = *(u32 *) D;

          if (MASK_RGB_32( d ) == (DkeyH | Dkey)) {
               *(u32 *) D = *(u32 *) S;
          } else {
               if (MASK_RGB_L( d ) == Dkey) {
#ifdef WORDS_BIGENDIAN
                    D[0] = S[0];
#else
                    D[1] = S[1];
#endif
               } else
               if (MASK_RGB_H( d ) == DkeyH) {
#ifdef WORDS_BIGENDIAN
                    D[1] = S[1];
#else
                    D[0] = S[0];
#endif
               }
          }

          S += Ostep;
          D += Ostep;
     }

     /* last potential pixel */
     if (l & 1) {
          if (Ostep < 0) {
               S++;
               D++;
          }

          if (MASK_RGB( *D ) == Dkey)
               *D = *S;
     }
}

/********************************* Bop_PFI_KtoK_Aop_PFI ***********************/

static void Bop_PFI_OP_Aop_PFI(KtoK)( GenefxState *gfxs )
{
     int  l     = gfxs->length;
     int  Ostep = gfxs->Ostep;
     u16 *S     = gfxs->Bop[0];
     u16 *D     = gfxs->Aop[0];
     u16  Skey  = gfxs->Skey;
     u16  Dkey  = gfxs->Dkey;

     if (Ostep < 0) {
          S += gfxs->length - 1;
          D += gfxs->length - 1;
     }

     while (l--) {
          u16 s = *S;

          if (MASK_RGB( s ) != Skey && MASK_RGB( *D ) == Dkey)
               *D = s;

          S += Ostep;
          D += Ostep;
     }
}

/********************************* Bop_PFI_SKto_Aop_PFI ***********************/

static void Bop_PFI_OP_Aop_PFI(SKto)( GenefxState *gfxs )
{
     int  l     = gfxs->length;
     int  i     = gfxs->Xphase;
     int  SperD = gfxs->SperD;
     u16 *S     = gfxs->Bop[0];
     u16 *D     = gfxs->Aop[0];
     u16  Skey  = gfxs->Skey;

     while (l--) {
          u16 s = S[i>>16];

          if (MASK_RGB( s ) != Skey)
               *D = s;

          D++;
          i += SperD;
     }
}

/********************************* Bop_PFI_StoK_Aop_PFI ***********************/

static void Bop_PFI_OP_Aop_PFI(StoK)( GenefxState *gfxs )
{
     int  l     = gfxs->length;
     int  i     = gfxs->Xphase;
     int  SperD = gfxs->SperD;
     u16 *S     = gfxs->Bop[0];
     u16 *D     = gfxs->Aop[0];
     u16  Dkey  = gfxs->Dkey;

     while (l--) {
          if (MASK_RGB( *D ) != Dkey)
               *D = S[i>>16];

          D++;
          i += SperD;
     }
}

/********************************* Bop_PFI_SKtoK_Aop_PFI **********************/

static void Bop_PFI_OP_Aop_PFI(SKtoK)( GenefxState *gfxs )
{
     int  l     = gfxs->length;
     int  i     = gfxs->Xphase;
     int  SperD = gfxs->SperD;
     u16 *S     = gfxs->Bop[0];
     u16 *D     = gfxs->Aop[0];
     u16  Skey  = gfxs->Skey;
     u16  Dkey  = gfxs->Dkey;

     while (l--) {
          u16 s = S[i>>16];

          if (MASK_RGB( s ) != Skey && MASK_RGB( *D ) == Dkey)
               *D = s;

          D++;
          i += SperD;
     }
}

/******************************************************************************/

#undef MASK_RGB
#undef MASK_RGB_L
#undef MASK_RGB_H
#undef MASK_RGB_32

#undef RGB_MASK
#undef Cop_OP_Aop_PFI
#undef Bop_PFI_OP_Aop_PFI
