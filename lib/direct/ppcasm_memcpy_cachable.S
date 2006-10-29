/*
 * String handling functions for PowerPC.
 *
 * Copyright (C) 1996 Paul Mackerras.
 *
 *
 * In a mail from Paul on 23.10.2006 05:47:
 *
 * You may put an LGPL permission statement on that code, replacing the
 * GPL permission statement.  From a technical point of view, I'm not
 * sure that the code in ppcasm_memcpy_cachable.S is the best thing to
 * use in userspace, though; for one thing, it has a cache line size
 * assumption encoded into it.  Why don't you just use the glibc memcpy?
 * It's pretty well optimized these days, AFAIK.
 * 
 * Paul.
 *
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#define __ASSEMBLY__

#include <linux/config.h>

#if defined(CONFIG_8xx) || defined(CONFIG_403GCX)
#define L1_CACHE_LINE_SIZE       16
#define LG_L1_CACHE_LINE_SIZE     4 
#elif defined(CONFIG_PPC64BRIDGE) 
#define L1_CACHE_LINE_SIZE      128
#define LG_L1_CACHE_LINE_SIZE     7  
#else
#define L1_CACHE_LINE_SIZE       32
#define LG_L1_CACHE_LINE_SIZE     5
#endif

#include "ppc_asm.h"

#define COPY_16_BYTES		\
	lwz	r7,4(r4);	\
	lwz	r8,8(r4);	\
	lwz	r9,12(r4);	\
	lwzu	r10,16(r4);	\
	stw	r7,4(r6);	\
	stw	r8,8(r6);	\
	stw	r9,12(r6);	\
	stwu	r10,16(r6)

#define COPY_16_BYTES_WITHEX(n)	\
8 ## n ## 0:			\
	lwz	r7,4(r4);	\
8 ## n ## 1:			\
	lwz	r8,8(r4);	\
8 ## n ## 2:			\
	lwz	r9,12(r4);	\
8 ## n ## 3:			\
	lwzu	r10,16(r4);	\
8 ## n ## 4:			\
	stw	r7,4(r6);	\
8 ## n ## 5:			\
	stw	r8,8(r6);	\
8 ## n ## 6:			\
	stw	r9,12(r6);	\
8 ## n ## 7:			\
	stwu	r10,16(r6)

#define COPY_16_BYTES_EXCODE(n)			\
9 ## n ## 0:					\
	addi	r5,r5,-(16 * n);		\
	b	104f;				\
9 ## n ## 1:					\
	addi	r5,r5,-(16 * n);		\
	b	105f;				\
.section __ex_table,"a";			\
	.align	2;				\
	.long	8 ## n ## 0b,9 ## n ## 0b;	\
	.long	8 ## n ## 1b,9 ## n ## 0b;	\
	.long	8 ## n ## 2b,9 ## n ## 0b;	\
	.long	8 ## n ## 3b,9 ## n ## 0b;	\
	.long	8 ## n ## 4b,9 ## n ## 1b;	\
	.long	8 ## n ## 5b,9 ## n ## 1b;	\
	.long	8 ## n ## 6b,9 ## n ## 1b;	\
	.long	8 ## n ## 7b,9 ## n ## 1b;	\
	.text

	.text


CACHELINE_MASK = (L1_CACHE_LINE_SIZE - 1)

	.global	direct_ppcasm_cacheable_memcpy
direct_ppcasm_cacheable_memcpy:
#if 0 /* this part causes "error loading shared library: unexpected reloc type
         0x0b (???) */
	add	r7,r3,r5		/* test if the src & dst overlap */
	add	r8,r4,r5
	cmplw	0,r4,r7
	cmplw	1,r3,r8
	crand	0,0,4			/* cr0.lt &= cr1.lt */
	blt	ppcasm_memcpy			/* if regions overlap */
#endif
	addi	r4,r4,-4
	addi	r6,r3,-4
	neg	r0,r3
	andi.	r0,r0,CACHELINE_MASK	/* # bytes to start of cache line */
	beq	58f

	cmplw	0,r5,r0			/* is this more than total to do? */
	blt	63f			/* if not much to do */
	andi.	r8,r0,3			/* get it word-aligned first */
	subf	r5,r0,r5
	mtctr	r8
	beq+	61f
70:	lbz	r9,4(r4)		/* do some bytes */
	stb	r9,4(r6)
	addi	r4,r4,1
	addi	r6,r6,1
	bdnz	70b
61:	srwi.	r0,r0,2
	mtctr	r0
	beq	58f
72:	lwzu	r9,4(r4)		/* do some words */
	stwu	r9,4(r6)
	bdnz	72b

58:	srwi.	r0,r5,LG_L1_CACHE_LINE_SIZE /* complete cachelines */
	clrlwi	r5,r5,32-LG_L1_CACHE_LINE_SIZE
	li	r11,4
	mtctr	r0
	beq	63f
53:
#if !defined(CONFIG_8xx)
	dcbz	r11,r6
#endif
	COPY_16_BYTES
#if L1_CACHE_LINE_SIZE >= 32
	COPY_16_BYTES
#if L1_CACHE_LINE_SIZE >= 64
	COPY_16_BYTES
	COPY_16_BYTES
#if L1_CACHE_LINE_SIZE >= 128
	COPY_16_BYTES
	COPY_16_BYTES
	COPY_16_BYTES
	COPY_16_BYTES
#endif
#endif
#endif
	bdnz	53b

63:	srwi.	r0,r5,2
	mtctr	r0
	beq	64f
30:	lwzu	r0,4(r4)
	stwu	r0,4(r6)
	bdnz	30b

64:	andi.	r0,r5,3
	mtctr	r0
	beq+	65f
40:	lbz	r0,4(r4)
	stb	r0,4(r6)
	addi	r4,r4,1
	addi	r6,r6,1
	bdnz	40b
65:	blr

