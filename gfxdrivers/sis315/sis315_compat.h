#ifndef _SIS315_COMPAT_H
#define _SIS315_COMPAT_H

#include <dfb_types.h>

#include <fbdev/fb.h>

#ifndef FB_ACCEL_SIS_GLAMOUR_2
#define FB_ACCEL_SIS_GLAMOUR_2  40     /* SiS 315, 650, 740            */
#endif
#ifndef FB_ACCEL_SIS_XABRE
#define FB_ACCEL_SIS_XABRE      41     /* SiS 330 ("Xabre")            */
#endif


struct sisfb_info {
     u32     sisfb_id;          /* for identifying sisfb */
#ifndef SISFB_ID
#define SISFB_ID       0x53495346    /* Identify myself with 'SISF' */
#endif
     u32     chip_id;          /* PCI-ID of detected chip */
     u32     memory;               /* total video memory in KB */
     u32     heapstart;          /* heap start offset in KB */
     u8      fbvidmode;          /* current sisfb mode */

     u8      sisfb_version;
     u8      sisfb_revision;
     u8      sisfb_patchlevel;

     u8      sisfb_caps;          /* sisfb capabilities */

     u32     sisfb_tqlen;          /* turbo queue length (in KB) */

     u32     sisfb_pcibus;          /* The card's PCI ID */
     u32     sisfb_pcislot;
     u32     sisfb_pcifunc;

     u8      sisfb_lcdpdc;          /* PanelDelayCompensation */

     u8      sisfb_lcda;          /* Detected status of LCDA for low res/text modes */

     u32     sisfb_vbflags;
     u32     sisfb_currentvbflags;

     u32     sisfb_scalelcd;
     u32     sisfb_specialtiming;

     u8      sisfb_haveemi;
     u8      sisfb_emi30,sisfb_emi31,sisfb_emi32,sisfb_emi33;
     u8      sisfb_haveemilcd;

     u8      sisfb_lcdpdca;          /* PanelDelayCompensation for LCD-via-CRT1 */

     u16     sisfb_tvxpos, sisfb_tvypos;     /* Warning: Values + 32 ! */

     u32     sisfb_heapsize;          /* heap size (in KB) */
     u32     sisfb_videooffset;     /* Offset of viewport in video memory (in bytes) */

     u32     sisfb_curfstn;          /* currently running FSTN/DSTN mode */
     u32     sisfb_curdstn;

     u16     sisfb_pci_vendor;     /* PCI vendor (SiS or XGI) */

     u32     sisfb_vbflags2;          /* ivideo->vbflags2 */

     u8      sisfb_can_post;          /* sisfb can POST this card */
     u8      sisfb_card_posted;     /* card is POSTED */
     u8      sisfb_was_boot_device;     /* This card was the boot video device (ie is primary) */

     u8      reserved[183];          /* for future use */
};

#define SISFB_GET_INFO_SIZE        _IOR(0xF3,0x00,u32)

#define SISFB_GET_INFO             _IOR(0xF3,0x01,struct sisfb_info)
#define SISFB_GET_AUTOMAXIMIZE     _IOR(0xF3,0x03,u32)
#define SISFB_SET_AUTOMAXIMIZE     _IOW(0xF3,0x03,u32)

#define SISFB_GET_INFO_OLD         _IOR('n',0xF8,u32)
#define SISFB_GET_AUTOMAXIMIZE_OLD _IOR('n',0xFA,u32)
#define SISFB_SET_AUTOMAXIMIZE_OLD _IOW('n',0xFA,u32)


#define SISFB_VERSION(a,b,c)     (((a) << 16) + ((b) << 8) + (c))


#endif /* _SIS315_COMPAT_H */
