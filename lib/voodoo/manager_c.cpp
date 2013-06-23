/*
   (c) Copyright 2001-2011  The world wide DirectFB Open Source Community (directfb.org)
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

//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <list>

extern "C" {
#include <direct/debug.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <voodoo/conf.h>
#include <voodoo/server.h>
}

#include <voodoo/connection_packet.h>
#include <voodoo/connection_raw.h>
#include <voodoo/link.h>
#include <voodoo/manager.h>


D_DEBUG_DOMAIN( Voodoo_Manager,  "Voodoo/Manager",  "Voodoo Manager" );

/**********************************************************************************************************************/


#define VOODOO_MANAGER_MESSAGE_BLOCKS_MAX 20

static __inline__ int
calc_blocks( va_list args,
             VoodooMessageBlock *ret_blocks, size_t *ret_num )
{
     int                    size = 4;  /* for the terminating VMBT_NONE */
     size_t                 num  = 0;
     VoodooMessageBlockType type;

     /* Fetch first block type. */
     type = (VoodooMessageBlockType) va_arg( args, int );

     while (type != VMBT_NONE) {
          if (num == VOODOO_MANAGER_MESSAGE_BLOCKS_MAX) {
               // FIXME: support more blocks?
               D_UNIMPLEMENTED();
               break;
          }

          /* Set message block type. */
          ret_blocks[num].type = type;

          switch (type) {
               case VMBT_ID:
                    ret_blocks[num].len = 4;
                    ret_blocks[num].ptr = NULL;
                    ret_blocks[num].val = va_arg( args, u32 );

                    D_DEBUG( "Voodoo/Message: + ID %u\n", ret_blocks[num].val );
                    break;

               case VMBT_INT:
                    ret_blocks[num].len = 4;
                    ret_blocks[num].ptr = NULL;
                    ret_blocks[num].val = va_arg( args, s32 );

                    D_DEBUG( "Voodoo/Message: + INT %d\n", ret_blocks[num].val );
                    break;

               case VMBT_UINT:
                    ret_blocks[num].len = 4;
                    ret_blocks[num].ptr = NULL;
                    ret_blocks[num].val = va_arg( args, u32 );

                    D_DEBUG( "Voodoo/Message: + UINT %u\n", ret_blocks[num].val );
                    break;

               case VMBT_DATA:
                    ret_blocks[num].len = va_arg( args, int );
                    ret_blocks[num].ptr = va_arg( args, void * );

//                    D_ASSERT( ret_blocks[num].len > 0 );
                    D_ASSERT( ret_blocks[num].ptr != NULL );

                    D_DEBUG( "Voodoo/Message: + DATA at %p with length %d\n", ret_blocks[num].ptr, ret_blocks[num].len );
                    break;

               case VMBT_ODATA:
                    ret_blocks[num].len = va_arg( args, int );
                    ret_blocks[num].ptr = va_arg( args, void * );

                    D_ASSERT( ret_blocks[num].len > 0 );

                    D_DEBUG( "Voodoo/Message: + ODATA at %p with length %d\n", ret_blocks[num].ptr, ret_blocks[num].len );

                    if (!ret_blocks[num].ptr)
                         ret_blocks[num].len = 0;
                    break;

               case VMBT_STRING:
                    ret_blocks[num].ptr = va_arg( args, char * );
                    ret_blocks[num].len = strlen( (const char*) ret_blocks[num].ptr ) + 1;

                    D_ASSERT( ret_blocks[num].ptr != NULL );

                    D_DEBUG( "Voodoo/Message: + STRING '%s' at %p with length %d\n", (char*) ret_blocks[num].ptr, ret_blocks[num].ptr, ret_blocks[num].len );
                    break;

               default:
                    D_BREAK( "unknown message block type" );
          }

          /* Fetch next block type. */
          type = (VoodooMessageBlockType) va_arg( args, int );

          size += 8 + VOODOO_MSG_ALIGN(ret_blocks[num].len);

          num++;
     }

     *ret_num = num;

     return size;
}


/**********************************************************************************************************************/
/**********************************************************************************************************************/
/**********************************************************************************************************************/
/* Old C API
 */

/*


register add refs proxy
unregister releases proxy

proxy destruct releases real

*/

class VoodooInstanceInterface : public VoodooInstance {
public:
     VoodooManager  *manager;
     VoodooInstance *super;
     IAny           *proxy;
     IAny           *real;
     VoodooDispatch  dispatch;


     static std::list<VoodooInstanceInterface*> interfaces;


public:
     VoodooInstanceInterface( VoodooManager  *manager,
                              VoodooInstance *super,
                              IAny           *proxy,
                              IAny           *real,
                              VoodooDispatch  dispatch )
          :
          manager( manager ),
          super( super ),
          proxy( proxy ),
          real( real ),
          dispatch( dispatch )
     {
          D_DEBUG_AT( Voodoo_Manager, "VoodooInstanceInterface::%s( %p, manager %p, super %p, proxy %p, real %p, dispatch %p )\n",
                      __func__, this, manager, super, proxy, real, dispatch );

          if (super)
               super->AddRef();

          interfaces.push_back( this );
     }

protected:
     virtual ~VoodooInstanceInterface()
     {
          D_DEBUG_AT( Voodoo_Manager, "VoodooInstanceInterface::%s( %p )\n", __func__, this );

          D_MAGIC_ASSERT( this, VoodooInstance );

          if (proxy) {
               D_DEBUG_AT( Voodoo_Manager, "  -> releasing proxy interface\n" );

               proxy->Release( proxy );
          }


          if (super) {
               D_DEBUG_AT( Voodoo_Manager, "  -> releasing super instance\n" );

               super->Release();
          }

          interfaces.remove( this );
     }

public:
     virtual DirectResult
     Dispatch( VoodooManager        *manager,
               VoodooRequestMessage *msg )
     {
          D_DEBUG_AT( Voodoo_Manager, "VoodooInstanceInterface::%s( %p, manager %p, msg %p )\n", __func__, this, manager, msg );

          D_MAGIC_ASSERT( this, VoodooInstance );

          D_ASSERT( dispatch != NULL );

          return dispatch( proxy, real, manager, msg );
     }
};

std::list<VoodooInstanceInterface*> VoodooInstanceInterface::interfaces;

/**********************************************************************************************************************/

class VoodooContextClassic : public VoodooContext {
private:
     VoodooServer *server;

public:
     VoodooContextClassic( VoodooServer *server )
          :
          server( server )
     {
     }

     virtual DirectResult
     HandleSuper( VoodooManager *manager, const char *name, VoodooInstanceID *ret_instance )
     {
          return voodoo_server_construct( server, manager, name, ret_instance );
     }
};

DirectResult
voodoo_manager_create( VoodooLink     *link,
                       VoodooClient   *client,
                       VoodooServer   *server,
                       VoodooManager **ret_manager )
{
     VoodooConnection *connection;

     D_ASSERT( ret_manager != NULL );

     /* Add connection */
     if ((link->code & 0x8000ffff) == 0x80008676) {
          D_INFO( "Voodoo/Manager: Connection mode is PACKET\n" );

          connection = new VoodooConnectionPacket( link );
     }
     else {
          D_INFO( "Voodoo/Manager: Connection mode is RAW\n" );

          connection = new VoodooConnectionRaw( link );

          // FIXME: query manager dynamically for compression instead
          voodoo_config->compression_min = 0;
     }

     *ret_manager = new VoodooManager( connection, new VoodooContextClassic( server ) );  // FIXME: leak

     return DR_OK;
}

DirectResult
voodoo_manager_quit( VoodooManager *manager )
{
     D_MAGIC_ASSERT( manager, VoodooManager );

     manager->quit();

     return DR_OK;
}

DirectResult
voodoo_manager_destroy( VoodooManager *manager )
{
     D_MAGIC_ASSERT( manager, VoodooManager );

     delete manager;

     for (std::list<VoodooInstanceInterface*>::const_iterator iter = VoodooInstanceInterface::interfaces.begin();
          iter != VoodooInstanceInterface::interfaces.end(); iter++)
     {
          VoodooInstanceInterface *instance = *iter;

          if (instance->manager == manager)
               D_INFO( "Zombie: Instance %p, proxy %p, real %p, super %p\n", instance, instance->proxy, instance->real, instance->super );
     }

     return DR_OK;
}

bool
voodoo_manager_is_closed( const VoodooManager *manager )
{
     D_MAGIC_ASSERT( manager, VoodooManager );

     return manager->is_quit;
}

/**************************************************************************************************/

DirectResult
voodoo_manager_super( VoodooManager    *manager,
                      const char       *name,
                      VoodooInstanceID *ret_instance )
{
     D_MAGIC_ASSERT( manager, VoodooManager );

     return manager->do_super( name, ret_instance );
}

DirectResult
voodoo_manager_request( VoodooManager           *manager,
                        VoodooInstanceID         instance,
                        VoodooMethodID           method,
                        VoodooRequestFlags       flags,
                        VoodooResponseMessage  **ret_response, ... )
{
     DirectResult ret;

     D_MAGIC_ASSERT( manager, VoodooManager );

     va_list ap;

     va_start( ap, ret_response );


     VoodooMessageBlock    blocks[VOODOO_MANAGER_MESSAGE_BLOCKS_MAX];
     size_t                num_blocks;
     size_t                data_size;

     data_size = calc_blocks( ap, blocks, &num_blocks );


     ret = manager->do_request( instance, method, flags, ret_response, blocks, num_blocks, data_size );

     va_end( ap );

     return ret;
}

DirectResult
voodoo_manager_next_response( VoodooManager          *manager,
                              VoodooResponseMessage  *response,
                              VoodooResponseMessage **ret_response )
{
     D_MAGIC_ASSERT( manager, VoodooManager );

     return manager->next_response( response, ret_response );
}

DirectResult
voodoo_manager_finish_request( VoodooManager         *manager,
                               VoodooResponseMessage *response )
{
     D_MAGIC_ASSERT( manager, VoodooManager );

     return manager->finish_request( response );
}

DirectResult
voodoo_manager_respond( VoodooManager          *manager,
                        bool                    flush,
                        VoodooMessageSerial     request,
                        DirectResult            result,
                        VoodooInstanceID        instance, ... )
{
     DirectResult ret;

     D_MAGIC_ASSERT( manager, VoodooManager );

     va_list ap;

     va_start( ap, instance );


     VoodooMessageBlock    blocks[VOODOO_MANAGER_MESSAGE_BLOCKS_MAX];
     size_t                num_blocks;
     size_t                data_size;

     data_size = calc_blocks( ap, blocks, &num_blocks );


     ret = manager->do_respond( flush, request, result, instance, blocks, num_blocks, data_size );

     va_end( ap );

     return ret;
}

DirectResult
voodoo_manager_register_local( VoodooManager    *manager,
                               VoodooInstanceID  super,
                               void             *dispatcher,
                               void             *real,
                               VoodooDispatch    dispatch,
                               VoodooInstanceID *ret_instance )
{
     DirectResult    ret;
     VoodooInstance *super_instance = NULL;

     D_MAGIC_ASSERT( manager, VoodooManager );

     if (super != VOODOO_INSTANCE_NONE) {
          ret = manager->lookup_local( super, &super_instance );
          if (ret) {
               D_DERROR( ret, "Voodoo/Manager: Could not lookup super instance %u!\n", super );
               return ret;
          }
     }


     VoodooInstanceInterface *instance = new VoodooInstanceInterface( manager, super_instance, (IAny*) dispatcher, (IAny*) real, dispatch );

     ret = manager->register_local( instance, ret_instance );

     instance->Release();

     return ret;
}

DirectResult
voodoo_manager_unregister_local( VoodooManager    *manager,
                                 VoodooInstanceID  instance_id )
{
     D_MAGIC_ASSERT( manager, VoodooManager );

     return manager->unregister_local( instance_id );
}

DirectResult
voodoo_manager_lookup_local( VoodooManager     *manager,
                             VoodooInstanceID   instance_id,
                             void             **ret_dispatcher,
                             void             **ret_real )
{
     DirectResult    ret;
     VoodooInstance *instance;

     D_MAGIC_ASSERT( manager, VoodooManager );

     ret = manager->lookup_local( instance_id, &instance );
     if (ret)
          return ret;

     if (ret_dispatcher)
          *ret_dispatcher = ((VoodooInstanceInterface*) instance)->proxy;

     if (ret_real)
          *ret_real = ((VoodooInstanceInterface*) instance)->real;

     return DR_OK;
}

DirectResult
voodoo_manager_register_remote( VoodooManager    *manager,
                                bool              super,
                                void             *requestor,
                                VoodooInstanceID  instance_id )
{
     DirectResult ret;

     D_MAGIC_ASSERT( manager, VoodooManager );

     VoodooInstanceInterface *instance = new VoodooInstanceInterface( manager, NULL, (IAny*) requestor, NULL, NULL);

     ret = manager->register_remote( instance, instance_id );

     instance->Release();

     return ret;
}


DirectResult
voodoo_manager_lookup_remote( VoodooManager     *manager,
                              VoodooInstanceID   instance_id,
                              void             **ret_requestor )
{
     DirectResult    ret;
     VoodooInstance *instance;

     D_MAGIC_ASSERT( manager, VoodooManager );

     ret = manager->lookup_remote( instance_id, &instance );
     if (ret)
          return ret;

     if (ret_requestor)
          *ret_requestor = ((VoodooInstanceInterface*) instance)->proxy;

     return DR_OK;
}

DirectResult
voodoo_manager_check_allocation( VoodooManager *manager,
                                 unsigned int   amount )
{
#ifndef WIN32
     FILE   *f;
     char    buf[2000];
     int     size;
     char   *p;
     size_t  bytes;

     D_MAGIC_ASSERT( manager, VoodooManager );

     if (!voodoo_config->memory_max)
          return DR_OK;

     direct_snprintf( buf, sizeof(buf), "/proc/%d/status", direct_getpid() );

     f = fopen( buf, "r" );
     if (!f) {
          D_ERROR( "Could not open '%s'!\n", buf );
          return DR_FAILURE;
     }

     bytes = fread( buf, 1, sizeof(buf)-1, f );

     fclose( f );

     if (bytes) {
          buf[bytes] = 0;

          p = strstr( buf, "VmRSS:" );
          if (!p) {
               D_ERROR( "Could not find memory information!\n" );
               return DR_FAILURE;
          }

          sscanf( p + 6, " %u", &size );

          D_INFO( "SIZE: %u kB (+%u kB)\n", size, amount / 1024 );

          if (size * 1024 + amount > voodoo_config->memory_max)
               return DR_LIMITEXCEEDED;
     }
#endif
     return DR_OK;
}

long long
voodoo_manager_connection_delay( VoodooManager *manager )
{
     D_MAGIC_ASSERT( manager, VoodooManager );

     return manager->connection_delay();
}

long long
voodoo_manager_clock_to_local( VoodooManager *manager,
                               long long      remote )
{
     D_MAGIC_ASSERT( manager, VoodooManager );

     return manager->clock_to_local( remote );
}

long long
voodoo_manager_clock_to_remote( VoodooManager *manager,
                                long long      local )
{
     D_MAGIC_ASSERT( manager, VoodooManager );

     return manager->clock_to_remote( local );
}

