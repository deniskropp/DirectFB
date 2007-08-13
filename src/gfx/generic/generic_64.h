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


static void Cop_to_Aop_32_64( GenefxState *gfxs )
{
     int    w, l = gfxs->length;
     u32   *D    = gfxs->Aop[0];
     u32    Cop  = gfxs->Cop;
     u64    DCop = ((u64)Cop << 32) | Cop;
     
     if ((long)D & 4) {
          *D++ = Cop;
          l--;
     }

     for (w = l >> 1; w; w--) {
          *((u64*)D) = DCop;
          D += 2;
     }
     
     if (l & 1)
          *D = Cop;
}

static void Bop_rgb32_Kto_Aop_64( GenefxState *gfxs )
{
     int    w, l  = gfxs->length;
     u32   *D     = gfxs->Aop[0];
     u32   *S     = gfxs->Bop[0];
     u32    Skey  = gfxs->Skey;
     u64    DSkey = ((u64)Skey << 32) | Skey;
     
     if ((long)D & 4) {
          if ((*S & 0x00ffffff) != Skey)
               *D = *S;
          D++;
          S++;
          l--;
     }               

     for (w = l >> 1; w; w--) {
          u64 s = *((u64*)S);

          if ((s & 0x00ffffff00ffffffull) != DSkey) {
               if ((s & 0x00ffffff00000000ull) != 
                    (DSkey & 0x00ffffff00000000ull)) {
                    if ((s & 0x0000000000ffffffull) != 
                         (DSkey & 0x0000000000ffffffull)) {
                         *((u64*)D) = s;
                    }
                    else {
#ifdef WORDS_BIGENDIAN
                         D[0] = (u32)(s >> 32);
#else
                         D[1] = (u32)(s >> 32);
#endif
                    }
               }
               else {
#ifdef WORDS_BIGENDIAN
                    D[1] = (u32)s;
#else
                    D[0] = (u32)s;
#endif
               }
          }
          S += 2;
          D += 2;
     }
     
     if (l & 1) {                 /* do the last potential pixel */
          if ((*S & 0x00ffffff) != Skey)
               *D = *S;
     }
}

static void Bop_rgb32_toK_Aop_64( GenefxState *gfxs )
{
     int    w, l  = gfxs->length;
     u32   *D     = gfxs->Aop[0];
     u32   *S     = gfxs->Bop[0];
     u32    Dkey  = gfxs->Dkey;
     u64    DDkey = ((u64)Dkey << 32) | Dkey;
     
     if ((long)D & 4) {
          if ((*D & 0x00ffffff) == Dkey)
               *D = *S;
          D++;
          S++;
          l--;
     }               

     for (w = l >> 1; w; w--) {
          u64 d = *((u64*)D);

          if ((d & 0x00ffffff00ffffffull) != DDkey) {
               if ((d & 0x00ffffff00000000ull) ==
                    (DDkey & 0x00ffffff00000000ull)) {
                    if ((d & 0x0000000000ffffffull) ==
                         (DDkey & 0x0000000000ffffffull)) {
                         *((u64*)D) = *((u64*)S);
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
          if ((*D & 0x00ffffff) == Dkey)
               *D = *S;
     }
}

static void Bop_32_Sto_Aop_64( GenefxState *gfxs )
{
     int    w, l   = gfxs->length;
     int    i      = 0;
     u32   *D      = gfxs->Aop[0];
     u32   *S      = gfxs->Bop[0];
     int    SperD  = gfxs->SperD;
     int    SperD2 = SperD << 1;
     
     if ((long)D & 4) {
          *D++ = *S;
          i += SperD;
          l--;
     }
     
     for (w = l >> 1; w; w--) {
#ifdef WORDS_BIGENDIAN
          *((u64*)D) = ((u64)S[i>>16] << 32) | S[(i+SperD)>>16];
#else
          *((u64*)D) = ((u64)S[(i+SperD)>>16] << 32) | S[i>>16];
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
     u64   *D     = (u64*)gfxs->Dacc;
     u64    color;

#ifdef WORDS_BIGENDIAN
     color = ((u64)gfxs->color.b << 48) |
             ((u64)gfxs->color.g << 32) |
             ((u64)gfxs->color.r << 16) |
             ((u64)gfxs->color.a);
#else
     color = ((u64)gfxs->color.a << 48) |
             ((u64)gfxs->color.r << 32) |
             ((u64)gfxs->color.g << 16) |
             ((u64)gfxs->color.b);
#endif

     for (; w; w--) {
          *D ^= color;
          D++;
     }
}

