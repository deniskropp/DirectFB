/*
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



#include <config.h>

#include <direct/filesystem.h>
#include <direct/list.h>
#include <direct/log.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/print.h>
#include <direct/system.h>
#include <direct/thread.h>
#include <direct/trace.h>
#include <direct/util.h>
#include <direct/Utils.h>

#include <direct/String.hxx>


#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#else
static inline int
backtrace(void **buffer, int size)
{
	return 0;
}
#endif


#if DIRECT_BUILD_DYNLOAD
#include <dlfcn.h>
#endif

#define MAX_BUFFERS 200
#define MAX_LEVEL   200

#define NAME_LEN    92




D_LOG_DOMAIN( Direct_Trace, "Direct/Trace", "Trace support" );

/**************************************************************************************************/

__dfb_no_instrument_function__
static void print_symbol( void     *fn,
                          int       index,
                          D_String *string );

/**************************************************************************************************/

typedef enum {
     TF_NONE   = 0x00000000,

     TF_DEBUG  = 0x00000001
} TraceFlags;

typedef struct {
     void       *addr;
     TraceFlags  flags;
} Trace;

struct __D_DirectTraceBuffer {
     DirectLink link;
     int        magic;
     pid_t tid;
     char *name;
     DirectThread *thread;
     int   level;
     bool  in_trace;
     Trace trace[MAX_LEVEL];
};

/**************************************************************************************************/

#if DIRECT_BUILD_TRACE
static DirectLink  *buffers;
static DirectMutex  buffers_lock = DIRECT_RECURSIVE_MUTEX_INITIALIZER(buffers_lock);
#endif

/**************************************************************************************************/

__dfb_no_instrument_function__
static inline DirectTraceBuffer *
get_trace_buffer( void )
{
     DirectTraceBuffer *buffer = NULL;

#if DIRECT_BUILD_TRACE
     DirectThread *self = direct_thread_self();

     buffer = self->trace_buffer;
     if (!buffer) {
          buffer = direct_calloc( 1, sizeof(DirectTraceBuffer) );
          if (!buffer)
               return NULL;

          buffer->tid    = direct_gettid();
          buffer->thread = direct_thread_self();

          D_MAGIC_SET( buffer, DirectTraceBuffer );

          self->trace_buffer = buffer;

          direct_mutex_lock( &buffers_lock );
          direct_list_append( &buffers, &buffer->link );
          direct_mutex_unlock( &buffers_lock );
     }
#endif

     return buffer;
}

/**************************************************************************************************/

typedef struct {
     long offset;
     char name[NAME_LEN];
} Symbol;

typedef struct {
     DirectLink  link;

     char       *filename;
     Symbol     *symbols;
     int         capacity;
     int         num_symbols;
} SymbolTable;

static DirectLink  *tables      = NULL;
static DirectMutex  tables_lock = DIRECT_RECURSIVE_MUTEX_INITIALIZER(tables_lock);


__dfb_no_instrument_function__
static void
add_symbol( SymbolTable *table, long offset, const char *name )
{
     Symbol *symbol;

     if (table->num_symbols == table->capacity) {
          Symbol *symbols;
          int     capacity = table->capacity * 2;

          if (!capacity)
               capacity = 256;

          symbols = direct_malloc( capacity * sizeof(Symbol) );
          if (!symbols) {
               D_WARN( "out of memory" );
               return;
          }

          direct_memcpy( symbols, table->symbols, table->num_symbols * sizeof(Symbol) );

          direct_free( table->symbols );

          table->symbols  = symbols;
          table->capacity = capacity;
     }

     symbol = &table->symbols[ table->num_symbols++ ];

     symbol->offset = offset;

     direct_snputs( symbol->name, name, NAME_LEN );
}

__dfb_no_instrument_function__
static SymbolTable *
load_symbols( const char *filename )
{
     DirectResult ret;
     SymbolTable *table;
     DirectFile   fp;
     bool         is_pipe = false;
     char         file[1024];
     char         line[1024];
     int          command_len;
     const char  *full_path = filename;
     char        *tmp;

     D_DEBUG_AT( Direct_Trace, "%s( %s )\n", __FUNCTION__, filename );

     if (filename) {
          ret = direct_access( filename, R_OK );
          if (ret && ret == DR_FILENOTFOUND) {
               size_t len = 0;

               ret = direct_readlink( "/proc/self/exe", file, sizeof(file) - 1, &len );
               if (ret) {
                    D_DERROR( ret, "Direct/Trace: direct_readlink( \"/proc/self/exe\" ) failed!\n" );
                    return NULL;
               }

               // Terminator!
               file[len] = 0;


               tmp = strrchr( file, '/' );
               if (!tmp)
                    return NULL;

               if (direct_strcmp( filename, tmp + 1 ))
                    return NULL;

               full_path = file;
          }
     }
     else {
          size_t len = 0;

          ret = direct_readlink( "/proc/self/exe", file, sizeof(file) - 1, &len );
          if (ret) {
               D_DERROR( ret, "Direct/Trace: readlink( \"/proc/self/exe\" ) failed!\n" );
               return NULL;
          }

          // Terminator 2
          file[len] = 0;

          full_path = file;
     }

     command_len = direct_strlen( full_path ) + 32;

     {
          char command[command_len+1];

          /* First check if there's an "nm-n" file. */
          tmp = strrchr( full_path, '/' );
          if (!tmp)
               return NULL;

          *tmp = 0;
          direct_snprintf( command, command_len, "%s/nm-n.%s", full_path, tmp + 1 );
          *tmp = '/';

          ret = direct_access( command, R_OK );
          if (ret == DR_OK) {
               ret = direct_file_open( &fp, command, O_RDONLY, 0 );
               if (ret)
                    D_DERROR( ret, "Direct/Trace: direct_file_open( \"%s\", \"r\" ) failed!\n", command );
          }
          else {
               direct_snprintf( command, command_len, "%s.nm", full_path );

               ret = direct_access( command, R_OK );
               if (ret == DR_OK) {
                    ret = direct_file_open( &fp, command, O_RDONLY, 0 );
                    if (ret)
                         D_DERROR( ret, "Direct/Trace: direct_file_open( \"%s\", \"r\" ) failed!\n", command );
               }
          }

          /* Fallback to live mode. */
          if (ret) {
               direct_snprintf( command, command_len, "nm -nC %s", full_path );

               if (!direct_config->nm_for_trace) {
                    D_DEBUG_AT( Direct_Trace, "not running '%s', enable via 'nm-for-trace' option\n", command );
                    return NULL;
               }

               D_DEBUG_AT( Direct_Trace, "running '%s'...\n", command );

               ret = direct_popen( &fp, command, O_RDONLY );
               if (ret) {
                    D_DERROR( ret, "Direct/Trace: direct_popen( \"%s\", \"r\" ) failed!\n", command );
                    return NULL;
               }

               is_pipe = true;
          }
     }

     table = direct_calloc( 1, sizeof(SymbolTable) );
     if (!table) {
          D_OOM();
          goto out;
     }

     if (filename)
          table->filename = direct_strdup( filename );

     while (direct_fgets( &fp, line, sizeof(line) ) == DR_OK) {
          int  n;
          int  digits = sizeof(long) * 2;
          long offset = 0;
          int  length = direct_strlen(line);

          if (line[0] == ' ' || length < (digits + 5) || line[length-1] != '\n')
               continue;

          if (line[digits + 1] != 't' && line[digits + 1] != 'T' && line[digits + 1] != 'W')
               continue;

          if (line[digits] != ' ' || line[digits + 2] != ' ' || line[digits + 3] == '.')
               continue;

          for (n=0; n<digits; n++) {
               char c = line[n];

               offset <<= 4;

               if (c >= '0' && c <= '9')
                    offset |= c - '0';
               else
                    offset |= c - 'a' + 10;
          }

          line[length-1] = 0;

          add_symbol( table, offset, line + digits + 3 );
     }

out:
     if (is_pipe)
          direct_pclose( &fp );
     else
          direct_file_close( &fp );

     return table;
}

__dfb_no_instrument_function__
static int
compare_symbols(const void *x, const void *y)
{
     return  *((const long*) x)  -  *((const long*) y);
}

__dfb_no_instrument_function__
static SymbolTable *
find_table( const char *filename )
{
     SymbolTable *table;

     if (filename) {
          direct_list_foreach (table, tables) {
               if (table->filename && !direct_strcmp( filename, table->filename ))
                    return table;
          }
     }
     else {
          direct_list_foreach (table, tables) {
               if (!table->filename)
                    return table;
          }
     }

     return NULL;
}

/**************************************************************************************************/

__dfb_no_instrument_function__
const char *
direct_trace_lookup_symbol( const char *filename, long offset )
{
     Symbol      *symbol;
     SymbolTable *table;

     direct_mutex_lock( &tables_lock );

     table = find_table( filename );
     if (!table) {
          table = load_symbols( filename );
          if (!table) {
               direct_mutex_unlock( &tables_lock );
               return false;
          }

          direct_list_prepend( &tables, &table->link );
     }

     direct_mutex_unlock( &tables_lock );

     symbol = direct_bsearch( &offset, table->symbols, table->num_symbols,
                              sizeof(Symbol), compare_symbols );

     return symbol ? symbol->name : NULL;
}

__dfb_no_instrument_function__
const char *
direct_trace_lookup_file( void *address, void **ret_base )
{
#if DIRECT_BUILD_DYNLOAD
     Dl_info info;

     if (dladdr( address, &info )) {
          if (ret_base)
               *ret_base = info.dli_fbase;

          return info.dli_fname;
     }
     else
#endif
     {
          if (ret_base)
               *ret_base = NULL;
     }

     return NULL;
}

__dfb_no_instrument_function__
void
direct_trace_print_stack( DirectTraceBuffer *buffer )
{
     int       i;
     int       level;
     D_String *string;

     if (!direct_config->trace)
          return;

     if (!buffer) {
          buffer = get_trace_buffer();
          if (!buffer) {
               direct_print_backtrace();
               return;
          }
     }

     if (buffer->in_trace)
          return;

     buffer->in_trace = true;


     level = buffer->level;
     if (level > MAX_LEVEL) {
          D_WARN( "only showing %d of %d items", MAX_LEVEL, level );
          level = MAX_LEVEL;
     }
     else if (level == 0) {
          buffer->in_trace = false;
          return;
     }

     string = D_String_NewEmpty();
     if (!string)
          return;

     D_String_PrintF( string, "(-) [%5d: -STACK- '%s']\n", buffer->tid, buffer->thread ? buffer->thread->name : buffer->name );

     for (i=level-1; i>=0; i--)
          print_symbol( buffer->trace[i].addr, level - i - 1, string );

     D_String_PrintF( string, "\n" );

     direct_log_write( NULL, D_String_Buffer( string ), D_String_Length( string ) );

     D_String_Delete( string );

     buffer->in_trace = false;
}

__dfb_no_instrument_function__
void
direct_trace_print_stacks()
{
#if DIRECT_BUILD_TRACE
     DirectTraceBuffer *b;
     DirectTraceBuffer *buffer = get_trace_buffer();

     direct_mutex_lock( &buffers_lock );

     if (buffer && buffer->level)
          direct_trace_print_stack( buffer );

     direct_list_foreach (b, buffers) {
          if (b != buffer && b->level)
               direct_trace_print_stack( b );
     }

     direct_mutex_unlock( &buffers_lock );
#endif
}

__dfb_no_instrument_function__
int
direct_trace_debug_indent()
{
     int                in     = 0;
     DirectTraceBuffer *buffer = get_trace_buffer();

     if (buffer) {
          int level = buffer->level - 1;

          if (level < 0)
               return 0;

          buffer->trace[level--].flags |= TF_DEBUG;

          for (in=0; level>=0; level--) {    // FIXME: optimise by storing indent in trace frame
               if (buffer->trace[level].flags & TF_DEBUG)
                    in++;
          }
     }

     return in;
}

__dfb_no_instrument_function__
void *
direct_trace_get_caller()
{
     void              *caller = NULL;
     DirectTraceBuffer *buffer = get_trace_buffer();

     if (buffer) {
          int level = buffer->level - 2;

          if (level >= 0)
               caller = buffer->trace[level].addr;
     }

     return caller;
}

__dfb_no_instrument_function__
DirectTraceBuffer *
direct_trace_copy_buffer( DirectTraceBuffer *buffer )
{
     int                level;
     DirectTraceBuffer *copy;

     if (!buffer) {
          buffer = get_trace_buffer();
          if (!buffer)
               return NULL;
     }

     level = buffer->level;
     if (level > MAX_LEVEL) {
          D_WARN( "only copying %d of %d items", MAX_LEVEL, level );
          level = MAX_LEVEL;
     }

     copy = direct_calloc( 1, sizeof(DirectTraceBuffer) - sizeof(Trace) * (MAX_LEVEL - level) );
     if (!copy)
          return NULL;

     if (buffer->thread && buffer->thread->name)
          copy->name = direct_strdup( buffer->thread->name );

     copy->tid   = buffer->tid;
     copy->level = buffer->level;

     direct_memcpy( &copy->trace[0], &buffer->trace[0], level * sizeof(Trace) );

     D_MAGIC_SET( copy, DirectTraceBuffer );

     return copy;
}

__dfb_no_instrument_function__
void
direct_trace_free_buffer( DirectTraceBuffer *buffer )
{
     D_MAGIC_ASSERT( buffer, DirectTraceBuffer );

     if (buffer->thread) {
#if DIRECT_BUILD_TRACE
          direct_mutex_lock( &buffers_lock );
          direct_list_remove( &buffers, &buffer->link );
          direct_mutex_unlock( &buffers_lock );
#endif

          buffer->thread = NULL;
     }

     if (buffer->name)
          direct_free( buffer->name );

     D_MAGIC_CLEAR( buffer );

     direct_free( buffer );
}

/**********************************************************************************************************************/

#if DIRECT_BUILD_TRACE

__dfb_no_instrument_function__ void __cyg_profile_func_enter( void *this_fn, void *call_site );
__dfb_no_instrument_function__ void __cyg_profile_func_exit ( void *this_fn, void *call_site );

void
__cyg_profile_func_enter( void *this_fn,
                          void *call_site )
{
     if (direct_config->trace) {
          DirectTraceBuffer *buffer = get_trace_buffer();

          if (buffer) {
               int    level = buffer->level++;
               Trace *trace = &buffer->trace[level];

               if (level < MAX_LEVEL) {
                    trace->addr  = this_fn;
                    trace->flags = TF_NONE;
               }
          }
     }
}

void
__cyg_profile_func_exit( void *this_fn,
                         void *call_site )
{
     if (direct_config->trace) {
          DirectTraceBuffer *buffer = get_trace_buffer();

          if (buffer) {
               if (buffer->level > 0)
                    buffer->level--;
          }
     }
}

#endif


static void
print_symbol( void     *fn,
              int       index,
              D_String *string )
{
#if DIRECT_BUILD_DYNLOAD
     Dl_info info;

     if (dladdr( fn, &info )) {
          if (info.dli_fname) {
               const char *symbol = NULL;

#if DIRECT_BUILD_TRACE
               symbol = direct_trace_lookup_symbol(info.dli_fname, (long)(fn - info.dli_fbase));
               if (!symbol) {
                    symbol = direct_trace_lookup_symbol(info.dli_fname, (long)(fn));
                    if (!symbol) {
#endif
                         if (info.dli_sname)
                              symbol = info.dli_sname;
                         else
                              symbol = "??";
#if DIRECT_BUILD_TRACE
                    }
               }
#endif

               D_String_PrintF( string, "  #%-2d 0x%08lx in %s () from %s [%p]\n",
                                index, (unsigned long) fn, D_Demangle(symbol), info.dli_fname, info.dli_fbase );
          }
          else if (info.dli_sname) {
               D_String_PrintF( string, "  #%-2d 0x%08lx in %s ()\n",
                                index, (unsigned long) fn, D_Demangle(info.dli_sname) );
          }
          else
               D_String_PrintF( string, "  #%-2d 0x%08lx in ?? ()\n",
                                index, (unsigned long) fn );
     }
     else
#endif
     {
          const char *symbol = direct_trace_lookup_symbol(NULL, (long)(fn));

          D_String_PrintF( string, "  #%-2d 0x%08lx in %s ()\n",
                           index, (unsigned long) fn, symbol ? D_Demangle(symbol) : "??" );
     }
}


void
direct_print_backtrace()
{
     void *buffer[32];
     int   count;

     count = backtrace( buffer, D_ARRAY_SIZE(buffer) );
     if (count > 0) {
          D_String *string = D_String_NewEmpty();

          if (string) {
               int i;

               D_String_PrintF( string, "(-) [%5d: -STACK- '%s']\n", direct_gettid(),
                                direct_thread_self_name() ? : "NO NAME" );

               for (i=0; i<count; i++)
                    print_symbol( buffer[i], i, string );
          }

          D_String_PrintF( string, "\n" );

          direct_log_write( NULL, D_String_Buffer( string ), D_String_Length( string ) );

          D_String_Delete( string );
     }
}

