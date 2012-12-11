/*
   (c) Copyright 2001-2008  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifndef __DIRECT__ATOMIC_H__
#define __DIRECT__ATOMIC_H__

#include <direct/types.h>

#if defined (__SH4__) || defined (__SH4A__)

/*
 * SH4 Atomic Operations
 */

#define D_SYNC_BOOL_COMPARE_AND_SWAP( ptr, old_value, new_value )               \
     ({                                                                         \
          __typeof__(*(ptr)) __temp;                                                \
          __typeof__(*(ptr)) __result;                                              \
                                                                                \
          __asm__ __volatile__ (                                                \
               "1:                                                    \n"       \
               "    movli.l        @%2, %0                            \n"       \
               "    mov            %0, %1                             \n"       \
               "    cmp/eq         %1, %3                             \n"       \
               "    bf             2f                                 \n"       \
               "    mov            %4, %0                             \n"       \
               "2:                                                    \n"       \
               "    movco.l        %0, @%2                            \n"       \
               "    bf             1b                                 \n"       \
               "    synco                                             \n"       \
               : "=&z" (__temp), "=&r" (__result)                               \
               : "r" (ptr), "r" (old_value), "r" (new_value)                    \
               : "t"                                                            \
          );                                                                    \
                                                                                \
          __result == (old_value);                                              \
     })


#define D_SYNC_ADD_AND_FETCH( ptr, value )                                      \
     ({                                                                         \
          __typeof__(*(ptr)) __result;                                              \
                                                                                \
          __asm__ __volatile__ (                                                \
               "1:                                                    \n"       \
               "    movli.l        @%1, %0                            \n"       \
               "    add            %2, %0                             \n"       \
               "    movco.l        %0, @%1                            \n"       \
               "    bf             1b                                 \n"       \
               "    synco                                             \n"       \
               : "=&z" (__result)                                               \
               : "r" (ptr), "r" (value)                                         \
               : "t"                                                            \
          );                                                                    \
                                                                                \
          __result;                                                             \
     })


#define D_SYNC_FETCH_AND_CLEAR( ptr )                                           \
     ({                                                                         \
          __typeof__(*(ptr)) __temp;                                                \
          __typeof__(*(ptr)) __result;                                              \
                                                                                \
          __asm__ __volatile__ (                                                \
               "1:                                                    \n"       \
               "    movli.l        @%2, %0                            \n"       \
               "    mov            %0, %1                             \n"       \
               "    xor            %0, %0                             \n"       \
               "    movco.l        %0, @%2                            \n"       \
               "    bf             1b                                 \n"       \
               "    synco                                             \n"       \
               : "=&z" (__temp), "=&r" (__result)                               \
               : "r" (ptr)                                                      \
               : "t"                                                            \
          );                                                                    \
                                                                                \
          __result;                                                             \
     })


#define D_SYNC_ADD( ptr, value )                                                \
     do {                                                                       \
          __typeof__(*(ptr)) __temp;                                                \
                                                                                \
          __asm__ __volatile__ (                                                \
               "1:                                                    \n"       \
               "    movli.l        @%1, %0                            \n"       \
               "    add            %2, %0                             \n"       \
               "    movco.l        %0, @%1                            \n"       \
               "    bf             1b                                 \n"       \
               : "=&z" (__temp)                                                 \
               : "r" (ptr), "r" (value)                                         \
               : "t"                                                            \
          );                                                                    \
     } while (0)

/*
 * FIFO Push
 *
 *   *iptr = *fptr
 *   *fptr =  iptr
 */
#define D_SYNC_PUSH_SINGLE( fptr, iptr )                                        \
     do {                                                                       \
          unsigned long **__fptr = (unsigned long **)(fptr);                    \
          unsigned long **__iptr = (unsigned long **)(iptr);                    \
          unsigned long  *__temp;                                               \
                                                                                \
          __asm__ __volatile__ (                                                \
               "1:                                                    \n"       \
               "    movli.l        @%1, %0                            \n"       \
               "    mov.l          %0, @%2                            \n"       \
               "    mov            %2, %0                             \n"       \
               "    movco.l        %0, @%1                            \n"       \
               "    bf             1b                                 \n"       \
               "    synco                                             \n"       \
               : "=&z" (__temp)                                                 \
               : "r" (__fptr), "r" (__iptr)                                     \
               : "t"                                                            \
          );                                                                    \
     } while (0)

/*
 * FIFO Pop
 *
 *    iptr = *fptr
 *   *fptr = *iptr  <- if iptr != NULL
 *
 *   return iptr
 */
#define D_SYNC_POP_SINGLE( fptr )                                               \
     ({                                                                         \
          unsigned long **__fptr = (unsigned long **)(fptr);                    \
          unsigned long **__iptr;                                               \
          unsigned long  *__temp;                                               \
                                                                                \
          __asm__ __volatile__ (                                                \
               "1:                                                    \n"       \
               "    movli.l        @%2, %0                            \n"       \
               "    mov            %0, %1                             \n"       \
               "    cmp/eq         #0, %0                             \n"       \
               "    bt             2f                                 \n"       \
               "    mov.l          @%1, %0                            \n"       \
               "2:                                                    \n"       \
               "    movco.l        %0, @%2                            \n"       \
               "    bf             1b                                 \n"       \
               "    synco                                             \n"       \
               : "=&z" (__temp), "=&r" (__iptr)                                 \
               : "r" (__fptr)                                                   \
               : "t"                                                            \
          );                                                                    \
                                                                                \
          (__typeof__(*(fptr))) __iptr;                                             \
     })


#endif


#if defined(ARCH_ARMv7) && !defined(ARCH_IWMMXT)

static inline int _D__atomic_cmpxchg(volatile int *ptr, int old, int _new)
{
	unsigned long oldval, res;

	do {
		__asm__ __volatile__("@ atomic_cmpxchg\n"
		"ldrex	%1, [%2]\n"
		"mov	%0, #0\n"
		"teq	%1, %3\n"
		"strexeq %0, %4, [%2]\n"
		    : "=&r" (res), "=&r" (oldval)
		    : "r" (ptr), "Ir" (old), "r" (_new)
		    : "cc");
	} while (res);

	return oldval;
}

#define D_SYNC_BOOL_COMPARE_AND_SWAP( ptr, old_value, new_value )                    \
     (_D__atomic_cmpxchg( (void*) ptr, (int) old_value, (int) new_value ) == (int) old_value)

#define D_SYNC_FETCH_AND_CLEAR( ptr )                                                \
     ({                                                                              \
          volatile __typeof__((ptr)) __ptr = (ptr);                                                     \
          __typeof__(*(ptr)) __temp;                                                     \
                                                                                     \
          do {                                                                       \
               __temp = (*__ptr);                                                      \
          } while (!D_SYNC_BOOL_COMPARE_AND_SWAP( __ptr, __temp, 0 ));                 \
                                                                                     \
          __temp;                                                                    \
     })

static inline int _D__atomic_add_return(int i, volatile int *v)
{
	unsigned long tmp;
	int result;

	__asm__ __volatile__("@ atomic_add_return\n"
"1:	ldrex	%0, [%2]\n"
"	add	%0, %0, %3\n"
"	strex	%1, %0, [%2]\n"
"	teq	%1, #0\n"
"	bne	1b"
	: "=&r" (result), "=&r" (tmp)
	: "r" (v), "Ir" (i)
	: "cc");

	return result;
}

#define D_SYNC_ADD_AND_FETCH( ptr, value )                                           \
     (_D__atomic_add_return( (int) (value), (volatile int*) (ptr) ))

#endif



#if defined(ARCH_MIPS)

static inline int _D__atomic_cmpxchg(volatile int *ptr, int old, int _new)
{
	unsigned long retval;

        __asm__ __volatile__(
              "	.set	push					\n"
              "	.set	noat					\n"
              "	.set	mips3					\n"
              "1:	ll	%0, %2			# __cmpxchg_u32	\n"
              "	bne	%0, %z3, 2f				\n"
              "	.set	mips0					\n"
              "	move	$1, %z4					\n"
              "	.set	mips3					\n"
              "	sc	$1, %1					\n"
              "	beqz	$1, 1b					\n"
#ifdef CONFIG_SMP
              "	sync						\n"
#endif
              "2:							\n"
              "	.set	pop					\n"
              : "=&r" (retval), "=R" (*ptr)
              : "R" (*ptr), "Jr" (old), "Jr" (_new)
              : "memory");

	return retval;
}

#define D_SYNC_BOOL_COMPARE_AND_SWAP( ptr, old_value, new_value )                    \
     (_D__atomic_cmpxchg( (void*) ptr, (int) old_value, (int) new_value ) == (int) old_value)

#define D_SYNC_FETCH_AND_CLEAR( ptr )                                                \
     ({                                                                              \
          volatile __typeof__((ptr)) __ptr = (ptr);                                                     \
          __typeof__(*(ptr)) __temp;                                                     \
                                                                                     \
          do {                                                                       \
               __temp = (*__ptr);                                                      \
          } while (!D_SYNC_BOOL_COMPARE_AND_SWAP( __ptr, __temp, 0 ));                 \
                                                                                     \
          __temp;                                                                    \
     })

static inline int _D__atomic_add_return(int i, volatile int *v)
{
     int temp;
     
     __asm__ __volatile__(
     "	.set	mips3					\n"
     "1:	ll	%0, %1		# atomic_add		\n"
     "	addu	%0, %2					\n"
     "	sc	%0, %1					\n"
     "	beqz	%0, 2f					\n"
     "	.subsection 2					\n"
     "2:	b	1b					\n"
     "	.previous					\n"
     "	.set	mips0					\n"
     : "=&r" (temp), "=m" (*v)
     : "Ir" (i), "m" (*v));

     return temp;
}

#define D_SYNC_ADD_AND_FETCH( ptr, value )                                           \
     _D__atomic_add_return( (int) (value), (volatile int*) (ptr) )

#endif



#ifdef WIN32

/*
 * Win32
 */

#ifndef D_SYNC_BOOL_COMPARE_AND_SWAP
#define D_SYNC_BOOL_COMPARE_AND_SWAP( ptr, old_value, new_value )     \
	 0
#endif

#ifndef D_SYNC_FETCH_AND_CLEAR
#define D_SYNC_FETCH_AND_CLEAR( ptr )                                 \
	 0
#endif

#ifndef D_SYNC_ADD_AND_FETCH
#define D_SYNC_ADD_AND_FETCH( ptr, value )                            \
	 0
#endif

#ifndef D_SYNC_ADD
#define D_SYNC_ADD( ptr, value )                                      \
	 0
#endif

#else //WIN32

#ifndef D_SYNC_BOOL_COMPARE_AND_SWAP
#define D_SYNC_BOOL_COMPARE_AND_SWAP( ptr, old_value, new_value )     \
     __sync_bool_compare_and_swap( ptr, old_value, new_value )
#endif

#ifndef D_SYNC_FETCH_AND_CLEAR
#define D_SYNC_FETCH_AND_CLEAR( ptr )                                 \
     __sync_fetch_and_and( ptr, 0 )
#endif

#ifndef D_SYNC_ADD_AND_FETCH
#define D_SYNC_ADD_AND_FETCH( ptr, value )                                      \
     ({                                                                         \
          int           __val;                                                  \
          volatile int *__ptr = (volatile int *)(void*)(ptr);                   \
                                                                                \
          do {                                                                  \
               __val = *__ptr;                                                  \
          } while (!D_SYNC_BOOL_COMPARE_AND_SWAP( __ptr, __val, __val + value ));   \
                                                                                \
          __val + value;                                                        \
     })
//     __sync_add_and_fetch( ptr, value )
#endif

#ifndef D_SYNC_ADD
#define D_SYNC_ADD( ptr, value )                                      \
     do { (void) D_SYNC_ADD_AND_FETCH( ptr, value ); } while (0)
#endif

#endif //!WIN32

/*
 * FIFO Push
 *
 *   *iptr = *fptr
 *   *fptr =  iptr
 */

#ifndef D_SYNC_PUSH_SINGLE
#define D_SYNC_PUSH_SINGLE( fptr, iptr )                                        \
     do {                                                                       \
          volatile long **__fptr = (volatile long **)(void*)(fptr);             \
          volatile long **__iptr = (volatile long **)(void*)(iptr);             \
                                                                                \
          do {                                                                  \
               *__iptr = *__fptr;                                               \
          } while (!D_SYNC_BOOL_COMPARE_AND_SWAP( __fptr, *__iptr, __iptr ));   \
     } while (0)
#endif

#ifndef D_SYNC_PUSH_MULTI
#define D_SYNC_PUSH_MULTI( fptr, iptr )                                         \
     do {                                                                       \
          volatile long **__fptr = (volatile long **)(void*)(fptr);             \
          volatile long **__iptr = (volatile long **)(void*)(iptr);             \
          volatile unsigned int   n = 1;                                        \
          unsigned int            r = 0;                                        \
                                                                                \
          while (true) {                                                        \
               *__iptr = *__fptr;                                               \
                                                                                \
               if (D_SYNC_BOOL_COMPARE_AND_SWAP( __fptr, *__iptr, __iptr ))     \
                    break;                                                      \
                                                                                \
               r = ((r + n) & 0x7f);                                            \
                                                                                \
               for (n=0; n<r; n++);                                             \
          }                                                                     \
     } while (0)
#endif


/*
 * FIFO Pop
 *
 *    iptr = *fptr
 *   *fptr = *iptr
 *
 *   return iptr
 */

#ifndef D_SYNC_POP_SINGLE

#ifdef WIN32
#define D_SYNC_POP_SINGLE( fptr )                                                         \
     0
#else
#define D_SYNC_POP_SINGLE( fptr )                                                         \
     ({                                                                                   \
          volatile long **__fptr = (volatile long**)(void*)(fptr);                        \
          volatile long  *__iptr;                                                         \
                                                                                          \
          do {                                                                            \
               __iptr = *__fptr;                                                          \
          } while (__iptr && !D_SYNC_BOOL_COMPARE_AND_SWAP( __fptr, __iptr, *__iptr ));   \
                                                                                          \
          (__typeof__(*(fptr))) __iptr;                                                   \
     })
#endif
#endif

#ifndef D_SYNC_POP_MULTI
#define D_SYNC_POP_MULTI( fptr )                                                \
     ({                                                                         \
          volatile long         **__fptr = (volatile long**)(void*)(fptr);      \
          volatile long          *__iptr;                                       \
          volatile int           n = 1;                                         \
          volatile int           r = 0;                                         \
                                                                                \
          while (true) {                                                        \
               __iptr = *__fptr;                                                \
                                                                                \
               if (D_SYNC_BOOL_COMPARE_AND_SWAP( __fptr, __iptr, *__iptr ))     \
                    break;                                                      \
                                                                                \
               r = ((r + n) & 0x7f);                                            \
                                                                                \
               for (n=0; n<r; n++);                                             \
          }                                                                     \
                                                                                \
          (__typeof__(*(fptr))) __iptr;                                         \
     })
#endif


#ifndef D_SYNC_PUSH
#if DIRECT_BUILD_MULTICORE
#define D_SYNC_PUSH D_SYNC_PUSH_MULTI
#else
#define D_SYNC_PUSH D_SYNC_PUSH_SINGLE
#endif
#endif

#ifndef D_SYNC_POP
#if DIRECT_BUILD_MULTICORE
#define D_SYNC_POP D_SYNC_POP_MULTI
#else
#define D_SYNC_POP D_SYNC_POP_SINGLE
#endif
#endif


#endif

