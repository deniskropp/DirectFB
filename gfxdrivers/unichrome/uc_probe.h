/*
   Copyright (c) 2004 Andreas Robinson, All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

#ifndef __UC_PROBE_H__
#define __UC_PROBE_H__

#ifndef PCI_VENDOR_ID_VIA
#define PCI_VENDOR_ID_VIA 0x1106
#endif

#ifndef FB_ACCEL_VIA_UNICHROME
#define FB_ACCEL_VIA_UNICHROME 77
#endif

struct uc_via_chipinfo
{
#ifdef KERNEL
    u16 id;                 // PCI id
#else
    u16 id;
#endif
    char* name;             // Human readable name, e.g CLE266/UniChrome
};

static struct uc_via_chipinfo uc_via_devices[] = 
{
    {0x3122, "CLE266/UniChrome"},       // aka VT3122
    {0x7205, "KM400/UniChrome"},        // aka VT3205, P4M800
    {0x7204, "K8M800/UniChrome Pro"},   // aka VT3204, Unichrome Pro B
    {0x3118, "CN400/UniChrome Pro"},    // aka VT3259, PM8?0, Unichrome Pro A
    {0x3344, "CN700/Unichrome Pro"},    // aka VT3314, P4M800Pro, VN800, CN900
    {0, ""}
};

#endif /* __UC_PROBE_H__ */
