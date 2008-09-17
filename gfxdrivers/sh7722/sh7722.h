#ifndef __SH7722__SH7722_H__
#define __SH7722__SH7722_H__

#include <sys/ioctl.h>

#include <sh772x_gfx.h>

#include "sh7722_regs.h"
#include "sh7722_types.h"


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

//#define SH7722_TDG_REG_USE_IOCTLS

#ifdef SH7722_TDG_REG_USE_IOCTLS
static inline u32
SH7722_TDG_GETREG32( SH7722DriverData *sdrv,
                     u32               address )
{
     SH772xRegister reg = { address, 0 };

     if (ioctl( sdrv->gfx_fd, SH772xGFX_IOCTL_GETREG32, &reg ) < 0)
          D_PERROR( "SH772xGFX_IOCTL_GETREG32( 0x%08x )\n", reg.address );

     return reg.value;
}

static inline void
SH7722_TDG_SETREG32( SH7722DriverData *sdrv,
                     u32               address,
                     u32               value )
{
     SH772xRegister reg = { address, value };

     if (ioctl( sdrv->gfx_fd, SH772xGFX_IOCTL_SETREG32, &reg ) < 0)
          D_PERROR( "SH772xGFX_IOCTL_SETREG32( 0x%08x, 0x%08x )\n", reg.address, reg.value );
}
#else
static inline u32
SH7722_TDG_GETREG32( SH7722DriverData *sdrv,
                     u32               address )
{
     D_ASSERT( address >= dfb_config->mmio_phys );
     D_ASSERT( address < (dfb_config->mmio_phys + dfb_config->mmio_length) );

     return *(volatile u32*)(sdrv->mmio_base + (address - dfb_config->mmio_phys));
}

static inline void
SH7722_TDG_SETREG32( SH7722DriverData *sdrv,
                     u32               address,
                     u32               value )
{
     D_ASSERT( address >= dfb_config->mmio_phys );
     D_ASSERT( address < (dfb_config->mmio_phys + dfb_config->mmio_length) );

     *(volatile u32*)(sdrv->mmio_base + (address - dfb_config->mmio_phys)) = value;
}
#endif


static inline u32
SH7722_GETREG32( SH7722DriverData *sdrv,
                 u32               address )
{
     SH772xRegister reg = { address, 0 };

     if (ioctl( sdrv->gfx_fd, SH772xGFX_IOCTL_GETREG32, &reg ) < 0)
          D_PERROR( "SH772xGFX_IOCTL_GETREG32( 0x%08x )\n", reg.address );

     return reg.value;
}

static inline void
SH7722_SETREG32( SH7722DriverData *sdrv,
                 u32               address,
                 u32               value )
{
     SH772xRegister reg = { address, value };

     if (ioctl( sdrv->gfx_fd, SH772xGFX_IOCTL_SETREG32, &reg ) < 0)
          D_PERROR( "SH772xGFX_IOCTL_SETREG32( 0x%08x, 0x%08x )\n", reg.address, reg.value );
}


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

#endif
