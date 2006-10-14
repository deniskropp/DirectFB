#ifndef __DFB_TYPES_H__
#define __DFB_TYPES_H__


#ifdef USE_KOS

#include <sys/types.h>

typedef uint8 u8;
typedef uint16 u16;
typedef uint32 u32;
typedef uint64 u64;

typedef sint8 s8;
typedef sint16 s16;
typedef sint32 s32;
typedef sint64 s64;

#else

#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

#endif

#endif
