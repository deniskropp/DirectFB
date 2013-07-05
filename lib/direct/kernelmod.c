/*
    libdirect Kernel Module
    Part of DirectFB

   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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

#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/version.h>
#ifdef HAVE_LINUX_CONFIG_H
#include <linux/config.h>
#endif
#include <linux/types.h>
#include <linux/kernel.h>


#include <direct/clock.h>
#include <direct/conf.h>
#include <direct/debug.h>
#include <direct/direct.h>
#include <direct/fifo.h>
#include <direct/filesystem.h>
#include <direct/hash.h>
#include <direct/interface.h>
#include <direct/log.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/print.h>
#include <direct/processor.h>
#include <direct/result.h>
#include <direct/serial.h>
#include <direct/signals.h>
#include <direct/system.h>
#include <direct/thread.h>
#include <direct/trace.h>
#include <direct/tree.h>
#include <direct/util.h>


D_LOG_DOMAIN( Direct_Module, "Direct/Module", "Initialization and shutdown of libdirect Kernel Module" );

/**********************************************************************************************************************/

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Denis Oliver Kropp <dok@directfb.org>");

MODULE_DESCRIPTION("libdirect");
MODULE_VERSION("1.5.0");

/**********************************************************************************************************************/

// ...
extern int  DirectResult__ctor(void);
extern void DirectResult__dtor(void);

/**********************************************************************************************************************/

int __init
libdirect_init(void)
{
     DirectResult__ctor();

     D_DEBUG_AT( Direct_Module, "%s()\n", __FUNCTION__ );

     D_INFO( "Direct/Module: Initializing module...\n" );

     return 0;
}

void __exit
libdirect_exit(void)
{
     D_DEBUG_AT( Direct_Module, "%s()\n", __FUNCTION__ );

     D_INFO( "Direct/Module: Exiting module...\n" );

     DirectResult__dtor();
}

/**********************************************************************************************************************/

module_init(libdirect_init);
module_exit(libdirect_exit);

/**********************************************************************************************************************/
/**********************************************************************************************************************/

/*****************************************************
 * EXPORT TABLE FOR ALL SYMBOLS...                   *
 *****************************************************/

EXPORT_SYMBOL_GPL( DirectResultTypeRegister );
EXPORT_SYMBOL_GPL( DirectResultTypeUnregister );

EXPORT_SYMBOL_GPL( DirectResultString );

EXPORT_SYMBOL_GPL( DirectGetInterface );
EXPORT_SYMBOL_GPL( DirectProbeInterface );
EXPORT_SYMBOL_GPL( DirectRegisterInterface );
EXPORT_SYMBOL_GPL( DirectUnregisterInterface );


EXPORT_SYMBOL_GPL( direct_initialize );
EXPORT_SYMBOL_GPL( direct_shutdown );

EXPORT_SYMBOL_GPL( direct_clock_get_abs_micros );
EXPORT_SYMBOL_GPL( direct_clock_get_abs_millis );
EXPORT_SYMBOL_GPL( direct_clock_get_micros );
EXPORT_SYMBOL_GPL( direct_clock_get_millis );
EXPORT_SYMBOL_GPL( direct_clock_get_time );
EXPORT_SYMBOL_GPL( direct_clock_resolution );
EXPORT_SYMBOL_GPL( direct_clock_set_time );

EXPORT_SYMBOL_GPL( direct_cleanup_handler_add );
EXPORT_SYMBOL_GPL( direct_cleanup_handler_remove );

EXPORT_SYMBOL_GPL( direct_config );
EXPORT_SYMBOL_GPL( direct_config_set );

#if DIRECT_BUILD_DEBUGS
EXPORT_SYMBOL_GPL( direct_assertion );
EXPORT_SYMBOL_GPL( direct_assumption );
EXPORT_SYMBOL_GPL( direct_debug_log );
EXPORT_SYMBOL_GPL( direct_debug_at );
EXPORT_SYMBOL_GPL( direct_debug_at_always );

EXPORT_SYMBOL_GPL( direct_dbg_calloc );
EXPORT_SYMBOL_GPL( direct_dbg_malloc );
EXPORT_SYMBOL_GPL( direct_dbg_realloc );
EXPORT_SYMBOL_GPL( direct_dbg_strdup );
EXPORT_SYMBOL_GPL( direct_dbg_free );

EXPORT_SYMBOL_GPL( direct_dbg_interface_add );
EXPORT_SYMBOL_GPL( direct_dbg_interface_remove );
#endif

//EXPORT_SYMBOL_GPL( direct_break );

EXPORT_SYMBOL_GPL( direct_print_interface_leaks );
EXPORT_SYMBOL_GPL( direct_print_memcpy_routines );
EXPORT_SYMBOL_GPL( direct_print_memleaks );

EXPORT_SYMBOL_GPL( direct_calloc );
EXPORT_SYMBOL_GPL( direct_malloc );
EXPORT_SYMBOL_GPL( direct_realloc );
EXPORT_SYMBOL_GPL( direct_strdup );
EXPORT_SYMBOL_GPL( direct_free );

EXPORT_SYMBOL_GPL( direct_open );
EXPORT_SYMBOL_GPL( direct_read );
EXPORT_SYMBOL_GPL( direct_write );
EXPORT_SYMBOL_GPL( direct_close );

EXPORT_SYMBOL_GPL( direct_futex );
EXPORT_SYMBOL_GPL( direct_futex_wait );
EXPORT_SYMBOL_GPL( direct_futex_wait_timed );
EXPORT_SYMBOL_GPL( direct_futex_wake );

EXPORT_SYMBOL_GPL( direct_getpid );
EXPORT_SYMBOL_GPL( direct_gettid );
EXPORT_SYMBOL_GPL( direct_sigaction );
EXPORT_SYMBOL_GPL( direct_page_align );
EXPORT_SYMBOL_GPL( direct_pagesize );
EXPORT_SYMBOL_GPL( direct_tgkill );
EXPORT_SYMBOL_GPL( direct_trap );

EXPORT_SYMBOL_GPL( direct_fifo_init );
EXPORT_SYMBOL_GPL( direct_fifo_destroy );
EXPORT_SYMBOL_GPL( direct_fifo_push );
EXPORT_SYMBOL_GPL( direct_fifo_pull );
EXPORT_SYMBOL_GPL( direct_fifo_pop );
EXPORT_SYMBOL_GPL( direct_fifo_wait );
EXPORT_SYMBOL_GPL( direct_fifo_wait_timed );
EXPORT_SYMBOL_GPL( direct_fifo_wakeup );

EXPORT_SYMBOL_GPL( direct_hash_create );
EXPORT_SYMBOL_GPL( direct_hash_init );
EXPORT_SYMBOL_GPL( direct_hash_deinit );
EXPORT_SYMBOL_GPL( direct_hash_destroy );
EXPORT_SYMBOL_GPL( direct_hash_insert );
EXPORT_SYMBOL_GPL( direct_hash_remove );
EXPORT_SYMBOL_GPL( direct_hash_lookup );
EXPORT_SYMBOL_GPL( direct_hash_count );
EXPORT_SYMBOL_GPL( direct_hash_iterate );

EXPORT_SYMBOL_GPL( direct_log_domain_configure );
EXPORT_SYMBOL_GPL( direct_log_domain_log );
EXPORT_SYMBOL_GPL( direct_log_domain_vprintf );

EXPORT_SYMBOL_GPL( direct_log_create );
EXPORT_SYMBOL_GPL( direct_log_default );
EXPORT_SYMBOL_GPL( direct_log_deinit );
EXPORT_SYMBOL_GPL( direct_log_destroy );
EXPORT_SYMBOL_GPL( direct_log_flush );
EXPORT_SYMBOL_GPL( direct_log_init );
EXPORT_SYMBOL_GPL( direct_log_lock );
EXPORT_SYMBOL_GPL( direct_log_printf );
EXPORT_SYMBOL_GPL( direct_log_set_buffer );
EXPORT_SYMBOL_GPL( direct_log_set_default );
EXPORT_SYMBOL_GPL( direct_log_unlock );

EXPORT_SYMBOL_GPL( direct_memcpy );

EXPORT_SYMBOL_GPL( direct_messages_bug );
EXPORT_SYMBOL_GPL( direct_messages_derror );
EXPORT_SYMBOL_GPL( direct_messages_dlerror );
EXPORT_SYMBOL_GPL( direct_messages_error );
EXPORT_SYMBOL_GPL( direct_messages_info );
EXPORT_SYMBOL_GPL( direct_messages_once );
EXPORT_SYMBOL_GPL( direct_messages_perror );
EXPORT_SYMBOL_GPL( direct_messages_unimplemented );
EXPORT_SYMBOL_GPL( direct_messages_warn );

EXPORT_SYMBOL_GPL( direct_processor_allocate );
EXPORT_SYMBOL_GPL( direct_processor_destroy );
EXPORT_SYMBOL_GPL( direct_processor_init );
EXPORT_SYMBOL_GPL( direct_processor_post );
EXPORT_SYMBOL_GPL( direct_processor_recycle );

EXPORT_SYMBOL_GPL( direct_serial_notify );
EXPORT_SYMBOL_GPL( direct_serial_wait );

EXPORT_SYMBOL_GPL( direct_signal_handler_add );
EXPORT_SYMBOL_GPL( direct_signal_handler_remove );
EXPORT_SYMBOL_GPL( direct_signals_block_all );
EXPORT_SYMBOL_GPL( direct_signals_initialize );
EXPORT_SYMBOL_GPL( direct_signals_shutdown );

EXPORT_SYMBOL_GPL( direct_thread_add_init_handler );
EXPORT_SYMBOL_GPL( direct_thread_cancel );
EXPORT_SYMBOL_GPL( direct_thread_create );
EXPORT_SYMBOL_GPL( direct_thread_deinit );
EXPORT_SYMBOL_GPL( direct_thread_destroy );
EXPORT_SYMBOL_GPL( direct_thread_detach );
EXPORT_SYMBOL_GPL( direct_thread_init );
EXPORT_SYMBOL_GPL( direct_thread_join );
EXPORT_SYMBOL_GPL( direct_thread_lock );
EXPORT_SYMBOL_GPL( direct_thread_notify );
EXPORT_SYMBOL_GPL( direct_thread_policy_name );
EXPORT_SYMBOL_GPL( direct_thread_remove_init_handler );
EXPORT_SYMBOL_GPL( direct_thread_scheduler_name );
EXPORT_SYMBOL_GPL( direct_thread_self );
EXPORT_SYMBOL_GPL( direct_thread_self_name );
EXPORT_SYMBOL_GPL( direct_thread_set_name );
EXPORT_SYMBOL_GPL( direct_thread_terminate );
EXPORT_SYMBOL_GPL( direct_thread_testcancel );
EXPORT_SYMBOL_GPL( direct_thread_type_name );
EXPORT_SYMBOL_GPL( direct_thread_unlock );
EXPORT_SYMBOL_GPL( direct_thread_wait );

EXPORT_SYMBOL_GPL( direct_trace_copy_buffer );
EXPORT_SYMBOL_GPL( direct_trace_debug_indent );
EXPORT_SYMBOL_GPL( direct_trace_free_buffer );
EXPORT_SYMBOL_GPL( direct_trace_lookup_file );
EXPORT_SYMBOL_GPL( direct_trace_lookup_symbol );
EXPORT_SYMBOL_GPL( direct_trace_print_stack );
EXPORT_SYMBOL_GPL( direct_trace_print_stacks );

EXPORT_SYMBOL_GPL( direct_tree_new );
EXPORT_SYMBOL_GPL( direct_tree_destroy );
EXPORT_SYMBOL_GPL( direct_tree_insert );
EXPORT_SYMBOL_GPL( direct_tree_lookup );

EXPORT_SYMBOL_GPL( direct_base64_decode );
EXPORT_SYMBOL_GPL( direct_base64_encode );
EXPORT_SYMBOL_GPL( direct_md5_sum );
EXPORT_SYMBOL_GPL( direct_trim );
EXPORT_SYMBOL_GPL( direct_try_open );

EXPORT_SYMBOL_GPL( direct_snprintf );
EXPORT_SYMBOL_GPL( direct_snputs );
EXPORT_SYMBOL_GPL( direct_sscanf );
EXPORT_SYMBOL_GPL( direct_strcasecmp );
EXPORT_SYMBOL_GPL( direct_strcmp );
EXPORT_SYMBOL_GPL( direct_strerror );
EXPORT_SYMBOL_GPL( direct_strlen );
EXPORT_SYMBOL_GPL( direct_strncasecmp );
EXPORT_SYMBOL_GPL( direct_strtok_r );
EXPORT_SYMBOL_GPL( direct_strtoul );
EXPORT_SYMBOL_GPL( direct_vsnprintf );
EXPORT_SYMBOL_GPL( direct_vsscanf );

EXPORT_SYMBOL_GPL( errno2result );

EXPORT_SYMBOL_GPL( __divdi3 );
EXPORT_SYMBOL_GPL( __moddi3 );
EXPORT_SYMBOL_GPL( __qdivrem );
EXPORT_SYMBOL_GPL( __udivdi3 );
EXPORT_SYMBOL_GPL( __umoddi3 );

