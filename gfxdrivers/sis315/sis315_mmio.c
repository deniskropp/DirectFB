
#include "sis315_mmio.h"

__u32 sis_rl(volatile __u8 *mmio, unsigned int offset)
{
	return *(volatile __u32 *)(mmio + offset);
}

void sis_wl(volatile __u8 *mmio, unsigned int offset, __u32 value)
{
	*(volatile __u32 *)(mmio + offset) = value;
}

