
#include <config.h>

#if 0

extern "C" {
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/trace.h>
}

#include <string>


//pair 1
void* operator new(std::size_t size) throw(std::bad_alloc)
{
     void *ptr;

     ptr = D_MALLOC( size );
//     D_INFO( "new %zu -> %p\n", size, ptr );
     //D_INFO( "new %zu -> %p   (%p)\n", size, ptr,
     //        direct_trace_get_caller() );
     //D_INFO( "new %zu -> %p   (%s)\n", size, ptr,
     //        direct_trace_lookup_symbol_at( __builtin_return_address(1)/*direct_trace_get_caller()*/ ) );

     return ptr;
}
void operator delete(void* ptr) throw()
{
//     D_INFO( "free %p\n", ptr );
     D_FREE( ptr );
}

//pair 2
void* operator new   (std::size_t size, const std::nothrow_t&) throw()
{
     void *ptr;

     ptr = D_MALLOC( size );
//     D_INFO( "new %zu -> %p\n", size, ptr );

     return ptr;
}
void operator delete (void* ptr, const std::nothrow_t&) throw()
{
//     D_INFO( "free %p\n", ptr );
     D_FREE( ptr );
}

//pair 3
void* operator new  [](std::size_t size) throw(std::bad_alloc)
{
     void *ptr;

     ptr = D_MALLOC( size );
//     D_INFO( "new %zu -> %p\n", size, ptr );

     return ptr;
}
void operator delete[](void* ptr) throw()
{
//     D_INFO( "free %p\n", ptr );
     D_FREE( ptr );
}

//pair 4
void* operator new  [](std::size_t size, const std::nothrow_t&) throw()
{
     void *ptr;

     ptr = D_MALLOC( size );
//     D_INFO( "new %zu -> %p\n", size, ptr );

     return ptr;
}
void operator delete[](void* ptr, const std::nothrow_t&) throw()
{
//     D_INFO( "free %p\n", ptr );
     D_FREE( ptr );
}

#endif

