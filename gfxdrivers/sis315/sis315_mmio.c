
#include "sis315_mmio.h"

u32 sis_rl(volatile u8 *mmio, unsigned int offset)
{
	return *(volatile u32 *)(mmio + offset);
}

void sis_wl(volatile u8 *mmio, unsigned int offset, u32 value)
{
	*(volatile u32 *)(mmio + offset) = value;
}

