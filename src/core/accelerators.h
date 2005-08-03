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

#ifndef __ACCELERATORS_H__
#define __ACCELERATORS_H__

/* Linux Framebuffer accelerators */
#ifndef FB_ACCEL_ATARIBLITT
# define FB_ACCEL_ATARIBLITT           0x00000001 /* Atari Blitter */
#endif
#ifndef FB_ACCEL_AMIGABLITT
# define FB_ACCEL_AMIGABLITT           0x00000002 /* Amiga Blitter */
#endif
#ifndef FB_ACCEL_S3_TRIO64
# define FB_ACCEL_S3_TRIO64            0x00000003 /* Cybervision64 (S3 Trio64) */
#endif
#ifndef FB_ACCEL_NCR_77C32BLT
# define FB_ACCEL_NCR_77C32BLT         0x00000004 /* RetinaZ3 (NCR 77C32BLT) */
#endif
#ifndef FB_ACCEL_S3_VIRGE
# define FB_ACCEL_S3_VIRGE             0x00000005 /* Cybervision64/3D (S3 ViRGE) */
#endif
#ifndef FB_ACCEL_ATI_MACH64GX
# define FB_ACCEL_ATI_MACH64GX         0x00000006 /* ATI Mach 64GX family */
#endif
#ifndef FB_ACCEL_DEC_TGA
# define FB_ACCEL_DEC_TGA              0x00000007 /* DEC 21030 TGA */
#endif
#ifndef FB_ACCEL_ATI_MACH64CT
# define FB_ACCEL_ATI_MACH64CT         0x00000008 /* ATI Mach 64CT family */
#endif
#ifndef FB_ACCEL_ATI_MACH64VT
# define FB_ACCEL_ATI_MACH64VT         0x00000009 /* ATI Mach 64CT family VT class */
#endif
#ifndef FB_ACCEL_ATI_MACH64GT
# define FB_ACCEL_ATI_MACH64GT         0x0000000a /* ATI Mach 64CT family GT class */
#endif
#ifndef FB_ACCEL_SUN_CREATOR
# define FB_ACCEL_SUN_CREATOR          0x0000000b /* Sun Creator/Creator3D */
#endif
#ifndef FB_ACCEL_SUN_CGSIX
# define FB_ACCEL_SUN_CGSIX            0x0000000c /* Sun cg6 */
#endif
#ifndef FB_ACCEL_SUN_LEO
# define FB_ACCEL_SUN_LEO              0x0000000d /* Sun leo/zx */
#endif
#ifndef FB_ACCEL_IMS_TWINTURBO
# define FB_ACCEL_IMS_TWINTURBO        0x0000000e /* IMS Twin Turbo */
#endif
#ifndef FB_ACCEL_3DLABS_PERMEDIA2
# define FB_ACCEL_3DLABS_PERMEDIA2     0x0000000f /* 3Dlabs Permedia 2 */
#endif
#ifndef FB_ACCEL_MATROX_MGA2064W
# define FB_ACCEL_MATROX_MGA2064W      0x00000010 /* Matrox MGA2064W (Millenium) */
#endif
#ifndef FB_ACCEL_MATROX_MGA1064SG
# define FB_ACCEL_MATROX_MGA1064SG     0x00000011 /* Matrox MGA1064SG (Mystique) */
#endif
#ifndef FB_ACCEL_MATROX_MGA2164W
# define FB_ACCEL_MATROX_MGA2164W      0x00000012 /* Matrox MGA2164W (Millenium II) */
#endif
#ifndef FB_ACCEL_MATROX_MGA2164W_AGP
# define FB_ACCEL_MATROX_MGA2164W_AGP  0x00000013 /* Matrox MGA2164W (Millenium II) */
#endif
#ifndef FB_ACCEL_MATROX_MGAG100
# define FB_ACCEL_MATROX_MGAG100       0x00000014 /* Matrox G100 (Productiva G100) */
#endif
#ifndef FB_ACCEL_MATROX_MGAG200
# define FB_ACCEL_MATROX_MGAG200       0x00000015 /* Matrox G200 (Myst, Mill, ...) */
#endif
#ifndef FB_ACCEL_SUN_CG14
# define FB_ACCEL_SUN_CG14             0x00000016 /* Sun cgfourteen */
#endif
#ifndef FB_ACCEL_SUN_BWTWO
# define FB_ACCEL_SUN_BWTWO            0x00000017 /* Sun bwtwo */
#endif
#ifndef FB_ACCEL_SUN_CGTHREE
# define FB_ACCEL_SUN_CGTHREE          0x00000018 /* Sun cgthree */
#endif
#ifndef FB_ACCEL_SUN_TCX
# define FB_ACCEL_SUN_TCX              0x00000019 /* Sun tcx */
#endif
#ifndef FB_ACCEL_MATROX_MGAG400
# define FB_ACCEL_MATROX_MGAG400       0x0000001a /* Matrox G400 */
#endif
#ifndef FB_ACCEL_NV3
# define FB_ACCEL_NV3                  0x0000001b /* nVidia RIVA 128 */
#endif
#ifndef FB_ACCEL_NV4
# define FB_ACCEL_NV4                  0x0000001c /* nVidia RIVA TNT */
#endif
#ifndef FB_ACCEL_NV5
# define FB_ACCEL_NV5                  0x0000001d /* nVidia RIVA TNT2 */
#endif
#ifndef FB_ACCEL_CT_6555x
# define FB_ACCEL_CT_6555x             0x0000001e /* C&T 6555x */
#endif
#ifndef FB_ACCEL_3DFX_BANSHEE
# define FB_ACCEL_3DFX_BANSHEE         0x0000001f /* 3Dfx Banshee */
#endif
#ifndef FB_ACCEL_ATI_RAGE128
# define FB_ACCEL_ATI_RAGE128          0x00000020 /* ATI Rage128 family */
#endif
#ifndef FB_ACCEL_IGS_CYBER2000
# define FB_ACCEL_IGS_CYBER2000        0x00000021 /* CyberPro 2000 */
#endif
#ifndef FB_ACCEL_IGS_CYBER2010
# define FB_ACCEL_IGS_CYBER2010        0x00000022 /* CyberPro 2010 */
#endif
#ifndef FB_ACCEL_IGS_CYBER5000
# define FB_ACCEL_IGS_CYBER5000        0x00000023 /* CyberPro 5000 */
#endif
#ifndef FB_ACCEL_SIS_GLAMOUR
# define FB_ACCEL_SIS_GLAMOUR          0x00000024 /* SiS 300/630/540 */
#endif
#ifndef FB_ACCEL_3DLABS_PERMEDIA3
# define FB_ACCEL_3DLABS_PERMEDIA3     0x00000025 /* 3Dlabs Permedia 3 */
#endif
#ifndef FB_ACCEL_ATI_RADEON
# define FB_ACCEL_ATI_RADEON           0x00000026 /* ATI Radeon family */
#endif
#ifndef FB_ACCEL_I810
# define FB_ACCEL_I810                 0x00000027 /* Intel 810/815 */
#endif
#ifndef FB_ACCEL_SIS_GLAMOUR_2
# define FB_ACCEL_SIS_GLAMOUR_2        0x00000028 /* SiS 315, 650, 740 */
#endif
#ifndef FB_ACCEL_SIS_XABRE
# define FB_ACCEL_SIS_XABRE            0x00000029 /* SiS 330 ("Xabre") */
#endif
#ifndef FB_ACCEL_I830
# define FB_ACCEL_I830                 0x0000002a /* Intel 830M/845G/85x/865G */
#endif
#ifndef FB_ACCEL_NV10
# define FB_ACCEL_NV10                 0x0000002b /* nVidia Arch 10 */
#endif
#ifndef FB_ACCEL_NV20
# define FB_ACCEL_NV20                 0x0000002c /* nVidia Arch 20 */
#endif
#ifndef FB_ACCEL_NV30
# define FB_ACCEL_NV30                 0x0000002d /* nVidia Arch 30 */
#endif
#ifndef FB_ACCEL_NV40
# define FB_ACCEL_NV40                 0x0000002e /* nVidia Arch 40 */
#endif
#ifndef FB_ACCEL_NEOMAGIC_NM2070
# define FB_ACCEL_NEOMAGIC_NM2070      0x0000005a /* NeoMagic NM2070 */
#endif
#ifndef FB_ACCEL_NEOMAGIC_NM2090
# define FB_ACCEL_NEOMAGIC_NM2090      0x0000005b /* NeoMagic NM2090 */
#endif
#ifndef FB_ACCEL_NEOMAGIC_NM2093
# define FB_ACCEL_NEOMAGIC_NM2093      0x0000005c /* NeoMagic NM2093 */
#endif
#ifndef FB_ACCEL_NEOMAGIC_NM2097
# define FB_ACCEL_NEOMAGIC_NM2097      0x0000005d /* NeoMagic NM2097 */
#endif
#ifndef FB_ACCEL_NEOMAGIC_NM2160
# define FB_ACCEL_NEOMAGIC_NM2160      0x0000005e /* NeoMagic NM2160 */
#endif
#ifndef FB_ACCEL_NEOMAGIC_NM2200
# define FB_ACCEL_NEOMAGIC_NM2200      0x0000005f /* NeoMagic NM2200 */
#endif
#ifndef FB_ACCEL_NEOMAGIC_NM2230
# define FB_ACCEL_NEOMAGIC_NM2230      0x00000060 /* NeoMagic NM2230 */
#endif
#ifndef FB_ACCEL_NEOMAGIC_NM2360
# define FB_ACCEL_NEOMAGIC_NM2360      0x00000061 /* NeoMagic NM2360 */
#endif
#ifndef FB_ACCEL_NEOMAGIC_NM2380
# define FB_ACCEL_NEOMAGIC_NM2380      0x00000062 /* NeoMagic NM2380 */
#endif
#ifndef FB_ACCEL_SAVAGE4
# define FB_ACCEL_SAVAGE4              0x00000080 /* S3 Savage4 */
#endif
#ifndef FB_ACCEL_SAVAGE3D
# define FB_ACCEL_SAVAGE3D             0x00000081 /* S3 Savage3D */
#endif
#ifndef FB_ACCEL_SAVAGE3D_MV
# define FB_ACCEL_SAVAGE3D_MV          0x00000082 /* S3 Savage3D-MV */
#endif
#ifndef FB_ACCEL_SAVAGE2000
# define FB_ACCEL_SAVAGE2000           0x00000083 /* S3 Savage2000 */
#endif
#ifndef FB_ACCEL_SAVAGE_MX_MV
# define FB_ACCEL_SAVAGE_MX_MV         0x00000084 /* S3 Savage/MX-MV */
#endif
#ifndef FB_ACCEL_SAVAGE_MX
# define FB_ACCEL_SAVAGE_MX            0x00000085 /* S3 Savage/MX */
#endif
#ifndef FB_ACCEL_SAVAGE_IX_MV
# define FB_ACCEL_SAVAGE_IX_MV         0x00000086 /* S3 Savage/IX-MV */
#endif
#ifndef FB_ACCEL_SAVAGE_IX
# define FB_ACCEL_SAVAGE_IX            0x00000087 /* S3 Savage/IX */
#endif
#ifndef FB_ACCEL_PROSAVAGE_PM
# define FB_ACCEL_PROSAVAGE_PM         0x00000088 /* S3 ProSavage PM133 */
#endif
#ifndef FB_ACCEL_PROSAVAGE_KM
# define FB_ACCEL_PROSAVAGE_KM         0x00000089 /* S3 ProSavage KM133 */
#endif
#ifndef FB_ACCEL_S3TWISTER_P
# define FB_ACCEL_S3TWISTER_P          0x0000008a /* S3 Twister */
#endif
#ifndef FB_ACCEL_S3TWISTER_K
# define FB_ACCEL_S3TWISTER_K          0x0000008b /* S3 TwisterK */
#endif
#ifndef FB_ACCEL_SUPERSAVAGE
# define FB_ACCEL_SUPERSAVAGE          0x0000008c /* S3 Supersavage */
#endif
#ifndef FB_ACCEL_PROSAVAGE_DDR
# define FB_ACCEL_PROSAVAGE_DDR        0x0000008d /* S3 ProSavage DDR */
#endif
#ifndef FB_ACCEL_PROSAVAGE_DDRK
# define FB_ACCEL_PROSAVAGE_DDRK       0x0000008e /* S3 ProSavage DDR-K */
#endif


#endif /* __ACCELERATORS_H__ */
