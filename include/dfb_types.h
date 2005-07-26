#ifndef __DFB_TYPES_H__
#define __DFB_TYPES_H__

#include <sys/types.h>

#ifndef USE_KOS

#define __u8  u_int8_t
#define __u16 u_int16_t
#define __u32 u_int32_t
#define __u64 u_int64_t

#define __s8  int8_t
#define __s16 int16_t
#define __s32 int32_t
#define __s64 int64_t

#else

#define __u8  uint8
#define __u16 uint16
#define __u32 uint32
#define __u64 uint64

#define __s8  sint8
#define __s16 sint16
#define __s32 sint32
#define __s64 sint64

#endif

#endif
