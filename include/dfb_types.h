#ifndef __DFB_TYPES_H__
#define __DFB_TYPES_H__

#ifdef __linux__
#include <asm/types.h>
#else
#include <sys/types.h>
#define __u8  u_int8_t
#define __u16 u_int16_t
#define __u32 u_int32_t

#define __s16 int16_t
#endif

#endif
