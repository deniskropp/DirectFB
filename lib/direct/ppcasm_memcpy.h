#ifndef __DIRECT__PPCASM_MEMCPY_H__
#define __DIRECT__PPCASM_MEMCPY_H__

void *direct_ppcasm_cacheable_memcpy( void *dest, const void *src, size_t n);
void *direct_ppcasm_memcpy          ( void *dest, const void *src, size_t n);

#endif
