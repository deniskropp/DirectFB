/*
   Copyright (c) 2003 Andreas Robinson, All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

#include <stdio.h>
#include <stdlib.h>

#include <fusion/shmalloc.h>

#include "uc_fifo.h"

//#define UC_FIFO_DUMP_DATA

// Private functions ---------------------------------------------------------

/**
 * Pad the FIFO buffer to a 32 byte boundary. Used by uc_flush_agp().
 * @note Equivalent DRI code is in via_ioctl::viaFlushPrimsLocked()
 */

static void uc_fifo_pad(struct uc_fifo* fifo)
{
    switch (fifo->used & 0x7)
    {
    case 0:
        break;
    case 2:
        UC_FIFO_ADD(fifo, HALCYON_HEADER2);
        UC_FIFO_ADD(fifo, HC_ParaType_NotTex << 16);
        UC_FIFO_ADD(fifo, HC_DUMMY);
        UC_FIFO_ADD(fifo, HC_DUMMY);
        UC_FIFO_ADD(fifo, HC_DUMMY);
        UC_FIFO_ADD(fifo, HC_DUMMY);
        break;
    case 4:
        UC_FIFO_ADD(fifo, HALCYON_HEADER2);
        UC_FIFO_ADD(fifo, HC_ParaType_NotTex << 16);
        UC_FIFO_ADD(fifo, HC_DUMMY);
        UC_FIFO_ADD(fifo, HC_DUMMY);
        break;
    case 6:
        UC_FIFO_ADD(fifo, HALCYON_HEADER2);
        UC_FIFO_ADD(fifo, HC_ParaType_NotTex << 16);
        UC_FIFO_ADD(fifo, HC_DUMMY);
        UC_FIFO_ADD(fifo, HC_DUMMY);
        UC_FIFO_ADD(fifo, HC_DUMMY);
        UC_FIFO_ADD(fifo, HC_DUMMY);
        UC_FIFO_ADD(fifo, HC_DUMMY);
        UC_FIFO_ADD(fifo, HC_DUMMY);
        UC_FIFO_ADD(fifo, HC_DUMMY);
        UC_FIFO_ADD(fifo, HC_DUMMY);
        break;
    default:
        break;
    }
}

/**
 * Manually write the FIFO buffer to the hardware.
 * @note Equivalent DRI code is in via_ioctl::flush_sys()
 */

void uc_fifo_flush_sys(struct uc_fifo* fifo, volatile void *regs)
{
    __u32* p;
    __u32* q;

    volatile __u32* hwregs     = regs;
    volatile __u32* reg_tset   = regs + VIA_REG_TRANSET;
    volatile __u32* reg_tspace = regs + VIA_REG_TRANSPACE;

    int check2Dcmd;
    __u32 addr;

    p = fifo->buf;
    q = fifo->head;
    check2Dcmd = 0;

    uc_fifo_pad(fifo);

#ifdef UC_FIFO_DUMP_DATA
    printf("Flushing FIFO ... \n");
#endif

    while (p != q) {

        if (*p == HALCYON_HEADER2) {
            p++;
            check2Dcmd = !(*p == HALCYON_SUB_ADDR0);
#ifdef UC_FIFO_DUMP_DATA
            printf("tset = 0x%08x\n", *p);
#endif
            *reg_tset = *p;
            p++;
        }
        else if (check2Dcmd && ((*p & HALCYON_HEADER1MASK) == HALCYON_HEADER1)) {
            addr = (*p) & 0x0000001f;
            p++;
#ifdef UC_FIFO_DUMP_DATA
            printf("2D (0x%02x) = 0x%x\n", addr << 2, *p);
#endif
            *(hwregs + addr) = *p;
            p++;
        }
        else if ((*p & HALCYON_FIREMASK) == HALCYON_FIRECMD) {
#ifdef UC_FIFO_DUMP_DATA
            printf("tspace = 0x%08x\n", *p);
#endif
            *reg_tspace = *p;
            p++;

            if ((p != q) && ((*p & HALCYON_FIREMASK) == HALCYON_FIRECMD))
                p++;

            if ((*p & HALCYON_CMDBMASK) != HC_ACMD_HCmdB)
                check2Dcmd = 1;
        }
        else {
#ifdef UC_FIFO_DUMP_DATA
            printf("tspace = 0x%08x\n", *p);
#endif
            *reg_tspace = *p;
            p++;
        }
    }

    fifo->head = fifo->buf;
    fifo->used = 0;
    fifo->prep = 0;
}

/** Use an AGP transfer to write the FIFO buffer to the hardware. Not implemented. */
#if 0
static void uc_fifo_flush_agp(struct uc_fifo* fifo)
{
    // TODO - however, there is no point in doing this, because
    // an AGP transfer can require more register writes than
    // needed for drawing a single primitive. DirectFB needs to
    // adopt a begin/end architecture first, like OpenGL has.

    fifo->head = fifo->buf;
    fifo->used = 0;
    fifo->prep = 0;
}
#endif

// Public functions ----------------------------------------------------------

/** Create a FIFO. Returns NULL on failure. */

struct uc_fifo* uc_fifo_create(size_t size)
{
    struct uc_fifo* fifo;

    size += 32;     // Needed for padding.

    fifo = SHCALLOC(1, sizeof(struct uc_fifo));
    if (!fifo) return NULL;

    // Note: malloc won't work for DMA buffers...

    fifo->buf = SHMALLOC(sizeof(__u32) * size);
    if (!(fifo->buf)) {
        SHFREE(fifo);
        return NULL;
    }

    fifo->head = fifo->buf;
    fifo->used = 0;
    fifo->size = (unsigned int) size;
    fifo->prep = 0;

    //fifo->flush_sys = uc_fifo_flush_sys;

    //fifo->flush = uc_fifo_flush_sys;

    return fifo;
}

/** Destroy a FIFO */

void uc_fifo_destroy(struct uc_fifo* fifo)
{
    if (fifo) {
        if (fifo->buf) {
            SHFREE(fifo->buf);
            fifo->buf = NULL;
        }
        SHFREE(fifo);
    }
}
