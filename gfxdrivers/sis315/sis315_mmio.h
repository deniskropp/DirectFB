#ifndef _SIS315_MMIO_H
#define _SIS315_MMIO_H

#include <asm/types.h>

extern __inline__ __u8 sis_rb(volatile __u8 *mmio, unsigned int offset);
extern __inline__ __u16 sis_rw(volatile __u8 *mmio, unsigned int offset);
extern __inline__ __u32 sis_rl(volatile __u8 *mmio, unsigned int offset);

extern __inline__ void sis_wb(volatile __u8 *mmio, unsigned int offset, __u8 value);
extern __inline__ void sis_ww(volatile __u8 *mmio, unsigned int offset, __u16 value);
extern __inline__ void sis_wl(volatile __u8 *mmio, unsigned int offset, __u32 value);

#endif
