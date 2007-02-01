// Definitions of framebuffer ioctls

#ifndef __UC_IOCTL_H__
#define __UC_IOCTL_H__

#include <fbdev/fbdev.h>
#include <sys/ioctl.h>
#include <dfb_types.h>

// Parameters for FBIO_FLIPONVSYNC ioctl
struct fb_flip {
    u32 device;
    u32 field;
    u32 count;
    u32 offset[6];
};

#define VIAFB_FLIP_GRAPHICS    0
#define VIAFB_FLIP_V1          1
#define VIAFB_FLIP_V3          2
#define VIAFB_FLIP_SPIC        3
#define VIAFB_FLIP_NOP         255

#ifndef FBIO_FLIPONVSYNC
#define FBIO_FLIPONVSYNC       _IOWR('F', 0x21, struct fb_flip)
#endif

// Parameters for FBIO_WAITFORVSYNC ioctl
#define VIAFB_WAIT_ANY         0
#define VIAFB_WAIT_TOPFIELD    1
#define VIAFB_WAIT_BOTTOMFIELD 2
#define VIAFB_WAIT_FLIP        3

#endif // __UC_IOCTL_H__

