/*
   Copyright (c) 2003 Andreas Robinson, All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

// Hardware mapping functions ------------------------------------------------

#include "uc_hw.h"
#include <gfx/convert.h>

/// Map DirectFB blending functions to hardware
inline void
uc_map_blending_fn( struct uc_hw_alpha      *hwalpha,
                    DFBSurfaceBlendFunction  sblend,
                    DFBSurfaceBlendFunction  dblend,
                    DFBSurfacePixelFormat    dst_format )
{
     bool dst_alpha = DFB_PIXELFORMAT_HAS_ALPHA(dst_format);

     // The HW's blending equation is:
     // (Ca * FCa + Cbias + Cb * FCb) << Cshift

     // Set source blending function

     // Ca  -- always from source color.
     hwalpha->regHABLCsat = HC_HABLCsat_MASK | HC_HABLCa_OPC | HC_HABLCa_Csrc;
     // Aa  -- always from source alpha.
     hwalpha->regHABLAsat = HC_HABLAsat_MASK | HC_HABLAa_OPA | HC_HABLAa_Asrc;

     // FCa and FAa depend on the following condition.
     switch (sblend) {
          case DSBF_ZERO:
               // GL_ZERO -- (0, 0, 0, 0)
               hwalpha->regHABLCsat |= HC_HABLFCa_OPC | HC_HABLFCa_HABLRCa;
               hwalpha->regHABLAsat |= HC_HABLFAa_OPA | HC_HABLFAa_HABLFRA;
               hwalpha->regHABLRFCa = 0x0;
               hwalpha->regHABLRAa = 0x0;
               break;

          case DSBF_ONE:
               // GL_ONE -- (1, 1, 1, 1)
               hwalpha->regHABLCsat |= HC_HABLFCa_InvOPC | HC_HABLFCa_HABLRCa;
               hwalpha->regHABLAsat |= HC_HABLFAa_InvOPA | HC_HABLFAa_HABLFRA;
               hwalpha->regHABLRFCa = 0x0;
               hwalpha->regHABLRAa = 0x0;
               break;

          case DSBF_SRCCOLOR:
               // GL_SRC_COLOR -- (Rs, Gs, Bs, As)
               hwalpha->regHABLCsat |= HC_HABLFCa_OPC | HC_HABLFCa_Csrc;
               hwalpha->regHABLAsat |= HC_HABLFAa_OPA | HC_HABLFAa_Asrc;
               break;

          case DSBF_INVSRCCOLOR:
               // GL_ONE_MINUS_SRC_COLOR -- (1, 1, 1, 1) - (Rs, Gs, Bs, As)
               hwalpha->regHABLCsat |= HC_HABLFCa_InvOPC | HC_HABLFCa_Csrc;
               hwalpha->regHABLAsat |= HC_HABLFAa_InvOPA | HC_HABLFAa_Asrc;
               break;

          case DSBF_SRCALPHA:
               // GL_SRC_ALPHA -- (As, As, As, As)
               hwalpha->regHABLCsat |= HC_HABLFCa_OPC | HC_HABLFCa_Asrc;
               hwalpha->regHABLAsat |= HC_HABLFAa_OPA | HC_HABLFAa_Asrc;
               break;

          case DSBF_INVSRCALPHA:
               // GL_ONE_MINUS_SRC_ALPHA -- (1, 1, 1, 1) - (As, As, As, As)
               hwalpha->regHABLCsat |= HC_HABLFCa_InvOPC | HC_HABLFCa_Asrc;
               hwalpha->regHABLAsat |= HC_HABLFAa_InvOPA | HC_HABLFAa_Asrc;
               break;

          case DSBF_DESTALPHA:
               // GL_DST_ALPHA
               if (!dst_alpha) { // (1, 1, 1, 1)
                    hwalpha->regHABLCsat |= HC_HABLFCa_InvOPC | HC_HABLFCa_HABLRCa;
                    hwalpha->regHABLAsat |= HC_HABLFAa_InvOPA | HC_HABLFAa_HABLFRA;
                    hwalpha->regHABLRFCa = 0x0;
                    hwalpha->regHABLRAa = 0x0;
               }
               else { // (Ad, Ad, Ad, Ad)
                    hwalpha->regHABLCsat |= HC_HABLFCa_OPC | HC_HABLFCa_Adst;
                    hwalpha->regHABLAsat |= HC_HABLFAa_OPA | HC_HABLFAa_Adst;
               }
               break;

          case DSBF_INVDESTALPHA:
               // GL_ONE_MINUS_DST_ALPHA
               if (!dst_alpha) { // (1, 1, 1, 1) - (1, 1, 1, 1) = (0, 0, 0, 0)
                    hwalpha->regHABLCsat |= HC_HABLFCa_OPC | HC_HABLFCa_HABLRCa;
                    hwalpha->regHABLAsat |= HC_HABLFAa_OPA | HC_HABLFAa_HABLFRA;
                    hwalpha->regHABLRFCa = 0x0;
                    hwalpha->regHABLRAa = 0x0;
               }
               else { // (1, 1, 1, 1) - (Ad, Ad, Ad, Ad)
                    hwalpha->regHABLCsat |= HC_HABLFCa_InvOPC | HC_HABLFCa_Adst;
                    hwalpha->regHABLAsat |= HC_HABLFAa_InvOPA | HC_HABLFAa_Adst;
               }
               break;

          case DSBF_DESTCOLOR:
               // GL_DST_COLOR -- (Rd, Gd, Bd, Ad)
               hwalpha->regHABLCsat |= HC_HABLFCa_OPC | HC_HABLFCa_Cdst;
               hwalpha->regHABLAsat |= HC_HABLFAa_OPA | HC_HABLFAa_Adst;
               break;

          case DSBF_INVDESTCOLOR:
               // GL_ONE_MINUS_DST_COLOR -- (1, 1, 1, 1) - (Rd, Gd, Bd, Ad)
               hwalpha->regHABLCsat |= HC_HABLFCa_InvOPC | HC_HABLFCa_Cdst;
               hwalpha->regHABLAsat |= HC_HABLFAa_InvOPA | HC_HABLFAa_Adst;
               break;

          case DSBF_SRCALPHASAT:
               // GL_SRC_ALPHA_SATURATE
               if (!dst_alpha) {
                    // (f, f, f, 1), f = min(As, 1 - Ad) = min(As, 1 - 1) = 0
                    // So (f, f, f, 1) = (0, 0, 0, 1)
                    hwalpha->regHABLCsat |= HC_HABLFCa_OPC | HC_HABLFCa_HABLRCa;
                    hwalpha->regHABLAsat |= HC_HABLFAa_InvOPA | HC_HABLFAa_HABLFRA;
                    hwalpha->regHABLRFCa = 0x0;
                    hwalpha->regHABLRAa = 0x0;
               }
               else {
                    // (f, f, f, 1), f = min(As, 1 - Ad)
                    hwalpha->regHABLCsat |= HC_HABLFCa_OPC | HC_HABLFCa_mimAsrcInvAdst;
                    hwalpha->regHABLAsat |= HC_HABLFAa_InvOPA | HC_HABLFAa_HABLFRA;
                    hwalpha->regHABLRFCa = 0x0;
                    hwalpha->regHABLRAa = 0x0;
               }
               break;
     }

     // Set destination blending function

     // Op is add.
     // bias is 0.

     hwalpha->regHABLCsat |= HC_HABLCbias_HABLRCbias;
     hwalpha->regHABLAsat |= HC_HABLAbias_HABLRAbias;

     // Cb  -- always from destination color.
     hwalpha->regHABLCop = HC_HABLCb_OPC | HC_HABLCb_Cdst;
     // Ab  -- always from destination alpha.
     hwalpha->regHABLAop = HC_HABLAb_OPA | HC_HABLAb_Adst;

     // FCb -- depends on the following condition.
     switch (dblend) {
          case DSBF_ZERO:
               // GL_ZERO -- (0, 0, 0, 0)
               hwalpha->regHABLCop |= HC_HABLFCb_OPC | HC_HABLFCb_HABLRCb;
               hwalpha->regHABLAop |= HC_HABLFAb_OPA | HC_HABLFAb_HABLFRA;
               hwalpha->regHABLRFCb = 0x0;
               hwalpha->regHABLRAb = 0x0;
               break;

          case DSBF_ONE:
               // GL_ONE -- (1, 1, 1, 1)
               hwalpha->regHABLCop |= HC_HABLFCb_InvOPC | HC_HABLFCb_HABLRCb;
               hwalpha->regHABLAop |= HC_HABLFAb_InvOPA | HC_HABLFAb_HABLFRA;
               hwalpha->regHABLRFCb = 0x0;
               hwalpha->regHABLRAb = 0x0;
               break;

          case DSBF_SRCCOLOR:
               // GL_SRC_COLOR -- (Rs, Gs, Bs, As)
               hwalpha->regHABLCop |= HC_HABLFCb_OPC | HC_HABLFCb_Csrc;
               hwalpha->regHABLAop |= HC_HABLFAb_OPA | HC_HABLFAb_Asrc;
               break;

          case DSBF_INVSRCCOLOR:
               // GL_ONE_MINUS_SRC_COLOR -- (1, 1, 1, 1) - (Rs, Gs, Bs, As)
               hwalpha->regHABLCop |= HC_HABLFCb_InvOPC | HC_HABLFCb_Csrc;
               hwalpha->regHABLAop |= HC_HABLFAb_InvOPA | HC_HABLFAb_Asrc;
               break;

          case DSBF_SRCALPHA:
               // GL_SRC_ALPHA -- (As, As, As, As)
               hwalpha->regHABLCop |= HC_HABLFCb_OPC | HC_HABLFCb_Asrc;
               hwalpha->regHABLAop |= HC_HABLFAb_OPA | HC_HABLFAb_Asrc;
               break;

          case DSBF_INVSRCALPHA:
               // GL_ONE_MINUS_SRC_ALPHA -- (1, 1, 1, 1) - (As, As, As, As)
               hwalpha->regHABLCop |= HC_HABLFCb_InvOPC | HC_HABLFCb_Asrc;
               hwalpha->regHABLAop |= HC_HABLFAb_InvOPA | HC_HABLFAb_0;
               break;

          case DSBF_DESTALPHA:
               // GL_DST_ALPHA
               if (!dst_alpha) { // (1, 1, 1, 1)
                    hwalpha->regHABLCop |= HC_HABLFCb_InvOPC | HC_HABLFCb_HABLRCb;
                    hwalpha->regHABLAop |= HC_HABLFAb_InvOPA | HC_HABLFAb_HABLFRA;
                    hwalpha->regHABLRFCb = 0x0;
                    hwalpha->regHABLRAb = 0x0;
               }
               else { // (Ad, Ad, Ad, Ad)
                    hwalpha->regHABLCop |= HC_HABLFCb_OPC | HC_HABLFCb_Adst;
                    hwalpha->regHABLAop |= HC_HABLFAb_OPA | HC_HABLFAb_Adst;
               }
               break;

          case DSBF_INVDESTALPHA:
               // GL_ONE_MINUS_DST_ALPHA
               if (!dst_alpha) { // (1, 1, 1, 1) - (1, 1, 1, 1) = (0, 0, 0, 0)
                    hwalpha->regHABLCop |= HC_HABLFCb_OPC | HC_HABLFCb_HABLRCb;
                    hwalpha->regHABLAop |= HC_HABLFAb_OPA | HC_HABLFAb_HABLFRA;
                    hwalpha->regHABLRFCb = 0x0;
                    hwalpha->regHABLRAb = 0x0;
               }
               else { // (1, 1, 1, 1) - (Ad, Ad, Ad, Ad)
                    hwalpha->regHABLCop |= HC_HABLFCb_InvOPC | HC_HABLFCb_Adst;
                    hwalpha->regHABLAop |= HC_HABLFAb_InvOPA | HC_HABLFAb_Adst;
               }
               break;

          case DSBF_DESTCOLOR:
               // GL_DST_COLOR -- (Rd, Gd, Bd, Ad)
               hwalpha->regHABLCop |= HC_HABLFCb_OPC | HC_HABLFCb_Cdst;
               hwalpha->regHABLAop |= HC_HABLFAb_OPA | HC_HABLFAb_Adst;
               break;

          case DSBF_INVDESTCOLOR:
               // GL_ONE_MINUS_DST_COLOR -- (1, 1, 1, 1) - (Rd, Gd, Bd, Ad)
               hwalpha->regHABLCop |= HC_HABLFCb_InvOPC | HC_HABLFCb_Cdst;
               hwalpha->regHABLAop |= HC_HABLFAb_InvOPA | HC_HABLFAb_Adst;
               break;

          case DSBF_SRCALPHASAT:
               // Unsupported?

          default:
               hwalpha->regHABLCop |= HC_HABLFCb_OPC | HC_HABLFCb_HABLRCb;
               hwalpha->regHABLAop |= HC_HABLFAb_OPA | HC_HABLFAb_HABLFRA;
               hwalpha->regHABLRFCb = 0x0;
               hwalpha->regHABLRAb = 0x0;
               break;
     }
}

/// Map DFBSurfaceBlittingFlags to the hardware
inline void
uc_map_blitflags( struct uc_hw_texture    *tex,
                  DFBSurfaceBlittingFlags  bflags,
                  DFBSurfacePixelFormat    sformat )
{
     bool gotalpha = DFB_PIXELFORMAT_HAS_ALPHA(sformat);

     if (bflags & DSBLIT_COLORIZE) {
          // Cv0 = Ct*Cf

          // Hw setting:
          // Ca = Ct, Cb = Cf, Cop = +, Cc = 0, Cbias = 0, Cshift = No.

          tex->regHTXnTBLCsat_0 = HC_HTXnTBLCsat_MASK |
                                  HC_HTXnTBLCa_TOPC | HC_HTXnTBLCa_Tex |
                                  HC_HTXnTBLCb_TOPC | HC_HTXnTBLCb_Dif |
                                  HC_HTXnTBLCc_TOPC | HC_HTXnTBLCc_0;
          tex->regHTXnTBLCop_0 = HC_HTXnTBLCop_Add |
                                 HC_HTXnTBLCbias_Cbias | HC_HTXnTBLCbias_0 |
                                 HC_HTXnTBLCshift_No;
          tex->regHTXnTBLMPfog_0 = HC_HTXnTBLMPfog_0;
     }
     else {
          // Cv0 = Ct

          // Hw setting:
          // Ca = 0, Cb = 0, Cop = +, Cc = 0, Cbias = Ct, Cshift = No.

          tex->regHTXnTBLCsat_0 = HC_HTXnTBLCsat_MASK |
                                  HC_HTXnTBLCa_TOPC | HC_HTXnTBLCa_0 |
                                  HC_HTXnTBLCb_TOPC | HC_HTXnTBLCb_0 |
                                  HC_HTXnTBLCc_TOPC | HC_HTXnTBLCc_0;
          tex->regHTXnTBLCop_0 = HC_HTXnTBLCop_Add |
                                 HC_HTXnTBLCbias_Cbias | HC_HTXnTBLCbias_Tex |
                                 HC_HTXnTBLCshift_No;
          tex->regHTXnTBLMPfog_0 = HC_HTXnTBLMPfog_0;
     }

     if (bflags & DSBLIT_BLEND_COLORALPHA) {
          if ((bflags & DSBLIT_BLEND_ALPHACHANNEL) && gotalpha) {
               // Av0 = At*Af

               // Hw setting:
               // Aa = At, Ab = Af, Cop = +, Ac = 0, Abias = 0, Ashift = No.

               tex->regHTXnTBLAsat_0 = HC_HTXnTBLAsat_MASK |
                                       HC_HTXnTBLAa_TOPA | HC_HTXnTBLAa_Atex |
                                       HC_HTXnTBLAb_TOPA | HC_HTXnTBLAb_Adif |
                                       HC_HTXnTBLAc_TOPA | HC_HTXnTBLAc_HTXnTBLRA;
               tex->regHTXnTBLCop_0 |= HC_HTXnTBLAop_Add |
                                       HC_HTXnTBLAbias_HTXnTBLRAbias | HC_HTXnTBLAshift_No;
               tex->regHTXnTBLRAa_0 = 0x0;
               tex->regHTXnTBLRFog_0 = 0x0;
          }
          else {
               // (!(bflags & DSBLIT_BLEND_ALPHACHANNEL) && gotalpha) || !gotalpha
               // Av0 = Af

               // Hw setting:
               // Aa = 0, Ab = 0, Cop = +, Ac = 0, Abias = Af, Ashift = No.

               tex->regHTXnTBLAsat_0 = HC_HTXnTBLAsat_MASK |
                                       HC_HTXnTBLAa_TOPA | HC_HTXnTBLAa_HTXnTBLRA |
                                       HC_HTXnTBLAb_TOPA | HC_HTXnTBLAb_HTXnTBLRA |
                                       HC_HTXnTBLAc_TOPA | HC_HTXnTBLAc_HTXnTBLRA;
               tex->regHTXnTBLCop_0 |= HC_HTXnTBLAop_Add |
                                       HC_HTXnTBLAbias_Adif | HC_HTXnTBLAshift_No;
               tex->regHTXnTBLRAa_0 = 0x0;
               tex->regHTXnTBLRFog_0 = 0x0;
          }
     }
     else {  // !(bflags & DSBLIT_BLEND_COLORALPHA)
          if ((bflags & DSBLIT_BLEND_ALPHACHANNEL) && gotalpha) {
               // Av0 = At

               // Hw setting:
               // Aa = 0, Ab = 0, Cop = +, Ac = 0, Abias = At, Ashift = No.

               tex->regHTXnTBLAsat_0 = HC_HTXnTBLAsat_MASK |
                                       HC_HTXnTBLAa_TOPA | HC_HTXnTBLAa_HTXnTBLRA |
                                       HC_HTXnTBLAb_TOPA | HC_HTXnTBLAb_HTXnTBLRA |
                                       HC_HTXnTBLAc_TOPA | HC_HTXnTBLAc_HTXnTBLRA;
               tex->regHTXnTBLCop_0 |= HC_HTXnTBLAop_Add |
                                       HC_HTXnTBLAbias_Atex | HC_HTXnTBLAshift_No;
               tex->regHTXnTBLRAa_0 = 0x0;
               tex->regHTXnTBLRFog_0 = 0x0;
          }
          else { // !gotalpha
               // Av0 = 1.0

               // D_BUG warning: I'm guessing where values should go,
               // and how big (0xff = 1.0 ?) it should be.

               // Hw setting:
               // Aa = 1.0, Ab = 1.0, Cop = -, Ac = 1.0, Abias = 1.0, Ashift = No.
               // => Av = Aa*(Ab-Ac) + Abias = 1*(1-1)+1 = 1

               tex->regHTXnTBLAsat_0 = HC_HTXnTBLAsat_MASK |
                                       HC_HTXnTBLAa_TOPA | HC_HTXnTBLAa_HTXnTBLRA |
                                       HC_HTXnTBLAb_TOPA | HC_HTXnTBLAb_HTXnTBLRA |
                                       HC_HTXnTBLAc_TOPA | HC_HTXnTBLAc_HTXnTBLRA;
               tex->regHTXnTBLCop_0 |= HC_HTXnTBLAop_Add |
                                       HC_HTXnTBLAbias_Inv | HC_HTXnTBLAbias_HTXnTBLRAbias | HC_HTXnTBLAshift_No;
               tex->regHTXnTBLRAa_0 = 0x0;
               tex->regHTXnTBLRFog_0 = 0x0;
          }
     }
}

