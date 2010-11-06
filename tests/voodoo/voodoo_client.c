#include <direct/messages.h>

#include <voodoo/client.h>
#include <voodoo/manager.h>
#include <voodoo/message.h>

#include "voodoo_test.h"


/*
 * Client structure
 */
typedef struct {
     VoodooClient     *client;
     VoodooManager    *manager;

     VoodooInstanceID  instance_id;
} VoodooTestClient;



int
main( int argc, char *argv[] )
{
     DirectResult     ret;
     int              i;
     VoodooTestClient test;

     D_INFO( "Voodoo/Test: Connecting to server on localhost...\n" );

     ret = voodoo_client_create( "127.0.0.1", 23456, &test.client );
     if (ret) {
          D_DERROR( ret, "voodoo_client_create() failed!\n" );
          return ret;
     }

     test.manager = voodoo_client_manager( test.client );

     /* Create a new instance on the connection */
     ret = voodoo_manager_super( test.manager, "VoodooTest", &test.instance_id );
     if (ret) {
          D_DERROR( ret, "voodoo_manager_super() failed!\n" );
          return ret;
     }

     /* Call several times asynchronous increase... */
     for (i=0; i<1000000000; i++) {
          ret = voodoo_manager_request( test.manager, test.instance_id, VOODOO_TEST_INCREASE, VREQ_QUEUE, NULL,
                                        VMBT_NONE );
          if (ret) {
               D_DERROR( ret, "voodoo_manager_request( VOODOO_TEST_INCREASE ) failed!\n" );
               return ret;
          }
     }


     /* Call query synchronously */
     VoodooResponseMessage *response;

     ret = voodoo_manager_request( test.manager, test.instance_id, VOODOO_TEST_QUERY, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret) {
          D_DERROR( ret, "voodoo_manager_request( VOODOO_TEST_QUERY ) failed!\n" );
          return ret;
     }

     /* Read contents of response */
     VoodooMessageParser parser;
     unsigned int        counter;

     VOODOO_PARSER_BEGIN( parser, response );
     VOODOO_PARSER_GET_UINT( parser, counter );
     VOODOO_PARSER_END( parser );

     voodoo_manager_finish_request( test.manager, response );


     D_INFO( "Voodoo/Test: Counter is now at %u\n", counter );


     voodoo_client_destroy( test.client );

     return 0;
}

