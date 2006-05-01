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

#include <fbdev/fb.h>

#include <math.h>

#include <directfb.h>

#include <direct/messages.h>
#include <direct/util.h>

#include "regs.h"
#include "mmio.h"
#include "matrox.h"
#include "matrox_3d.h"


#ifdef ARCH_X86
#define RINT(x) my_rint(x)
#define CEIL(x) my_ceil(x)
#define FLOOR(x) my_floor(x)
#else
#define RINT(x) ((__s32)(x))
#define CEIL(x) ((__s32)ceil(x))
#define FLOOR(x) ((__s32)floor(x))
#endif


static inline long
my_rint(const float x)
{
     register float arg = x;
     long result;
     __asm__ ("fistl %0" : "=m" (result) : "t" (arg));
     return result;
}

static inline long
my_ceil(const float x)
{
     register float arg = x;
     volatile long value;
     volatile short cw, cwtmp;

     __asm__ volatile ("fnstcw %0" : "=m" (cw) : );
     cwtmp = (cw & 0xf3ff) | 0x0800; /* rounding up */
     __asm__ volatile ("fldcw %1\n"
                       "fistl %0\n"
                       "fldcw %2"
                       : "=m" (value)
                       : "m" (cwtmp), "m" (cw), "t" (arg));
     return value;
}

static inline long
my_floor(const float x)
{
     register float arg = x;
     volatile long value;
     volatile short cw, cwtmp;

     __asm__ volatile ("fnstcw %0" : "=m" (cw) : );
     cwtmp = (cw & 0xf3ff) | 0x0400;
     __asm__ volatile ("fldcw %1\n"
                       "fistl %0\n"
                       "fldcw %2"
                       : "=m" (value)
                       : "m" (cwtmp), "m" (cw), "t" (arg));
     return value;
}


#define F2COL(x) (RINT(x) & 0x00ffffff)

#define mgaF1800(x) (((__s32) (x)) & 0x0003ffff)
#define mgaF2400(x) (((__s32) (x)) & 0x00ffffff)
#define mgaF2200(x) (((__s32) (x)) & 0x003fffff)

#define OUTREG(r,d)  do { mga_out32( mmio, d, r ); } while (0)

#define MGA_S(start,xinc,yinc)                                        \
     do {                                                             \
          mga_out32( mmio, start, TMR6 );                             \
          mga_out32( mmio, xinc, TMR0 );                              \
          mga_out32( mmio, yinc, TMR1 );                              \
     } while (0)

#define MGA_T(start,xinc,yinc)                                        \
     do {                                                             \
          mga_out32( mmio, start, TMR7 );                             \
          mga_out32( mmio, xinc, TMR2 );                              \
          mga_out32( mmio, yinc, TMR3 );                              \
     } while (0)

#define MGA_Q(start,xinc,yinc)                                        \
     do {                                                             \
          mga_out32( mmio, start, TMR8 );                             \
          mga_out32( mmio, xinc, TMR4 );                              \
          mga_out32( mmio, yinc, TMR5 );                              \
     } while (0)


#define MGA_LSLOPE(dx,dy,sgn,err)                                     \
     do {                                                             \
          mga_out32( mmio, mgaF1800(dy), AR0 );                       \
          if ((dx) >= 0) {                                            \
               mga_out32( mmio, mgaF2400(-(dx)+(err)), AR1 );         \
               mga_out32( mmio, mgaF1800(-(dx)), AR2 );               \
               sgn &= ~SDXL;                                          \
          }                                                           \
          else {                                                      \
               mga_out32( mmio, mgaF2400((dx)+(dy)-(err)-1), AR1 );   \
               mga_out32( mmio, mgaF1800(dx), AR2 );                  \
               sgn |= SDXL;                                           \
          }                                                           \
     } while(0)


#define MGA_G400_LSLOPE(dx,dy,sgn,err)                                \
     do {                                                             \
          mga_out32( mmio, mgaF2200(dy), AR0 );                       \
          if ((dx) >= 0) {                                            \
               mga_out32( mmio, mgaF2400(-(dx)+(err)), AR1 );         \
               mga_out32( mmio, mgaF2200(-(dx)), AR2);                \
               sgn &= ~SDXL;                                          \
          }                                                           \
          else {                                                      \
               mga_out32( mmio, mgaF2400((dx)+(dy)-(err)-1), AR1 );   \
               mga_out32( mmio, mgaF2200(dx), AR2 );                  \
               sgn |= SDXL;                                           \
          }                                                           \
     } while(0)


#define MGA_RSLOPE(dx,dy,sgn,err)                                     \
     do {                                                             \
          mga_out32( mmio, mgaF1800(dy), AR6);                        \
          if ((dx) >= 0) {                                            \
               mga_out32( mmio, mgaF1800(-(dx)+(err)), AR4 );         \
               mga_out32( mmio, mgaF1800(-(dx)), AR5 );               \
               sgn &= ~SDXR;                                          \
          }                                                           \
          else {                                                      \
               mga_out32( mmio, mgaF1800((dx)+(dy)-(err)-1), AR4 );   \
               mga_out32( mmio, mgaF1800(dx), AR5 );                  \
               sgn |= SDXR;                                           \
          }                                                           \
     } while(0)


#define MGA_G400_RSLOPE(dx,dy,sgn,err)                                \
     do {                                                             \
          mga_out32( mmio, mgaF2200(dy), AR6 );                       \
          if ((dx) >= 0) {                                            \
               mga_out32( mmio, mgaF2200(-(dx)+(err)), AR4 );         \
               mga_out32( mmio, mgaF2200(-(dx)), AR5 );               \
               sgn &= ~SDXR;                                          \
          }                                                           \
          else {                                                      \
               mga_out32( mmio, mgaF2200((dx)+(dy)-(err)-1), AR4 );   \
               mga_out32( mmio, mgaF2200(dx), AR5);                   \
               sgn |= SDXR;                                           \
          }                                                           \
     } while(0)


typedef struct {
     DFBVertex *v0, *v1;  /* Y(v0) < Y(v1) */
     float      dx;          /* X(v1) - X(v0) */
     float      dy;          /* Y(v1) - Y(v0) */
     float      dxOOA;          /* dx * oneOverArea */
     float      dyOOA;          /* dy * oneOverArea */

     float      adjx,adjy; /* subpixel offset after rounding to integer */
     int        err;        /* error term ready for hardware */
     int        idx,idy;    /* delta-x & delta-y ready for hardware */
     int        sx,sy;          /* first sample point x,y coord */
     int        lines;      /* number of lines to be sampled on this edge */
} EdgeT;


static void
texture_triangle( MatroxDriverData *mdrv, MatroxDeviceData *mdev,
                  DFBVertex *v0, DFBVertex *v1, DFBVertex *v2 )
{
     EdgeT       eMaj, eTop, eBot;
     float       oneOverArea;
     DFBVertex  *vMin, *vMid, *vMax; /* Y(vMin)<=Y(vMid)<=Y(vMax) */
     int         Shape; /* 1 = Top half, 2 = bottom half, 3 = top+bottom */
//     float       bf = mga_bf_sign;

     volatile __u8 *mmio = mdrv->mmio_base;

/* find the order of the 3 vertices along the Y axis */
     {
          float y0 = v0->y;
          float y1 = v1->y;
          float y2 = v2->y;

          if (y0<=y1) {
               if (y1<=y2) {
                    vMin = v0;   vMid = v1;   vMax = v2;   /* y0<=y1<=y2 */
               }
               else if (y2<=y0) {
                    vMin = v2;   vMid = v0;   vMax = v1;   /* y2<=y0<=y1 */
               }
               else {
                    vMin = v0;   vMid = v2;   vMax = v1; /*bf = -bf;*/  /* y0<=y2<=y1 */
               }
          }
          else {
               if (y0<=y2) {
                    vMin = v1;   vMid = v0;   vMax = v2; /*bf = -bf;*/  /* y1<=y0<=y2 */
               }
               else if (y2<=y1) {
                    vMin = v2;   vMid = v1;   vMax = v0; /*bf = -bf;*/  /* y2<=y1<=y0 */
               }
               else {
                    vMin = v1;   vMid = v2;   vMax = v0;   /* y1<=y2<=y0 */
               }
          }
     }

/* vertex/edge relationship */
     eMaj.v0 = vMin;   eMaj.v1 = vMax;
     eTop.v0 = vMin;   eTop.v1 = vMid;
     eBot.v0 = vMid;   eBot.v1 = vMax;

/* compute deltas for each edge:  vertex[v1] - vertex[v0] */
     eMaj.dx = vMax->x - vMin->x;
     eMaj.dy = vMax->y - vMin->y;
     eTop.dx = vMid->x - vMin->x;
     eTop.dy = vMid->y - vMin->y;
     eBot.dx = vMax->x - vMid->x;
     eBot.dy = vMax->y - vMid->y;


/* compute oneOverArea */
     {
          float area = eMaj.dx * eBot.dy - eBot.dx * eMaj.dy;

          /* Do backface culling
           */
          //if ( area * bf < 0 || area == 0 )
          //return;

          oneOverArea = 1.0F / area;
     }

/* Edge setup.  For a triangle strip these could be reused... */
     {

#define DELTASCALE 16 /* Scaling factor for idx and idy. Note that idx and
                         idy are 18 bits signed, so don't choose too big
                         value. */

          int   ivMax_y;
          float temp;

          ivMax_y = CEIL(vMax->y);
          eTop.sy = eMaj.sy = CEIL(vMin->y);
          eBot.sy = CEIL(vMid->y);

          eMaj.lines = ivMax_y - eMaj.sy;
          if (eMaj.lines > 0) {
               float dxdy = eMaj.dx / eMaj.dy;
               eMaj.adjy = (float) eMaj.sy - vMin->y;
               temp = vMin->x + eMaj.adjy*dxdy;
               eMaj.sx = CEIL(temp);
               eMaj.adjx = (float) eMaj.sx - vMin->x;
               if (eMaj.lines == 1) {
                    eMaj.idy = 1;
                    eMaj.idx = 0;
                    eMaj.err = 0;
               }
               else {
                    eMaj.idy = RINT(eMaj.dy * DELTASCALE);
                    eMaj.idx = FLOOR(eMaj.idy * dxdy);
                    eMaj.err = RINT(((float) eMaj.sx - temp) * (float)eMaj.idy);
               }
          }
          else {
               return; /* CULLED */
          }

          Shape = 3;

          eBot.lines = ivMax_y - eBot.sy;
          if (eBot.lines > 0) {
               float dxdy = eBot.dx / eBot.dy;
               eBot.adjy = (float) eBot.sy - vMid->y;
               temp = vMid->x + eBot.adjy*dxdy;
               eBot.sx = CEIL(temp);
               eBot.adjx = (float) eBot.sx - vMid->x;
               if (eBot.lines == 1) {
                    eBot.idy = 1;
                    eBot.idx = 0;
                    eBot.err = 0;
               }
               else {
                    eBot.idy = RINT(eBot.dy * DELTASCALE);
                    eBot.idx = FLOOR(eBot.idy * dxdy);
                    eBot.err = RINT(((float) eBot.sx - temp) * (float)eBot.idy);
               }
          }
          else {
               Shape = 1;
          }

          eTop.lines = eBot.sy - eTop.sy;
          if (eTop.lines > 0) {
               float dxdy = eTop.dx / eTop.dy;
               eTop.adjy = eMaj.adjy;
               temp = vMin->x + eTop.adjy*dxdy;
               eTop.sx = CEIL(temp);
               eTop.adjx = (float) eTop.sx - vMin->x;
               if (eTop.lines == 1) {
                    eTop.idy = 1;
                    if (eBot.lines > 0) {
                         eTop.idx = eBot.sx - eTop.sx; /* needed for bottom half */
                    }
                    else {
                         eTop.idx = 0;
                    }
                    eTop.err = 0;
               }
               else {
                    eTop.idy = RINT(eTop.dy * DELTASCALE);
                    eTop.idx = FLOOR(eTop.idy * dxdy);
                    eTop.err = RINT(((float) eTop.sx - temp) * (float)eTop.idy);
               }
          }
          else {
               Shape = 2;
          }
     }

     {
          int ltor;        /* true if scanning left-to-right */
          EdgeT *eLeft, *eRight;
          int lines;
          DFBVertex *vTL;      /* Top left vertex */
          float adjx, adjy;

          /*
           * Execute user-supplied setup code
           */
#ifdef SETUP_CODE
          SETUP_CODE
#endif

          ltor = (oneOverArea > 0.0F);

          if (Shape == 2) {
               /* bottom half triangle */
               if (ltor) {
                    eLeft = &eMaj;
                    eRight = &eBot;
               }
               else {
                    eLeft = &eBot;
                    eRight = &eMaj;
               }
               lines = eBot.lines;
          }
          else {
               /* top half triangle */
               if (ltor) {
                    eLeft = &eMaj;
                    eRight = &eTop;
               }
               else {
                    eLeft = &eTop;
                    eRight = &eMaj;
               }
               lines = eTop.lines;
          }

          vTL = eLeft->v0;
          adjx = eLeft->adjx; adjy = eLeft->adjy;


          /* setup derivatives */
/* compute d?/dx and d?/dy derivatives */
          eBot.dxOOA = eBot.dx * oneOverArea;
          eBot.dyOOA = eBot.dy * oneOverArea;
          eMaj.dxOOA = eMaj.dx * oneOverArea;
          eMaj.dyOOA = eMaj.dy * oneOverArea;

#define DERIV( DZ, COMP) \
  { \
    float eMaj_DZ, eBot_DZ; \
    eMaj_DZ = vMax->COMP - vMin->COMP; \
    eBot_DZ = vMax->COMP - vMid->COMP; \
    DZ ## dx = eMaj_DZ * eBot.dyOOA - eMaj.dyOOA * eBot_DZ;    \
    DZ ## dy = eMaj.dxOOA * eBot_DZ - eMaj_DZ * eBot.dxOOA;    \
  }

          if (mdev->depth_buffer) {
               float Zstart;
               float dzdx, dzdy;

               DERIV(dz, z);

               if (dzdx>65535.0f*(1<<15) || dzdx<-65535.0f*(1<<15)) {
                    /* probably a sliver triangle */
                    dzdx = 0.0;
                    dzdy = 0.0;
               }

               Zstart = vTL->z + dzdx*adjx + dzdy*adjy;

               /* FIXME: 16 bit assumed */
               if (Zstart > 65535.0f*(1 << 15)) {
                    Zstart = 65535.0f*(1 << 15);
                    dzdx = 0.0F;
                    dzdy = 0.0F;
               }

               mga_waitfifo( mdrv, mdev, 3 );

               mga_out32( mmio, RINT(Zstart), DR0 );
               mga_out32( mmio, RINT(dzdx), DR2 );
               mga_out32( mmio, RINT(dzdy), DR3 );
          }

          {
               float dsdx, dsdy;
               float dtdx, dtdy;
               float dvdx, dvdy;

               mga_waitfifo( mdrv, mdev, 9 );

               DERIV(ds,s);

               MGA_S(RINT( (vTL->s+dsdx*adjx+dsdy*adjy) ),
                     RINT( dsdx ), RINT( dsdy ));

               DERIV(dt,t);

               MGA_T(RINT( (vTL->t+dtdx*adjx+dtdy*adjy) ),
                     RINT( dtdx ), RINT( dtdy ));

               DERIV(dv,w);
               {
                    int sq = RINT( (vTL->w+dvdx*adjx+dvdy*adjy) );
                    MGA_Q((sq == 0) ? 1 : sq,RINT(dvdx),RINT(dvdy));
               }
          }

          {
               __u32 sgn = 0;

               mga_waitfifo( mdrv, mdev, 9 );

               /* Draw part #1 */
               if (mdrv->accelerator == FB_ACCEL_MATROX_MGAG400) {
                    MGA_G400_LSLOPE(eLeft->idx,eLeft->idy,sgn,eLeft->err);
                    MGA_G400_RSLOPE(eRight->idx,eRight->idy,sgn,eRight->err);
               }
               else {
                    MGA_LSLOPE(eLeft->idx,eLeft->idy,sgn,eLeft->err);
                    MGA_RSLOPE(eRight->idx,eRight->idy,sgn,eRight->err);
               }

               mga_out32( mmio, sgn,                            SGN );
               mga_out32( mmio, ((__u32)(eLeft->sx) & 0xFFFF) | ((__u32)(eRight->sx) << 16), FXBNDRY );
               mga_out32( mmio, lines     | ((__u32)(eLeft->sy)  << 16), YDSTLEN | EXECUTE );

               if (Shape != 3) { /* has only one half? */
                    return;
               }

               mga_waitfifo( mdrv, mdev, 6 );

               /* Draw part #2 */
               if (ltor) {
                    if (mdrv->accelerator == FB_ACCEL_MATROX_MGAG400)
                         MGA_G400_RSLOPE(eBot.idx,eBot.idy,sgn,eBot.err);
                    else
                         MGA_RSLOPE(eBot.idx,eBot.idy,sgn,eBot.err);

                    mga_out32( mmio, eBot.sx, FXRIGHT );
               }
               else {
                    sgn |= SGN_BRKLEFT;
                    mga_out32( mmio, eBot.sx, FXLEFT );
                    if (mdrv->accelerator == FB_ACCEL_MATROX_MGAG400)
                         MGA_G400_LSLOPE(eBot.idx,eBot.idy,sgn,eBot.err);
                    else
                         MGA_LSLOPE(eBot.idx,eBot.idy,sgn,eBot.err);

               }

               mga_out32( mmio, sgn,        SGN );
               mga_out32( mmio, eBot.lines, LEN | EXECUTE );
          }
     }
}

#define INVWMAX 128.0F

bool
matroxTextureTriangles( void *drv, void *dev,
                        DFBVertex *vertices, int num,
                        DFBTriangleFormation formation )
{
     int               i;
     MatroxDriverData *mdrv = (MatroxDriverData*) drv;
     MatroxDeviceData *mdev = (MatroxDeviceData*) dev;
     volatile __u8    *mmio = mdrv->mmio_base;
     __u32             dwgctl;

     float wScale;

#if 0
     float InvWScale = 1.0f;
     float nearVal   = 1.0f;

     if (nearVal > 0) {
          /* limit InvWScale/wMin in (0,INVWMAX] to avoid over- and underflow.
             InvWScale is used by texture setup in mga_tritemp.h */
          int exp2;

          if (frexp(INVWMAX * nearVal,&exp2) != 0) {
               if (exp2 >= 2) {
                    InvWScale = 1 << (exp2-1);
               }
               else if (exp2 <= 0) {
                    InvWScale = 1.0 / (1 << (-exp2+1));
               }
          }
     }
#else
#define InvWScale 128.0f
#endif

     wScale = InvWScale * (float) (1 << 20);

     for (i=0; i<num; i++) {
          DFBVertex *v = &vertices[i];

          v->x -= 0.5f;
          v->y -= 0.5f;
          v->z *= (float) (1 << 15) * 65535.0f;
          v->w *= wScale;

          v->s *= v->w * (float) mdev->w / (float) (1 << mdev->w2);
          v->t *= v->w * (float) mdev->h / (float) (1 << mdev->h2);
     }

     if (mdev->depth_buffer)
          dwgctl = ATYPE_ZI | ZMODE_ZLTE;
     else
          dwgctl = ATYPE_I  | ZMODE_NOZCMP;

     mga_waitfifo( mdrv, mdev, 2 );

     mga_out32( mmio, dwgctl | BOP_COPY | SHFTZERO | OP_TEXTURE_TRAP, DWGCTL );
     mga_out32( mmio, (0x10<<21) | MAG_BILIN | MIN_ANISO | FILTER_ALPHA, TEXFILTER );

     switch (formation) {
          case DTTF_LIST:
               for (i=0; i<num; i+=3)
                    texture_triangle( mdrv, mdev, &vertices[i], &vertices[i+1], &vertices[i+2] );

               break;

          case DTTF_STRIP:
               texture_triangle( mdrv, mdev, &vertices[0], &vertices[1], &vertices[2] );

               for (i=3; i<num; i++)
                    texture_triangle( mdrv, mdev, &vertices[i-2], &vertices[i-1], &vertices[i] );

               break;

          case DTTF_FAN:
               texture_triangle( mdrv, mdev, &vertices[0], &vertices[1], &vertices[2] );

               for (i=3; i<num; i++)
                    texture_triangle( mdrv, mdev, &vertices[0], &vertices[i-1], &vertices[i] );

               break;

          default:
               D_ONCE( "unknown formation" );
               return false;
     }

     mga_waitfifo( mdrv, mdev, 5 );
     mga_out32( mmio, 0, TMR1 );
     mga_out32( mmio, 0, TMR2 );
     mga_out32( mmio, 0, TMR4 );
     mga_out32( mmio, 0, TMR5 );
     mga_out32( mmio, 0x100000, TMR8 );

     return true;
}

