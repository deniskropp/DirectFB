#ifndef _SIS315_MMIO_H
#define _SIS315_MMIO_H

#include <dfb_types.h>

extern __inline__ __u32 sis_rl(volatile __u8 *mmio, unsigned int offset);
extern __inline__ void sis_wl(volatile __u8 *mmio, unsigned int offset, __u32 value);

#endif
