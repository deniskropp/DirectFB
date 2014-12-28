#ifndef __VSP1__VSP1_H__
#define __VSP1__VSP1_H__

#include <sys/ioctl.h>

#include <xf86drm.h>

#include "vsp1_types.h"


void vsp1_buffer_finished( VSP1DriverData *gdrv,
                           VSP1DeviceData *gdev,
                           VSP1Buffer     *buffer );

#endif
