#ifndef __PXA3XX__PXA3XX_REGS_H__
#define __PXA3XX__PXA3XX_REGS_H__


/******************************************************************************
 * Register access
 */

#define PXA3XX_LCD_BASE  0x44000000
#define PXA3XX_GFX_BASE  0x54000000

/******************************************************************************
 * LCD registers
 */

#define LCCR0       (PXA3XX_LCD_BASE + 0x0000)
#define LCCR1       (PXA3XX_LCD_BASE + 0x0004)
#define LCCR2       (PXA3XX_LCD_BASE + 0x0008)
#define LCCR3       (PXA3XX_LCD_BASE + 0x000C)
#define LCCR4       (PXA3XX_LCD_BASE + 0x0010)
#define LCCR5       (PXA3XX_LCD_BASE + 0x0014)
#define LCCR6       (PXA3XX_LCD_BASE + 0x0018)

#define FBR0        (PXA3XX_LCD_BASE + 0x0020)
#define FBR1        (PXA3XX_LCD_BASE + 0x0024)
#define FBR2        (PXA3XX_LCD_BASE + 0x0028)
#define FBR3        (PXA3XX_LCD_BASE + 0x002C)
#define FBR4        (PXA3XX_LCD_BASE + 0x0030)
#define LCSR1       (PXA3XX_LCD_BASE + 0x0034)
#define LCSR0       (PXA3XX_LCD_BASE + 0x0038)
#define LIIDR       (PXA3XX_LCD_BASE + 0x003C)
#define TRGBR       (PXA3XX_LCD_BASE + 0x0040)
#define TCR         (PXA3XX_LCD_BASE + 0x0044)

#define OVL1C1      (PXA3XX_LCD_BASE + 0x0050)
#define OVL1C2      (PXA3XX_LCD_BASE + 0x0060)
#define OVL2C1      (PXA3XX_LCD_BASE + 0x0070)
#define OVL2C2      (PXA3XX_LCD_BASE + 0x0080)

#define CCR         (PXA3XX_LCD_BASE + 0x0090)

#define CMDCR       (PXA3XX_LCD_BASE + 0x0100)
#define PRSR        (PXA3XX_LCD_BASE + 0x0104)

#define FBR5        (PXA3XX_LCD_BASE + 0x0110)
#define FBR6        (PXA3XX_LCD_BASE + 0x0114)

#define FDADR0      (PXA3XX_LCD_BASE + 0x0200)
#define FSADR0      (PXA3XX_LCD_BASE + 0x0204)
#define FIDR0       (PXA3XX_LCD_BASE + 0x0208)
#define LDCMD0      (PXA3XX_LCD_BASE + 0x020C)
#define FDADR1      (PXA3XX_LCD_BASE + 0x0210)
#define FSADR1      (PXA3XX_LCD_BASE + 0x0214)
#define FIDR1       (PXA3XX_LCD_BASE + 0x0218)
#define LDCMD1      (PXA3XX_LCD_BASE + 0x021C)
#define FDADR2      (PXA3XX_LCD_BASE + 0x0220)
#define FSADR2      (PXA3XX_LCD_BASE + 0x0224)
#define FIDR2       (PXA3XX_LCD_BASE + 0x0228)
#define LDCMD2      (PXA3XX_LCD_BASE + 0x022C)
#define FDADR3      (PXA3XX_LCD_BASE + 0x0230)
#define FSADR3      (PXA3XX_LCD_BASE + 0x0234)
#define FIDR3       (PXA3XX_LCD_BASE + 0x0238)
#define LDCMD3      (PXA3XX_LCD_BASE + 0x023C)
#define FDADR4      (PXA3XX_LCD_BASE + 0x0240)
#define FSADR4      (PXA3XX_LCD_BASE + 0x0244)
#define FIDR4       (PXA3XX_LCD_BASE + 0x0248)
#define LDCMD4      (PXA3XX_LCD_BASE + 0x024C)
#define FDADR5      (PXA3XX_LCD_BASE + 0x0250)
#define FSADR5      (PXA3XX_LCD_BASE + 0x0254)
#define FIDR5       (PXA3XX_LCD_BASE + 0x0258)
#define LDCMD5      (PXA3XX_LCD_BASE + 0x025C)
#define FDADR6      (PXA3XX_LCD_BASE + 0x0260)
#define FSADR6      (PXA3XX_LCD_BASE + 0x0264)
#define FIDR6       (PXA3XX_LCD_BASE + 0x0268)
#define LDCMD6      (PXA3XX_LCD_BASE + 0x026C)


/******************************************************************************
 * Graphics registers
 */

/** Miscelaneous control and Interrupt information
 *******************************************************/
#define GCCR        (PXA3XX_GFX_BASE + 0x0000)
#define GCISCR      (PXA3XX_GFX_BASE + 0x0004)
#define GCIECR      (PXA3XX_GFX_BASE + 0x0008)
#define GCNOPID     (PXA3XX_GFX_BASE + 0x000C)
#define GCALPHASET  (PXA3XX_GFX_BASE + 0x0010)
#define GCTSET      (PXA3XX_GFX_BASE + 0x0014)

/** Ring buffer information
 *******************************************************/
#define GCRBBR      (PXA3XX_GFX_BASE + 0x0020)
#define GCRBLR      (PXA3XX_GFX_BASE + 0x0024)
#define GCRBGR      (PXA3XX_GFX_BASE + 0x0028)
#define GCRBTR      (PXA3XX_GFX_BASE + 0x002C)
#define GCRBEXHR    (PXA3XX_GFX_BASE + 0x0030)

/** Batch buffer information
 *******************************************************/
#define GCBBBR      (PXA3XX_GFX_BASE + 0x0040)
#define GCBBHR      (PXA3XX_GFX_BASE + 0x0044)
#define GCBBEXHR    (PXA3XX_GFX_BASE + 0x0048)

/** Destination 0,1 and 2 information
 *******************************************************/
#define GCD0BR      (PXA3XX_GFX_BASE + 0x0060)
#define GCD0STP     (PXA3XX_GFX_BASE + 0x0064)
#define GCD0STR     (PXA3XX_GFX_BASE + 0x0068)
#define GCD0PF      (PXA3XX_GFX_BASE + 0x006C)
#define GCD1BR      (PXA3XX_GFX_BASE + 0x0070)
#define GCD1STP     (PXA3XX_GFX_BASE + 0x0074)
#define GCD1STR     (PXA3XX_GFX_BASE + 0x0078)
#define GCD1PF      (PXA3XX_GFX_BASE + 0x007C)
#define GCD2BR      (PXA3XX_GFX_BASE + 0x0080)
#define GCD2STP     (PXA3XX_GFX_BASE + 0x0084)
#define GCD2STR     (PXA3XX_GFX_BASE + 0x0088)
#define GCD2PF      (PXA3XX_GFX_BASE + 0x008C)

/** Source 0 and 1 information
 *******************************************************/
#define GCS0BR      (PXA3XX_GFX_BASE + 0x00E0)
#define GCS0STP     (PXA3XX_GFX_BASE + 0x00E4)
#define GCS0STR     (PXA3XX_GFX_BASE + 0x00E8)
#define GCS0PF      (PXA3XX_GFX_BASE + 0x00EC)
#define GCS1BR      (PXA3XX_GFX_BASE + 0x00F0)
#define GCS1STP     (PXA3XX_GFX_BASE + 0x00F4)
#define GCS1STR     (PXA3XX_GFX_BASE + 0x00F8)
#define GCS1PF      (PXA3XX_GFX_BASE + 0x00FC)

/** Pixel ALU scratch registers
 *******************************************************/
#define GCSC0WD0    (PXA3XX_GFX_BASE + 0x0160)
// ....and more

/** Abort Bad address storage registers
 *******************************************************/
#define GCCABADDR   (PXA3XX_GFX_BASE + 0x01E0)
#define GCTABADDR   (PXA3XX_GFX_BASE + 0x01E4)
#define GCMABADDR   (PXA3XX_GFX_BASE + 0x01E8)

#endif


