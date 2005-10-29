/*
   Copyright (c) 2003 Andreas Robinson, All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

#ifndef __UC_FIFO_H__
#define __UC_FIFO_H__

// Note to self: remove when added to makefile as -DUC_DEBUG.
#define UC_DEBUG 1

#include <dfb_types.h>

#include "regs2d.h"
#include "regs3d.h"
#include "mmio.h"

/**
 * uc_fifo - GPU data queue.
 *
 * buf:         buffer start (userspace address)
 * head:        pointer to first unused entry.
 *
 * size:        maximum number of entries in the fifo.
 * prep:        number of entries allocated to be used.
 * used:        number of entries currently in use.
 *
 * hwregs:      GPU register base address
 * reg_tset:    address to GPU TRANSET register
 * reg_tspace:  address to GPU TRANSPACE register
 *
 * flush:       function pointer to flush function (DMA or CPU)
 * flush_sys:   function pointer to flush_sys (non-DMA) function
 */

struct uc_fifo
{
    __u32* buf;
    __u32* head;

    unsigned int size;
    unsigned int prep;
    unsigned int used;

    //void (*flush)(struct uc_fifo* fifo, volatile void *hwregs);
    //void (*flush_sys)(struct uc_fifo* fifo, volatile void *hwregs);
};

// Help macros ---------------------------------------------------------------

// For the record: Macros suck maintenance- and debugging-wise,
// but provide guaranteed inlining of the code.

/**
 * Send the contents of the FIFO buffer to the hardware, and clear
 * the buffer. The transfer may be performed by the CPU or by DMA.
 */

//#define UC_FIFO_FLUSH(fifo) (fifo)->flush(fifo,ucdrv->hwregs)

/**
 * Same as UC_FIFO_FLUSH(), but always uses the CPU to transfer data.
 */

//#define UC_FIFO_FLUSH_SYS(fifo) (fifo)->flush_sys(fifo,ucdrv->hwregs)

#define UC_FIFO_FLUSH(fifo)     uc_fifo_flush_sys(fifo,ucdrv->hwregs)
#define UC_FIFO_FLUSH_SYS(fifo) uc_fifo_flush_sys(fifo,ucdrv->hwregs)

/**
 * Make sure there is room for dwsize double words in the FIFO.
 * If necessary, the FIFO is flushed first.
 *
 * @param fifo      the fifo
 * @param dwsize    number of double words to allocate
 *
 * @note It is ok to request more space than you will actually
 * be using. This is useful when you don't know exactly beforehand
 * how many entries you need.
 *
 * @note equivalent DRI code is in via_ioctl.c::viaCheckDma()
 */

#ifdef UC_DEBUG

#define UC_FIFO_PREPARE(fifo, dwsize)                            \
    do {                                                         \
        if ((fifo)->used + dwsize + 32 > (fifo)->size) {         \
            D_DEBUG("Unichrome: FIFO full - flushing it.");      \
            UC_FIFO_FLUSH(fifo);                                 \
        }                                                        \
        if (dwsize + (fifo)->prep + 32 > (fifo)->size) {         \
            D_BUG("Unichrome: FIFO too small for allocation.");  \
        }                                                        \
        (fifo)->prep += dwsize;                                  \
    } while(0)

#else

#define UC_FIFO_PREPARE(fifo, dwsize)                       \
    do {                                                    \
        if ((fifo)->used + dwsize + 32 > (fifo)->size) {    \
            UC_FIFO_FLUSH(fifo);                            \
        }                                                   \
        (fifo)->prep += dwsize;                             \
    } while(0)

#endif // UC_FIFO_DEBUG

/**
 * Add a 32-bit data word to the FIFO.
 * Takes one entry in the FIFO.
 */

#define UC_FIFO_ADD(fifo, data)     \
    do {                            \
        *((fifo)->head) = (data);   \
        (fifo)->head++;             \
        (fifo)->used++;             \
    } while(0)

/**
 * Add a command header. (HC_HEADER2 + parameter selection)
 * Takes two entries in the fifo.
 */

#define UC_FIFO_ADD_HDR(fifo, param)    \
    do {                                \
        UC_FIFO_ADD(fifo, HC_HEADER2);  \
        UC_FIFO_ADD(fifo, param);       \
    } while(0);

/**
 * Add a floating point value to the FIFO.
 * Non-floats (e.g integers) are converted first.
 * Takes one entry in the FIFO.
 */

#define UC_FIFO_ADD_FLOAT(fifo, val)        \
    do {                                    \
        union {float f; __u32 i;} v;        \
        v.f = (float) (val);                \
        UC_FIFO_ADD(fifo, v.i);             \
    } while(0)

/**
 * Add a vertex on the form (x, y, color) to the FIFO.
 * Takes three entries in the FIFO.
 * The color format is 0xAARRGGBB.
 */

#define UC_FIFO_ADD_XYC(fifo, x, y, color)  \
    do {                                    \
        UC_FIFO_ADD_FLOAT(fifo, x);         \
        UC_FIFO_ADD_FLOAT(fifo, y);         \
        UC_FIFO_ADD(fifo, color);           \
    } while(0)

/**
 * Add a vertex on the form (x, y, w, color, s, t) to the FIFO.
 * Takes six entries in the FIFO.
 * The color format is 0xAARRGGBB.
 */

#define UC_FIFO_ADD_XYWCST(fifo, x, y, w, color, s, t)  \
    do {                                                \
        UC_FIFO_ADD_FLOAT(fifo, x);                     \
        UC_FIFO_ADD_FLOAT(fifo, y);                     \
        UC_FIFO_ADD_FLOAT(fifo, w);                     \
        UC_FIFO_ADD(fifo, color);                       \
        UC_FIFO_ADD_FLOAT(fifo, s);                     \
        UC_FIFO_ADD_FLOAT(fifo, t);                     \
    } while(0)

#define UC_FIFO_ADD_XYZWCST(fifo, x, y, z, w, color, s, t)  \
    do {                                                \
        UC_FIFO_ADD_FLOAT(fifo, x);                     \
        UC_FIFO_ADD_FLOAT(fifo, y);                     \
        UC_FIFO_ADD_FLOAT(fifo, z);                     \
        UC_FIFO_ADD_FLOAT(fifo, w);                     \
        UC_FIFO_ADD(fifo, color);                       \
        UC_FIFO_ADD_FLOAT(fifo, s);                     \
        UC_FIFO_ADD_FLOAT(fifo, t);                     \
    } while(0)

#define UC_FIFO_ADD_XYCST(fifo, x, y, color, s, t)      \
    do {                                                \
        UC_FIFO_ADD_FLOAT(fifo, x);                     \
        UC_FIFO_ADD_FLOAT(fifo, y);                     \
        UC_FIFO_ADD(fifo, color);                       \
        UC_FIFO_ADD_FLOAT(fifo, s);                     \
        UC_FIFO_ADD_FLOAT(fifo, t);                     \
    } while(0)


/**
 * Add data specifically for the 2D controller, to the fifo.
 * Takes two entries in the FIFO.
 *
 * @param reg   2D register index
 * @param data  32-bit data to add
 */

#define UC_FIFO_ADD_2D(fifo, reg, data)                     \
    do {                                                    \
        UC_FIFO_ADD(fifo, ((reg) >> 2) | HALCYON_HEADER1);  \
        UC_FIFO_ADD(fifo, (data));                          \
    } while (0)

/**
 * Add data specifically for a 3D controller register, to the fifo.
 * Takes one entry in the FIFO.
 *
 * @param reg   3D register index (8 bit)
 * @param data  24-bit data to add (make sure bits 24 - 31 are cleared!)
 */

#define UC_FIFO_ADD_3D(fifo, reg, data) \
    UC_FIFO_ADD(fifo, ((reg) << 24) | (data))

/**
 * Pad the FIFO to an even number of entries.
 * Takes zero or one entries in the FIFO.
 */
#define UC_FIFO_PAD_EVEN(fifo)  \
        if (fifo->used & 1) UC_FIFO_ADD(fifo, HC_DUMMY)

/**
 * Check for buffer overruns.
 * Can be redefined to nothing in release builds.
 */

#ifdef UC_DEBUG

#define UC_FIFO_CHECK(fifo)                              \
    do {                                                 \
        if ((fifo)->used > ((fifo)->size) - 32) {        \
            D_BUG("Unichrome: FIFO overrun.");           \
        }                                                \
        if ((fifo)->used > (fifo)->prep) {               \
            D_BUG("Unichrome: FIFO allocation error.");  \
        }                                                \
    } while(0)

#else

#define UC_FIFO_CHECK(fifo) do { } while(0)

#endif // UC_DEBUG


// FIFO functions ------------------------------------------------------------

/** Create a FIFO. Returns NULL on failure. */

struct uc_fifo* uc_fifo_create( FusionSHMPoolShared *pool, size_t size);

/** Destroy a FIFO */

void uc_fifo_destroy(FusionSHMPoolShared *pool, struct uc_fifo* fifo);

void uc_fifo_flush_sys(struct uc_fifo* fifo, volatile void *regs);

#endif // __UC_FIFO_H__
