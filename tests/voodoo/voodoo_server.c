#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/messages.h>

#include <voodoo/server.h>
#include <voodoo/manager.h>
#include <voodoo/message.h>

#include "voodoo_test.h"


/*
 * Server structure
 */
typedef struct {
     VoodooServer  *server;

     unsigned int   count;
} VoodooTestServer;


/*
 * Per client structure
 */
typedef struct {
     /* ignore */
     IAny              dummy;

     VoodooManager    *manager;

     VoodooTestServer *server;
} VoodooTestConnection;


/* ignore */
static DirectResult
DummyRelease( IAny *thiz )
{
     return DR_OK;
}


/*
 * Callback for handling incoming requests
 */
static DirectResult
VoodooTestDispatch( void                 *dispatcher,  /* context 1 */
                    void                 *real,        /* context 2 */
                    VoodooManager        *manager,
                    VoodooRequestMessage *msg )
{
     VoodooTestConnection *connection = dispatcher;
     VoodooTestServer     *server     = connection->server;

     switch (msg->method) {
          case VOODOO_TEST_INCREASE:
               server->count++;
               break;

          case VOODOO_TEST_QUERY:
               voodoo_manager_respond( manager, true, msg->header.serial, DR_OK, VOODOO_INSTANCE_NONE,
                                       VMBT_UINT, server->count,
                                       VMBT_NONE );
               break;
     }

     return DR_OK;
}

/*
 * Callback for creating a new instance on a connection
 */
static DirectResult
VoodooTestConstruct( VoodooServer         *_server,
                     VoodooManager        *manager,
                     const char           *name,
                     void                 *ctx, /* context pointer */
                     VoodooInstanceID     *ret_instance )
{
     VoodooTestServer     *server = ctx;
     VoodooInstanceID      instance_id;
     VoodooTestConnection *connection;

     connection = D_CALLOC( 1, sizeof(VoodooTestConnection) );
     if (!connection)
          return D_OOM();

     connection->manager = manager;
     connection->server  = server;

     /* ignore */
     connection->dummy.Release = DummyRelease;

     /* Create a new instance with our callback */
     voodoo_manager_register_local( manager, VOODOO_INSTANCE_NONE,
                                    connection /* context 1 */, connection /* context 2 */,
                                    VoodooTestDispatch, &instance_id );

     D_INFO( "Voodoo/Test: Created instance %u for client with manager %p...\n", instance_id, manager );

     *ret_instance = instance_id;

     return DR_OK;
}



int
main( int argc, char *argv[] )
{
     DirectResult     ret;
     VoodooTestServer test = {0};

     ret = voodoo_server_create( "127.0.0.1", 23456, false, &test.server );
     if (ret) {
          D_DERROR( ret, "voodoo_server_create() failed!\n" );
          return ret;
     }

     voodoo_server_register( test.server, "VoodooTest", VoodooTestConstruct, &test /* context pointer */ );


     D_INFO( "Voodoo/Test: Running server...\n" );

     voodoo_server_run( test.server );

     voodoo_server_destroy( test.server );

     return 0;
}

