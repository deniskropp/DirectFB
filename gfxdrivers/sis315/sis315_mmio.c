
#include "sis315_mmio.h"

__u8 sis_rb(volatile __u8 *mmio, unsigned int offset)
{
	return *(volatile __u8 *)(mmio + offset);
}

__u16 sis_rw(volatile __u8 *mmio, unsigned int offset)
{
	return *(volatile __u16 *)(mmio + offset);
}

__u32 sis_rl(volatile __u8 *mmio, unsigned int offset)
{
	return *(volatile __u32 *)(mmio + offset);
}

void sis_wb(volatile __u8 *mmio, unsigned int offset, __u8 value)
{
	*(volatile __u8 *)(mmio + offset) = value;
}

void sis_ww(volatile __u8 *mmio, unsigned int offset, __u16 value)
{
	*(volatile __u16 *)(mmio + offset) = value;
}

void sis_wl(volatile __u8 *mmio, unsigned int offset, __u32 value)
{
	*(volatile __u32 *)(mmio + offset) = value;
}

