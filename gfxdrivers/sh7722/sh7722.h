#ifndef __SH7722__SH7722_H__
#define __SH7722__SH7722_H__

#include <sys/ioctl.h>

#include <sh7722gfx.h>

#include "sh7722_types.h"

extern SH7722GfxSharedArea *sh7722gfx_shared;


/******************************************************************************
 * Platform specific values (FIXME: add runtime config)
 */

#define ALGO_AP325
#undef  SH7722_ALGO_PANEL

/* LCD Panel Configuration */
#if defined(SH7722_ALGO_PANEL)
#  define	SH7722_LCD_WIDTH	640
#  define	SH7722_LCD_HEIGHT	480
#elif defined(ALGO_AP325)
#  define	SH7722_LCD_WIDTH	800
#  define	SH7722_LCD_HEIGHT	480
#else
#  define	SH7722_LCD_WIDTH	800
#  define	SH7722_LCD_HEIGHT	480
#endif


/******************************************************************************
 * Register access
 */

#define VEU_REG_BASE     0xFE920000
#define SH7722_BEU_BASE  0xFE930000
#define LCDC_REG_BASE    0xFE940000
#define JPEG_REG_BASE    0xFEA00000

#ifdef SH7722_REG_USE_IOCTLS
static inline u32
SH7722_GETREG32( SH7722DriverData *sdrv,
                 u32               address )
{
     SH7722Register reg = { address, 0 };

     if (ioctl( sdrv->gfx_fd, SH7722GFX_IOCTL_GETREG32, &reg ) < 0)
          D_PERROR( "SH7722GFX_IOCTL_GETREG32( 0x%08x )\n", reg.address );

     return reg.value;
}

static inline void
SH7722_SETREG32( SH7722DriverData *sdrv,
                 u32               address,
                 u32               value )
{
     SH7722Register reg = { address, value };

     if (ioctl( sdrv->gfx_fd, SH7722GFX_IOCTL_SETREG32, &reg ) < 0)
          D_PERROR( "SH7722GFX_IOCTL_SETREG32( 0x%08x, 0x%08x )\n", reg.address, reg.value );
}
#else
static inline u32
SH7722_GETREG32( SH7722DriverData *sdrv,
                 u32               address )
{
     D_ASSERT( address >= dfb_config->mmio_phys );
     D_ASSERT( address < (dfb_config->mmio_phys + dfb_config->mmio_length) );

     return *(volatile u32*)(sdrv->mmio_base + (address - dfb_config->mmio_phys));
}

static inline void
SH7722_SETREG32( SH7722DriverData *sdrv,
                 u32               address,
                 u32               value )
{
     D_ASSERT( address >= dfb_config->mmio_phys );
     D_ASSERT( address < (dfb_config->mmio_phys + dfb_config->mmio_length) );

     *(volatile u32*)(sdrv->mmio_base + (address - dfb_config->mmio_phys)) = value;
}
#endif


/******************************************************************************
 * BEU
 */

/* BEU start register */
#define BESTR       (SH7722_BEU_BASE + 0x0000)

/* BEU source memory width register 1 */
#define BSMWR1      (SH7722_BEU_BASE + 0x0010)

/* BEU source size register 1 */
#define BSSZR1      (SH7722_BEU_BASE + 0x0014)

/* BEU source address Y register 1 */
#define BSAYR1      (SH7722_BEU_BASE + 0x0018)

/* BEU source address C register 1 */
#define BSACR1      (SH7722_BEU_BASE + 0x001C)

/* BEU source address A register 1 */
#define BSAAR1      (SH7722_BEU_BASE + 0x0020)

/* BEU source image format register 1 */
#define BSIFR1      (SH7722_BEU_BASE + 0x0024)

/* BEU source memory width register 2 */
#define BSMWR2      (SH7722_BEU_BASE + 0x0028)

/* BEU source size register 2 */
#define BSSZR2      (SH7722_BEU_BASE + 0x002C)

/* BEU source address Y register 2 */
#define BSAYR2      (SH7722_BEU_BASE + 0x0030)

/* BEU source address C register 2 */
#define BSACR2      (SH7722_BEU_BASE + 0x0034)

/* BEU source address A register 2 */
#define BSAAR2      (SH7722_BEU_BASE + 0x0038)

/* BEU source image format register 2 */
#define BSIFR2      (SH7722_BEU_BASE + 0x003C)

/* BEU source memory width register 3 */
#define BSMWR3      (SH7722_BEU_BASE + 0x0040)

/* BEU source size register 3 */
#define BSSZR3      (SH7722_BEU_BASE + 0x0044)

/* BEU source address Y register 3 */
#define BSAYR3      (SH7722_BEU_BASE + 0x0048)

/* BEU source address C register 3 */
#define BSACR3      (SH7722_BEU_BASE + 0x004C)

/* BEU source address A register 3 */
#define BSAAR3      (SH7722_BEU_BASE + 0x0050)

/* BEU source image format register 3 */
#define BSIFR3      (SH7722_BEU_BASE + 0x0054)

/* BEU tile pattern size register */
#define BTPSR       (SH7722_BEU_BASE + 0x0058)

/* BEU multidisplay source memory width register 1 */
#define BMSMWR1     (SH7722_BEU_BASE + 0x0070)

/* BEU multidisplay source size register 1 */
#define BMSSZR1     (SH7722_BEU_BASE + 0x0074)

/* BEU multidisplay source address Y register 1 */
#define BMSAYR1     (SH7722_BEU_BASE + 0x0078)

/* BEU multidisplay source address C register 1 */
#define BMSACR1     (SH7722_BEU_BASE + 0x007C)

/* BEU multidisplay source memory width register 2 */
#define BMSMWR2     (SH7722_BEU_BASE + 0x0080)

/* BEU multidisplay source size register 2 */
#define BMSSZR2     (SH7722_BEU_BASE + 0x0084)

/* BEU multidisplay source address Y register 2 */
#define BMSAYR2     (SH7722_BEU_BASE + 0x0088)

/* BEU multidisplay source address C register 2 */
#define BMSACR2     (SH7722_BEU_BASE + 0x008C)

/* BEU multidisplay source memory width register 3 */
#define BMSMWR3     (SH7722_BEU_BASE + 0x0090)

/* BEU multidisplay source size register 3 */
#define BMSSZR3     (SH7722_BEU_BASE + 0x0094)

/* BEU multidisplay source address Y register 3 */
#define BMSAYR3     (SH7722_BEU_BASE + 0x0098)

/* BEU multidisplay source address C register 3 */
#define BMSACR3     (SH7722_BEU_BASE + 0x009C)

/* BEU multidisplay source memory width register 4 */
#define BMSMWR4     (SH7722_BEU_BASE + 0x00A0)

/* BEU multidisplay source size register 4 */
#define BMSSZR4     (SH7722_BEU_BASE + 0x00A4)

/* BEU multidisplay source address Y register 4 */
#define BMSAYR4     (SH7722_BEU_BASE + 0x00A8)

/* BEU multidisplay source address C register 4 */
#define BMSACR4     (SH7722_BEU_BASE + 0x00AC)

/* BEU multidisplay source image format register */
#define BMSIFR      (SH7722_BEU_BASE + 0x00F0)

/* BEU blend control register 0 */
#define BBLCR0      (SH7722_BEU_BASE + 0x0100)

/* BEU blend control register 1 */
#define BBLCR1      (SH7722_BEU_BASE + 0x0104)

/* BEU process control register */
#define BPROCR      (SH7722_BEU_BASE + 0x0108)

/* BEU multiwindow control register 0 */
#define BMWCR0      (SH7722_BEU_BASE + 0x010C)

/* Blend location register 1 */
#define BLOCR1      (SH7722_BEU_BASE + 0x0114)

/* Blend location register 2 */
#define BLOCR2      (SH7722_BEU_BASE + 0x0118)

/* Blend location register 3 */
#define BLOCR3      (SH7722_BEU_BASE + 0x011C)

/* BEU multidisplay location register 1 */
#define BMLOCR1     (SH7722_BEU_BASE + 0x0120)

/* BEU multidisplay location register 2 */
#define BMLOCR2     (SH7722_BEU_BASE + 0x0124)

/* BEU multidisplay location register 3 */
#define BMLOCR3     (SH7722_BEU_BASE + 0x0128)

/* BEU multidisplay location register 4 */
#define BMLOCR4     (SH7722_BEU_BASE + 0x012C)

/* BEU multidisplay transparent color control register 1 */
#define BMPCCR1     (SH7722_BEU_BASE + 0x0130)

/* BEU multidisplay transparent color control register 2 */
#define BMPCCR2     (SH7722_BEU_BASE + 0x0134)

/* Blend pack form register */
#define BPKFR       (SH7722_BEU_BASE + 0x0140)

/* BEU transparent color control register 0 */
#define BPCCR0      (SH7722_BEU_BASE + 0x0144)

/* BEU transparent color control register 11 */
#define BPCCR11     (SH7722_BEU_BASE + 0x0148)

/* BEU transparent color control register 12 */
#define BPCCR12     (SH7722_BEU_BASE + 0x014C)

/* BEU transparent color control register 21 */
#define BPCCR21     (SH7722_BEU_BASE + 0x0150)

/* BEU transparent color control register 22 */
#define BPCCR22     (SH7722_BEU_BASE + 0x0154)

/* BEU transparent color control register 31 */
#define BPCCR31     (SH7722_BEU_BASE + 0x0158)

/* BEU transparent color control register 32 */
#define BPCCR32     (SH7722_BEU_BASE + 0x015C)

/* BEU destination memory width register */
#define BDMWR       (SH7722_BEU_BASE + 0x0160)

/* BEU destination address Y register */
#define BDAYR       (SH7722_BEU_BASE + 0x0164)

/* BEU destination address C register */
#define BDACR       (SH7722_BEU_BASE + 0x0168)

/* BEU address fixed register */
#define BAFXR       (SH7722_BEU_BASE + 0x0180)

/* BEU swapping register */
#define BSWPR       (SH7722_BEU_BASE + 0x0184)

/* BEU event interrupt enable register */
#define BEIER       (SH7722_BEU_BASE + 0x0188)

/* BEU event register */
#define BEVTR       (SH7722_BEU_BASE + 0x018C)

/* BEU register control register */
#define BRCNTR      (SH7722_BEU_BASE + 0x0194)

/* BEU status register */
#define BSTAR       (SH7722_BEU_BASE + 0x0198)

/* BEU module reset register */
#define BBRSTR      (SH7722_BEU_BASE + 0x019C)

/* BEU register-plane forcible setting register */
#define BRCHR       (SH7722_BEU_BASE + 0x01A0)


/* Color Lookup Table - CLUT registers (0-255) */
#define BCLUT(n)    (SH7722_BEU_BASE + 0x3000 + (n) * 0x04)



/* BEU source memory width registers (0-2) */
#define BSMWR(n)    (SH7722_BEU_BASE + 0x0010 + (n) * 0x18)

/* BEU source size registers (0-2) */
#define BSSZR(n)    (SH7722_BEU_BASE + 0x0014 + (n) * 0x18)

/* BEU source address Y registers (0-2) */
#define BSAYR(n)    (SH7722_BEU_BASE + 0x0018 + (n) * 0x18)

/* BEU source address C registers (0-2) */
#define BSACR(n)    (SH7722_BEU_BASE + 0x001C + (n) * 0x18)

/* BEU source address A registers (0-2) */
#define BSAAR(n)    (SH7722_BEU_BASE + 0x0020 + (n) * 0x18)

/* BEU source image format registers (0-2) */
#define BSIFR(n)    (SH7722_BEU_BASE + 0x0024 + (n) * 0x18)



/* BEU multidisplay source memory width registers (0-3) */
#define BMSMWR(n)   (SH7722_BEU_BASE + 0x0070 + (n) * 0x10)

/* BEU multidisplay source size registers (0-3) */
#define BMSSZR(n)   (SH7722_BEU_BASE + 0x0074 + (n) * 0x10)

/* BEU multidisplay source address Y registers (0-3) */
#define BMSAYR(n)   (SH7722_BEU_BASE + 0x0078 + (n) * 0x10)

/* BEU multidisplay source address C registers (0-3) */
#define BMSACR(n)   (SH7722_BEU_BASE + 0x007C + (n) * 0x10)



/* Blend location registers (0-2) */
#define BLOCR(n)    (SH7722_BEU_BASE + 0x0114 + (n) * 0x04)

/* BEU multidisplay location registers (0-3) */
#define BMLOCR(n)   (SH7722_BEU_BASE + 0x0120 + (n) * 0x04)


/* BSIFR1-3 */
#define CHRR_YCBCR_444   0x000
#define CHRR_YCBCR_422   0x100
#define CHRR_YCBCR_420   0x200
#define CHRR_aYCBCR_444  0x300
#define CHRR_aYCBCR_422  0x400
#define CHRR_aYCBCR_420  0x500

#define RPKF_ARGB        0x000
#define RPKF_RGB32       0x000
#define RPKF_RGB24       0x002
#define RPKF_RGB16       0x003

/* BSIFR1 */
#define BSIFR1_IN1TE_RGBYUV   0x1000

/* BSIFR3 */
#define BSIFR3_MOD0_OSD       0x1000
#define BSIFR3_MOD1_LUT       0x2000

/* BPKFR */
#define WPCK_RGB12            2
#define WPCK_RGB16            6
#define WPCK_RGB18            17
#define WPCK_RGB32            19
#define WPCK_RGB24            21

#define CHDS_YCBCR444         0x000
#define CHDS_YCBCR422         0x100
#define CHDS_YCBCR420         0x200

#define BPKFR_RY_YUV          0x000
#define BPKFR_RY_RGB          0x800
#define BPKFR_TE_DISABLED     0x000
#define BPKFR_TE_ENABLED      0x400

/* BBLCR0 */
#define BBLCR0_LAY_123        0x05000000

#define BBLCR0_AMUX_BLENDPIXEL(n)  (0x10000000 << (n))

/* BBLCR1 */
#define MT_MEMORY             0x10000
#define MT_VOU                0x20000
#define MT_MEMORY_VOU         0x30000
#define MT_LCDC               0x40000
#define MT_LCDC_MEMORY        0x50000

#define BBLCR1_PWD_INPUT1     0x00000000
#define BBLCR1_PWD_INPUT2     0x01000000
#define BBLCR1_PWD_INPUT3     0x02000000
#define BBLCR1_PWD_INPUT_MASK 0x03000000

/* BSWPR */
#define BSWPR_MODSEL_GLOBAL   0x00000000
#define BSWPR_MODSEL_EACH     0x80000000

#define BSWPR_INPUT_BYTESWAP  0x00000001
#define BSWPR_INPUT_WORDSWAP  0x00000002
#define BSWPR_INPUT_LONGSWAP  0x00000004

#define BSWPR_OUTPUT_BYTESWAP 0x00000010
#define BSWPR_OUTPUT_WORDSWAP 0x00000020
#define BSWPR_OUTPUT_LONGSWAP 0x00000040

#define BSWPR_INPUT2_BYTESWAP 0x00000100
#define BSWPR_INPUT2_WORDSWAP 0x00000200
#define BSWPR_INPUT2_LONGSWAP 0x00000400

#define BSWPR_INPUT3_BYTESWAP 0x00010000
#define BSWPR_INPUT3_WORDSWAP 0x00020000
#define BSWPR_INPUT3_LONGSWAP 0x00040000

#define BSWPR_MULWIN_BYTESWAP 0x01000000
#define BSWPR_MULWIN_WORDSWAP 0x02000000
#define BSWPR_MULWIN_LONGSWAP 0x04000000


static inline void
BEU_Start( SH7722DriverData *sdrv,
           SH7722DeviceData *sdev )
{
     /* Wait for idle BEU. */
     while (SH7722_GETREG32( sdrv, BSTAR ) & 1);

     /* Start operation! */
     SH7722_SETREG32( sdrv, BESTR, (sdev->input_mask << 8) | 1 );
}

static inline void
BEU_Wait( SH7722DriverData *sdrv,
          SH7722DeviceData *sdev )
{
     /* Wait for idle BEU. */
     while (SH7722_GETREG32( sdrv, BSTAR ) & 1);
}


/******************************************************************************
 * VEU
 */

#define VEU_VESTR             (VEU_REG_BASE + 0x0000)
#define VEU_VESWR             (VEU_REG_BASE + 0x0010)
#define VEU_VESSR             (VEU_REG_BASE + 0x0014)
#define VEU_VSAYR             (VEU_REG_BASE + 0x0018)
#define VEU_VSACR             (VEU_REG_BASE + 0x001c)
#define VEU_VBSSR             (VEU_REG_BASE + 0x0020)
#define VEU_VEDWR             (VEU_REG_BASE + 0x0030)
#define VEU_VDAYR             (VEU_REG_BASE + 0x0034)
#define VEU_VDACR             (VEU_REG_BASE + 0x0038)
#define VEU_VTRCR             (VEU_REG_BASE + 0x0050)
#define VEU_VRFCR             (VEU_REG_BASE + 0x0054)
#define VEU_VRFSR             (VEU_REG_BASE + 0x0058)
#define VEU_VENHR             (VEU_REG_BASE + 0x005c)
#define VEU_VFMCR             (VEU_REG_BASE + 0x0070)
#define VEU_VVTCR             (VEU_REG_BASE + 0x0074)
#define VEU_VHTCR             (VEU_REG_BASE + 0x0078)
#define VEU_VAPCR             (VEU_REG_BASE + 0x0080)
#define VEU_VECCR             (VEU_REG_BASE + 0x0084)
#define VEU_VAFXR             (VEU_REG_BASE + 0x0090)
#define VEU_VSWPR             (VEU_REG_BASE + 0x0094)
#define VEU_VEIER             (VEU_REG_BASE + 0x00a0)
#define VEU_VEVTR             (VEU_REG_BASE + 0x00a4)
#define VEU_VSTAR             (VEU_REG_BASE + 0x00b0)
#define VEU_VBSRR             (VEU_REG_BASE + 0x00b4)


/******************************************************************************
 * LCD
 */

#define LCDC_LUT(n)           (LCDC_REG_BASE + (n) * 4)
#define LCDC_MLDDCKPAT1R      (LCDC_REG_BASE + 0x0400)
#define LCDC_MLDDCKPAT2R      (LCDC_REG_BASE + 0x0404)
#define LCDC_SLDDCKPAT1R      (LCDC_REG_BASE + 0x0408)
#define LCDC_SLDDCKPAT2R      (LCDC_REG_BASE + 0x040c)
#define LCDC_LDDCKR           (LCDC_REG_BASE + 0x0410)
#define LCDC_LDDCKSTPR        (LCDC_REG_BASE + 0x0414)
#define LCDC_MLDMT1R          (LCDC_REG_BASE + 0x0418)
#define LCDC_MLDMT2R          (LCDC_REG_BASE + 0x041c)
#define LCDC_MLDMT3R          (LCDC_REG_BASE + 0x0420)
#define LCDC_MLDDFR           (LCDC_REG_BASE + 0x0424)
#define LCDC_MLDSM1R          (LCDC_REG_BASE + 0x0428)
#define LCDC_MLDSM2R          (LCDC_REG_BASE + 0x042c)
#define LCDC_MLDSA1R          (LCDC_REG_BASE + 0x0430)
#define LCDC_MLDSA2R          (LCDC_REG_BASE + 0x0434)
#define LCDC_MLDMLSR          (LCDC_REG_BASE + 0x0438)
#define LCDC_MLDWBFR          (LCDC_REG_BASE + 0x043c)
#define LCDC_MLDWBCNTR        (LCDC_REG_BASE + 0x0440)
#define LCDC_MLDWBAR          (LCDC_REG_BASE + 0x0444)
#define LCDC_MLDHCNR          (LCDC_REG_BASE + 0x0448)
#define LCDC_MLDHSYNR         (LCDC_REG_BASE + 0x044c)
#define LCDC_MLDVLNR          (LCDC_REG_BASE + 0x0450)
#define LCDC_MLDVSYNR         (LCDC_REG_BASE + 0x0454)
#define LCDC_MLDHPDR          (LCDC_REG_BASE + 0x0458)
#define LCDC_MLDVPDR          (LCDC_REG_BASE + 0x045c)
#define LCDC_MLDPMR           (LCDC_REG_BASE + 0x0460)
#define LCDC_LDPALCR          (LCDC_REG_BASE + 0x0464)
#define LCDC_LDINTR           (LCDC_REG_BASE + 0x0468)
#define LCDC_LDSR             (LCDC_REG_BASE + 0x046c)
#define LCDC_LDCNT1R          (LCDC_REG_BASE + 0x0470)
#define LCDC_LDCNT2R          (LCDC_REG_BASE + 0x0474)
#define LCDC_LDRCNTR          (LCDC_REG_BASE + 0x0478)
#define LCDC_LDDDSR           (LCDC_REG_BASE + 0x047c)
#define LCDC_LDRCR            (LCDC_REG_BASE + 0x0484)


/******************************************************************************
 * JPEG
 */

/* JPEG code mode register */
#define JCMOD                 (JPEG_REG_BASE + 0x0000)

/* JPEG code command register */
#define JCCMD                 (JPEG_REG_BASE + 0x0004)

/* JPEG code status register */
#define JCSTS                 (JPEG_REG_BASE + 0x0008)

/* JPEG code quantization table number register */
#define JCQTN                 (JPEG_REG_BASE + 0x000C)

/* JPEG code Huffman table number register */
#define JCHTN                 (JPEG_REG_BASE + 0x0010)

/* JPEG code DRI upper register */
#define JCDRIU                (JPEG_REG_BASE + 0x0014)

/* JPEG code DRI lower register */
#define JCDRID                (JPEG_REG_BASE + 0x0018)

/* JPEG code vertical size upper register */
#define JCVSZU                (JPEG_REG_BASE + 0x001C)

/* JPEG code vertical size lower register */
#define JCVSZD                (JPEG_REG_BASE + 0x0020)

/* JPEG code horizontal size upper register */
#define JCHSZU                (JPEG_REG_BASE + 0x0024)

/* JPEG code horizontal size lower register */
#define JCHSZD                (JPEG_REG_BASE + 0x0028)

/* JPEG code data count upper register */
#define JCDTCU                (JPEG_REG_BASE + 0x002C)

/* JPEG code data count middle register */
#define JCDTCM                (JPEG_REG_BASE + 0x0030)

/* JPEG code data count lower register */
#define JCDTCD                (JPEG_REG_BASE + 0x0034)

/* JPEG interrupt enable register */
#define JINTE                 (JPEG_REG_BASE + 0x0038)

/* JPEG interrupt status register */
#define JINTS                 (JPEG_REG_BASE + 0x003C)

/* JPEG code decode error register */
#define JCDERR                (JPEG_REG_BASE + 0x0040)

/* JPEG code reset register */
#define JCRST                 (JPEG_REG_BASE + 0x0044)

/* JPEG interface control register */
#define JIFCNT                (JPEG_REG_BASE + 0x0060)

/* JPEG interface encoding control register */
#define JIFECNT               (JPEG_REG_BASE + 0x0070)

/* JPEG interface encode source Y address register 1 */
#define JIFESYA1              (JPEG_REG_BASE + 0x0074)

/* JPEG interface encode source C address register 1 */
#define JIFESCA1              (JPEG_REG_BASE + 0x0078)

/* JPEG interface encode source Y address register 2 */
#define JIFESYA2              (JPEG_REG_BASE + 0x007C)

/* JPEG interface encode source C address register 2 */
#define JIFESCA2              (JPEG_REG_BASE + 0x0080)

/* JPEG interface encode source memory width register */
#define JIFESMW               (JPEG_REG_BASE + 0x0084)

/* JPEG interface encode source vertical size register */
#define JIFESVSZ              (JPEG_REG_BASE + 0x0088)

/* JPEG interface encode source horizontal size register */
#define JIFESHSZ              (JPEG_REG_BASE + 0x008C)

/* JPEG interface encode destination address register 1 */
#define JIFEDA1               (JPEG_REG_BASE + 0x0090)

/* JPEG interface encode destination address register 2 */
#define JIFEDA2               (JPEG_REG_BASE + 0x0094)

/* JPEG interface encode data reload size register */
#define JIFEDRSZ              (JPEG_REG_BASE + 0x0098)

/* JPEG interface decoding control register */
#define JIFDCNT               (JPEG_REG_BASE + 0x00A0)

/* JPEG interface decode source address register 1 */
#define JIFDSA1               (JPEG_REG_BASE + 0x00A4)

/* JPEG interface decode source address register 2 */
#define JIFDSA2               (JPEG_REG_BASE + 0x00A8)

/* JPEG interface decode data reload size register */
#define JIFDDRSZ              (JPEG_REG_BASE + 0x00AC)

/* JPEG interface decode destination memory width register */
#define JIFDDMW               (JPEG_REG_BASE + 0x00B0)

/* JPEG interface decode destination vertical size register */
#define JIFDDVSZ              (JPEG_REG_BASE + 0x00B4)

/* JPEG interface decode destination horizontal size register */
#define JIFDDHSZ              (JPEG_REG_BASE + 0x00B8)

/* JPEG interface decode destination Y address register 1 */
#define JIFDDYA1              (JPEG_REG_BASE + 0x00BC)

/* JPEG interface decode destination C address register 1 */
#define JIFDDCA1              (JPEG_REG_BASE + 0x00C0)

/* JPEG interface decode destination Y address register 2 */
#define JIFDDYA2              (JPEG_REG_BASE + 0x00C4)

/* JPEG interface decode destination C address register 2 */
#define JIFDDCA2              (JPEG_REG_BASE + 0x00C8)


/* JPEG code quantization table 0 register */
#define JCQTBL0               (JPEG_REG_BASE + 0x10000) // to 0x1003C

/* JPEG code quantization table 1 register */
#define JCQTBL1               (JPEG_REG_BASE + 0x10040) // to 0x1007C

/* JPEG code quantization table 2 register */
#define JCQTBL2               (JPEG_REG_BASE + 0x10080) // to 0x100BC

/* JPEG code quantization table 3 register */
#define JCQTBL3               (JPEG_REG_BASE + 0x100C0) // to 0x100FC

/* JPEG code Huffman table DC0 register */
#define JCHTBD0               (JPEG_REG_BASE + 0x10100) // to 0x1010C

/* JPEG code Huffman table DC0 register */
#define JCHTBD1               (JPEG_REG_BASE + 0x10110) // to 0x10118

/* JPEG code Huffman table AC0 register */
#define JCHTBA0               (JPEG_REG_BASE + 0x10120) // to 0x1012C

/* JPEG code Huffman table AC0 register */
#define JCHTBA1               (JPEG_REG_BASE + 0x10130) // to 0x101D0

/* JPEG code Huffman table DC1 register */
#define JCHTBD2               (JPEG_REG_BASE + 0x10200) // to 0x1020C

/* JPEG code Huffman table DC1 register */
#define JCHTBD3               (JPEG_REG_BASE + 0x10210) // to 0x10218

/* JPEG code Huffman table AC1 register */
#define JCHTBA2               (JPEG_REG_BASE + 0x10220) // to 0x1022C

/* JPEG code Huffman table AC1 register */
#define JCHTBA3               (JPEG_REG_BASE + 0x10230) // to 0x102D0 


#define JCCMD_START           0x00000001
#define JCCMD_RESTART         0x00000002
#define JCCMD_END             0x00000004
#define JCCMD_RESET           0x00000080
#define JCCMD_READ_RESTART    0x00000400
#define JCMOD_DSP_ENCODE      0x00000000
#define JCMOD_DSP_DECODE      0x00000008
#define JCMOD_INPUT_CTRL      0x00000080  // must always be set
#define JIFCNT_VJSEL_JPU      0x00000000
#define JIFCNT_VJSEL_VPU      0x00000002
#define JIFECNT_SWAP_1234     0x00000000
#define JIFECNT_SWAP_2143     0x00000010
#define JIFECNT_SWAP_3412     0x00000020
#define JIFECNT_SWAP_4321     0x00000030
#define JIFDCNT_LINEBUF_MODE  0x00000001
#define JIFDCNT_SWAP_1234     0x00000000
#define JIFDCNT_SWAP_2143     0x00000002
#define JIFDCNT_SWAP_3412     0x00000004
#define JIFDCNT_SWAP_4321     0x00000006
#define JIFDCNT_RELOAD_ENABLE 0x00000008

#define JINTS_MASK            0x00005C68
#define JINTS_INS3_HEADER     0x00000008
#define JINTS_INS5_ERROR      0x00000020
#define JINTS_INS6_DONE       0x00000040
#define JINTS_INS10_XFER_DONE 0x00000400
#define JINTS_INS11_LINEBUF0  0x00000800
#define JINTS_INS12_LINEBUF1  0x00001000
#define JINTS_INS14_RELOAD    0x00004000

#endif
