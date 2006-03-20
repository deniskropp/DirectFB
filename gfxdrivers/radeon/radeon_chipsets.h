/*
 * Copyright (C) 2006 Claudio Ciccani <klan@users.sf.net>
 *
 * Graphics driver for ATI Radeon cards written by
 *             Claudio Ciccani <klan@users.sf.net>.  
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
 
#ifndef __RADEON_CHIPSETS_H__
#define __RADEON_CHIPSETS_H__

static const struct {
     __u16       id;
     __u16       chip;
     bool        igp;
     const char *name;
} dev_table[] = {
     { 0x5144, CHIP_R100 , false, "Radeon" },
     { 0x5145, CHIP_R100 , false, "Radeon" },
     { 0x5146, CHIP_R100 , false, "Radeon" },
     { 0x5147, CHIP_R100 , false, "Radeon" },
     { 0x5159, CHIP_RV100, false, "Radeon VE/7000" },
     { 0x515a, CHIP_RV100, false, "Radeon VE/7000" },
     { 0x4c59, CHIP_RV100, false, "Radeon Mobility M6" },
     { 0x4c5a, CHIP_RV100, false, "Radeon Mobility M6" },
     { 0x4c57, CHIP_RV200, false, "Radeon Mobility M7" },
     { 0x4c58, CHIP_RV200, false, "FireGL Mobility 7800 M7" },
     { 0x5157, CHIP_RV200, false, "Radeon 7500" },
     { 0x5158, CHIP_RV200, false, "Radeon 7500" },
     { 0x4136, CHIP_RS100, true , "Radeon IGP320" },
     { 0x4336, CHIP_RS100, true , "Radeon IGP320M" },
     { 0x4137, CHIP_RS200, true , "Radeon IGP330/340/350" },
     { 0x4337, CHIP_RS200, true , "Radeon IGP330M/340M/350M" },
     { 0x4237, CHIP_RS250, true , "Radeon 7000 IGP" },
     { 0x4437, CHIP_RS250, true , "Radeon Mobility 7000 IGP" },
     { 0x514c, CHIP_R200 , false, "Radeon 8500" },
     { 0x4242, CHIP_R200 , false, "Radeon 8500 AIW" },
     { 0x4243, CHIP_R200 , false, "Radeon 8500 AIW" },
     { 0x514d, CHIP_R200 , false, "Radeon 9100" },
     { 0x5148, CHIP_R200 , false, "FireGL 8700/8800" },
     { 0x4966, CHIP_RV250, false, "Radeon 9000 PRO" },
     { 0x4967, CHIP_RV250, false, "Radeon 9000" },
     { 0x4c66, CHIP_RV250, false, "Radeon Mobility 9000 M9" },
     { 0x4c67, CHIP_RV250, false, "Radeon Mobility 9000 M9" },
     { 0x4c64, CHIP_RV250, false, "FireGL Mobility 9000 M9" },
     { 0x5960, CHIP_RV280, false, "Radeon 9200 PRO" },
     { 0x5961, CHIP_RV280, false, "Radeon 9200" },
     { 0x5962, CHIP_RV280, false, "Radeon 9200" },
     { 0x5964, CHIP_RV280, false, "Radeon 9200 SE" },
     { 0x5c61, CHIP_RV280, false, "Radeon Mobility 9200 M9+" },
     { 0x5c63, CHIP_RV280, false, "Radeon Mobility 9200 M9+" },
     { 0x5834, CHIP_RS300, true , "Radeon 9100 IGP" },
     { 0x5835, CHIP_RS300, true , "Radeon Mobility 9100 IGP" },
     { 0x7834, CHIP_RS350, true , "Radeon 9100 PRO IGP" },
     { 0x7835, CHIP_RS350, true , "Radeon Mobility 9200 IGP" }, 
     { 0x4144, CHIP_R300 , false, "Radeon 9500" },
     { 0x4145, CHIP_R300 , false, "Radeon 9500" },
     { 0x4146, CHIP_R300 , false, "Radeon 9600 TX" },
     { 0x4147, CHIP_R300 , false, "FireGL Z1" },
     { 0x4e44, CHIP_R300 , false, "Radeon 9700 PRO" },
     { 0x4e45, CHIP_R300 , false, "Radeon 9700/9500PRO" },
     { 0x4e46, CHIP_R300 , false, "Radeon 9600 TX" },
     { 0x4e47, CHIP_R300 , false, "FireGL X1" },
     { 0x4150, CHIP_RV350, false, "Radeon 9600" },
     { 0x4151, CHIP_RV350, false, "Radeon 9600 SE" },
     { 0x4152, CHIP_RV350, false, "Radeon 9600 XT" },
     { 0x4153, CHIP_RV350, false, "Radeon 9600" },
     { 0x4154, CHIP_RV350, false, "FireGL T2" },
     { 0x4156, CHIP_RV350, false, "FireGL RV360" },
     { 0x4e50, CHIP_RV350, false, "Radeon Mobility 9600/9700 M10/M11" },
     { 0x4e51, CHIP_RV350, false, "Radeon Mobility 9600 M10" },
     { 0x4e52, CHIP_RV350, false, "Radeon Mobility 9600 M11" },
     { 0x4e53, CHIP_RV350, false, "Radeon Mobility 9600 M10" },
     { 0x4e54, CHIP_RV350, false, "FireGL Mobility T2 M10" },
     { 0x4e56, CHIP_RV350, false, "FireGL Mobility T2e M11" },
     { 0x4155, CHIP_RV350, false, "Radeon 9650" },
     { 0x4148, CHIP_R350 , false, "Radeon 9800 SE" },
     { 0x4149, CHIP_R350 , false, "Radeon 9800" },
     { 0x414a, CHIP_R350 , false, "Radeon 9800" },
     { 0x414b, CHIP_R350 , false, "FireGL X2" },
     { 0x4e48, CHIP_R350 , false, "Radeon 9800 PRO" },
     { 0x4e49, CHIP_R350 , false, "Radeon 9800" },
     { 0x4e4b, CHIP_R350 , false, "FireGL X2" },
     { 0x4e4a, CHIP_R350 , false, "Radeon 9800 XT" },
     { 0x3e50, CHIP_RV380, false, "Radeon X600" },
     { 0x3e54, CHIP_RV380, false, "FireGL V3200" },
     { 0x3150, CHIP_RV380, false, "Radeon Mobility X600 M24" },
     { 0x3152, CHIP_RV380, false, "Radeon Mobility X300 M24" },
     { 0x3154, CHIP_RV380, false, "FireGL M24 GL" },
     { 0x5b60, CHIP_RV380, false, "Radeon X300" },
     { 0x5b62, CHIP_RV380, false, "Radeon X600" },
     { 0x5b63, CHIP_RV380, false, "Radeon X550" },
     { 0x5b64, CHIP_RV380, false, "FireGL V3100" },
     { 0x5b65, CHIP_RV380, false, "FireMV 2200 PCIE" },
     { 0x5460, CHIP_RV380, false, "Radeon Mobility X300 M22" },
     { 0x5462, CHIP_RV380, false, "Radeon Mobility X600 SE M24C" },
     { 0x5464, CHIP_RV380, false, "FireGL M22 GL" },
     { 0x5a41, CHIP_RS400, false, "Radeon XPRESS 200" },
     { 0x5a42, CHIP_RS400, false, "Radeon XPRESS 200M" },
     { 0x5a61, CHIP_RS400, false, "Radeon XPRESS 200" },
     { 0x5a62, CHIP_RS400, false, "Radeon XPRESS 200M" },
     { 0x5954, CHIP_RS400, false, "Radeon XPRESS 200" },
     { 0x5955, CHIP_RS400, false, "Radeon XPRESS 200M" },
     { 0x5974, CHIP_RS400, false, "Radeon XPRESS 200" },
     { 0x5975, CHIP_RS400, false, "Radeon XPRESS 200M" },
     { 0x5e48, CHIP_RV410, false, "FireGL V5000" },
     { 0x564a, CHIP_RV410, false, "Mobility FireGL V5000 M26" },
     { 0x564b, CHIP_RV410, false, "Mobility FireGL V5000 M26" },
     { 0x564f, CHIP_RV410, false, "Mobility Radeon X700 XL M26" },
     { 0x5652, CHIP_RV410, false, "Mobility Radeon X700 M26" },
     { 0x5653, CHIP_RV410, false, "Mobility Radeon X700 M26" },
     { 0x5e4b, CHIP_RV410, false, "Radeon X700 PRO" },
     { 0x5e4a, CHIP_RV410, false, "Radeon X700 XT" },
     { 0x5e4d, CHIP_RV410, false, "Radeon X700" },
     { 0x5e4c, CHIP_RV410, false, "Radeon X700 SE" },
     { 0x5e4f, CHIP_RV410, false, "Radeon X700 SE" },
     { 0x4a48, CHIP_R420 , false, "Radeon X800" },
     { 0x4a49, CHIP_R420 , false, "Radeon X800 PRO" },
     { 0x4a4a, CHIP_R420 , false, "Radeon X800 SE" },
     { 0x4a4b, CHIP_R420 , false, "Radeon X800" },
     { 0x4a4c, CHIP_R420 , false, "Radeon X800" },
     { 0x4a4d, CHIP_R420 , false, "FireGL X3" },
     { 0x4a4e, CHIP_R420 , false, "Radeon Mobility 9800 M18" },
     { 0x4a50, CHIP_R420 , false, "Radeon X800 XT" },
     { 0x4a4f, CHIP_R420 , false, "Radeon X800 SE" },
     { 0x4a54, CHIP_R420 , false, "Radeon AIW X800" },
     { 0x5548, CHIP_R420 , false, "Radeon X800" },
     { 0x5549, CHIP_R420 , false, "Radeon X800 PRO" },
     { 0x554a, CHIP_R420 , false, "Radeon X800 LE" },
     { 0x554b, CHIP_R420 , false, "Radeon X800 SE" },
     { 0x5551, CHIP_R420 , false, "FireGL V5100" },
     { 0x5552, CHIP_R420 , false, "FireGL Unknown" },
     { 0x5554, CHIP_R420 , false, "FireGL Unknown" },
     { 0x5d57, CHIP_R420 , false, "Radeon X800 XT" },
     { 0x5550, CHIP_R420 , false, "FireGL V7100" },
     { 0x5d49, CHIP_R420 , false, "Mobility FireGL V5100 M28" },
     { 0x5d4a, CHIP_R420 , false, "Mobility Radeon X800 M28" },
     { 0x5d48, CHIP_R420 , false, "Mobility Radeon X800 XT M28" },
     { 0x554f, CHIP_R420 , false, "Radeon X800" },
     { 0x554d, CHIP_R420 , false, "Radeon X800 XL" },
     { 0x554e, CHIP_R420 , false, "Radeon X800 SE" },
     { 0x554c, CHIP_R420 , false, "Radeon X800 XTP" },
     { 0x5d4c, CHIP_R420 , false, "Radeon X850" },
     { 0x5d50, CHIP_R420 , false, "Radeon Unknown R480" },
     { 0x5d4e, CHIP_R420 , false, "Radeon X850 SE" },
     { 0x5d4f, CHIP_R420 , false, "Radeon X850 PRO" },
     { 0x5d52, CHIP_R420 , false, "Radeon X850 XT" },
     { 0x5d4d, CHIP_R420 , false, "Radeon X850 XT PE" },
     { 0x4b4b, CHIP_R420 , false, "Radeon X850 PRO" },
     { 0x4b4a, CHIP_R420 , false, "Radeon X850 SE" },
     { 0x4b49, CHIP_R420 , false, "Radeon X850 XT" },
     { 0x4b4c, CHIP_R420 , false, "Radeon X850 XT PE" }
};

#endif /* __RADEON_CHIPSETS_H__ */
