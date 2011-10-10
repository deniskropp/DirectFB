/*
   (c) Copyright 2011  Denis Oliver Kropp <dok@directfb.org>

   All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version
   2 of the License, or (at your option) any later version.
   
   --
   
   Debugging utilities
*/

#include <linux/compiler.h>
#include <linux/ctype.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/stddef.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include <linux/time.h>
#include <linux/types.h>
#include <linux/version.h>

#include <asm/fcntl.h>
#include <asm/signal.h>
#include <asm/string.h>

#include "debug.h"



static unsigned long
direct_clock_get_micros( void )
{
     unsigned long       micros;
     struct timespec spec;

     ktime_get_real_ts( &spec );

     micros = spec.tv_sec * 1000000 + spec.tv_nsec / 1000;

     return micros;
}

__attribute__((no_instrument_function))
pid_t
direct_gettid( void )
{
#if LINUX_VERSION_CODE > KERNEL_VERSION( 2, 6, 18 )    // FIXME: which version?
     return task_pid_vnr(current);
#else
     return current->tgid;
#endif
}


void
one_debug_printk( const char *format, ... )
{
     char        buf[512];
     unsigned long   micros = direct_clock_get_micros();
     const char *name   = "NO NAME";

     va_list ap;

     va_start( ap, format );

     vsnprintf( buf, sizeof(buf), format, ap );

     va_end( ap );

     printk( KERN_DEBUG "(-) [%-16.16s %03lu] (%5d) %s: %*s%s",
             name, micros,
             direct_gettid(), "One", /*indent*/ 0, "", buf );
}

