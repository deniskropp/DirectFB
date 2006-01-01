/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2005  convergence GmbH.

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


static void Cop_to_Aop_32_64( GenefxState *gfxs )
{
     int    w, l = gfxs->length;
     __u32 *D    = gfxs->Aop[0];
     __u32  Cop  = gfxs->Cop;
     __u64  DCop = ((__u64)Cop << 32) | Cop;
     
     if ((long)D & 4) {
          *D++ = Cop;
          l--;
     }

     for (w = l >> 1; w; w--) {
          *((__u64*)D) = DCop;
          D += 2;
     }
     
     if (l & 1)
          *D = Cop;
}

static void Bop_rgb32_Kto_Aop_64( GenefxState *gfxs )
{
     int    w, l  = gfxs->length;
     __u32 *D     = gfxs->Aop[0];
     __u32 *S     = gfxs->Bop[0];
     __u32  Skey  = gfxs->Skey;
     __u64  DSkey = ((__u64)Skey << 32) | Skey;
     
     if ((long)D & 4) {
          if (*S != Skey)
               *D = *S;
          D++;
          S++;
          l--;
     }               

     for (w = l >> 1; w; w--) {
          __u64 s = *((__u64*)S);

          if (s != DSkey) {
               if ((s & 0x00ffffff00000000ull) != 
                    (DSkey & 0x00ffffff00000000ull)) {
                    if ((s & 0x0000000000ffffffull) != 
                         (DSkey & 0x0000000000ffffffull)) {
                         *((__u64*)D) = s;
                    }
                    else {
#ifdef WORDS_BIGENDIAN
                         D[0] = (__u32)(s >> 32);
#else
                         D[1] = (__u32)(s >> 32);
#endif
                    }
               }
               else {
#ifdef WORDS_BIGENDIAN
                    D[1] = (__u32)s;
#else
                    D[0] = (__u32)s;
#endif
               }
          }
          S += 2;
          D += 2;
     }
     
     if (l & 1) {                 /* do the last potential pixel */
          if (*S != Skey)
               *D = *S;
     }
}

static void Bop_rgb32_toK_Aop_64( GenefxState *gfxs )
{
     int    w, l  = gfxs->length;
     __u32 *D     = gfxs->Aop[0];
     __u32 *S     = gfxs->Bop[0];
     __u32  Dkey  = gfxs->Dkey;
     __u64  DDkey = ((__u64)Dkey << 32) | Dkey;
     
     if ((long)D & 4) {
          if (*D == Dkey)
               *D = *S;
          D++;
          S++;
          l--;
     }               

     for (w = l >> 1; w; w--) {
          __u64 d = *((__u64*)D);

          if (d != DDkey) {
               if ((d & 0x00ffffff00000000ull) ==
                    (DDkey & 0x00ffffff00000000ull)) {
                    if ((d & 0x0000000000ffffffull) ==
                         (DDkey & 0x0000000000ffffffull)) {
                         *((__u64*)D) = *((__u64*)S);
                    }
                    else {
#ifdef WORDS_BIGENDIAN
                         D[0] = S[0];
#else
                         D[1] = S[1];
#endif
                    }
               }
               else {
#ifdef WORDS_BIGENDIAN
                    D[1] = S[1];
#else
                    D[0] = S[0];
#endif
               }
          }
          S += 2;
          D += 2;
     }
     
     if (l & 1) {                 /* do the last potential pixel */
          if (*D == Dkey)
               *D = *S;
     }
}

static void Bop_32_Sto_Aop_64( GenefxState *gfxs )
{
     int    w, l   = gfxs->length;
     int    i      = 0;
     __u32 *D      = gfxs->Aop[0];
     __u32 *S      = gfxs->Bop[0];
     int    SperD  = gfxs->SperD;
     int    SperD2 = SperD << 1;
     
     if ((long)D & 4) {
          *D++ = *S;
          i += SperD;
          l--;
     }
     
     for (w = l >> 1; w; w--) {
#ifdef WORDS_BIGENDIAN
          *((__u64*)D) = ((__u64)S[i>>16] << 32) | S[(i+SperD)>>16];
#else
          *((__u64*)D) = ((__u64)S[(i+SperD)>>16] << 32) | S[i>>16];
#endif
          D += 2;
          i += SperD2;
     }
     
     if (l & 1)
          *D = S[i>>16];
}

static void Dacc_xor_64( GenefxState *gfxs )
{
     int    w     = gfxs->length;
     __u64 *D     = (__u64*)gfxs->Dacc;
     __u64  color;

#ifdef WORDS_BIGENDIAN
     color = ((__u64)gfxs->color.b << 48) |
             ((__u64)gfxs->color.g << 32) |
             ((__u64)gfxs->color.r << 16) |
             ((__u64)gfxs->color.a);
#else
     color = ((__u64)gfxs->color.a << 48) |
             ((__u64)gfxs->color.r << 32) |
             ((__u64)gfxs->color.g << 16) |
             ((__u64)gfxs->color.b);
#endif

     for (; w; w--) {
          *D ^= color;
          D++;
     }
}

