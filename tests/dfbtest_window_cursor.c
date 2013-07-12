#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <direct/debug.h>
#include <direct/util.h>

#include <directfb.h>
#include <directfb_util.h>

/**************************************************************************************************/

typedef struct {
     int                  magic;

     IDirectFBWindow     *window;
     IDirectFBSurface    *surface;

     DFBWindowID          window_id;
} DemoWindow;

/**************************************************************************************************/

static IDirectFB             *m_dfb    = NULL;
static IDirectFBDisplayLayer *m_layer  = NULL;
static IDirectFBEventBuffer  *m_events = NULL;

/**************************************************************************************************/

static DemoWindow   windows[2];

/**************************************************************************************************/

static DFBResult
init_directfb( int *pargc, char ***pargv )
{
     DFBResult ret;

     D_ASSERT( m_dfb == NULL );
     D_ASSERT( m_layer == NULL );
     D_ASSERT( m_events == NULL );

     ret = DirectFBInit( pargc, pargv );
     if (ret) {
          DirectFBError( "DirectFBInit() failed", ret );
          return ret;
     }

     ret = DirectFBCreate( &m_dfb );
     if (ret) {
          DirectFBError( "DirectFBCreate() failed", ret );
          return ret;
     }

     ret = m_dfb->GetDisplayLayer( m_dfb, DLID_PRIMARY, &m_layer );
     if (ret) {
          DirectFBError( "IDirectFB::GetDisplayLayer() failed", ret );
          goto error;
     }

     ret = m_dfb->CreateEventBuffer( m_dfb, &m_events );
     if (ret) {
          DirectFBError( "IDirectFB::CreateEventBuffer() failed", ret );
          goto error;
     }

     return DFB_OK;


error:
     if (m_layer) {
          m_layer->Release( m_layer );
          m_layer = NULL;
     }

     if (m_dfb) {
          m_dfb->Release( m_dfb );
          m_dfb = NULL;
     }

     return ret;
}

static void
destroy_directfb( void )
{
     D_ASSERT( m_events != NULL );
     D_ASSERT( m_layer != NULL );
     D_ASSERT( m_dfb != NULL );

     m_events->Release( m_events );
     m_layer->Release( m_layer );
     m_dfb->Release( m_dfb );

     m_events = NULL;
     m_layer  = NULL;
     m_dfb    = NULL;
}

/**************************************************************************************************/

static void
init_cursor( IDirectFBWindow *window )
{
     DFBSurfaceDescription  desc;
     IDirectFBSurface      *surface;

     desc.flags       = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
     desc.width       = 100;
     desc.height      = 100;
     desc.pixelformat = DSPF_ARGB;

     m_dfb->CreateSurface( m_dfb, &desc, &surface );

     surface->Clear( surface, 0, 0, 0, 0 );

     surface->SetColor( surface, 0xff, 0, 0, 0xff );
     surface->DrawRectangle( surface, 0, 0, desc.width, desc.height );


     window->SetCursorShape( window, surface, 50, 50 );
}

static DFBResult
init_window( DemoWindow *dw,
             const DFBRectangle *rect, u32 key,
             DFBWindowStackingClass stacking,
             DFBWindowCapabilities caps,
             DFBWindowOptions options,
             DemoWindow *parent )
{
     DFBResult             ret;
     DFBWindowDescription  desc;
     IDirectFBWindow      *window;
     IDirectFBSurface     *surface = NULL;

     D_ASSERT( dw != NULL );
     DFB_RECTANGLE_ASSERT( rect );
     D_MAGIC_ASSERT_IF( parent, DemoWindow );

     /* Fill window description. */
     desc.flags     = DWDESC_WIDTH    | DWDESC_HEIGHT  | DWDESC_CAPS |
                      DWDESC_STACKING | DWDESC_OPTIONS | DWDESC_POSX | DWDESC_POSY;
     desc.width     = rect->w;
     desc.height    = rect->h;
     desc.posx      = rect->x;
     desc.posy      = rect->y;
     desc.caps      = caps;
     desc.stacking  = stacking;
     desc.options   = options;// | (key ? DWOP_COLORKEYING : DWOP_NONE);

     if (parent) {
          desc.flags     |= DWDESC_PARENT;
          desc.parent_id  = parent->window_id;
     }

     /* Create the window. */
     ret = m_layer->CreateWindow( m_layer, &desc, &window );
     if (ret) {
          DirectFBError( "IDirectFBDisplayLayer::CreateWindow() failed", ret );
          return ret;
     }

     if (!(caps & DWCAPS_INPUTONLY)) {
          ret = window->GetSurface( window, &surface );
          if (ret) {
               DirectFBError( "IDirectFBWindow::GetSurface() failed", ret );
               window->Release( window );
               return ret;
          }

          window->SetColorKey( window, (key >> 16) & 0xff, (key >> 8) & 0xff, key & 0xff );

          surface->Clear( surface, (key >> 16) & 0xff, (key >> 8) & 0xff, key & 0xff, 0xff );
     }

     window->AttachEventBuffer( window, m_events );

     window->GetID( window, &dw->window_id );

     init_cursor( window );

     /* Return new interfaces. */
     dw->window  = window;
     dw->surface = surface;

     D_MAGIC_SET( dw, DemoWindow );

     return DFB_OK;
}

static void
show_window( DemoWindow *dw,
             bool        visible )
{
     IDirectFBWindow *window;

     D_MAGIC_ASSERT( dw, DemoWindow );

     window = dw->window;
     D_ASSERT( window != NULL );

     window->SetOpacity( window, visible ? 0xff : 0x00 );
}

static void
destroy_window( DemoWindow *dw )
{
     IDirectFBSurface *surface;
     IDirectFBWindow  *window;

     D_MAGIC_ASSERT( dw, DemoWindow );

     surface = dw->surface;
     D_ASSERT( surface != NULL );

     window = dw->window;
     D_ASSERT( window != NULL );

     window->DetachEventBuffer( window, m_events );

     surface->Release( surface );
     window->Release( window );

     dw->surface = NULL;
     dw->window  = NULL;

     D_MAGIC_CLEAR( dw );
}

/**************************************************************************************************/

static DFBBoolean
dispatch_events( void )
{
     DFBWindowEvent event;

     while (m_events->GetEvent( m_events, DFB_EVENT(&event) ) == DFB_OK) {
          int i;

          switch (event.type) {
               case DWET_GOTFOCUS:
                    for (i=0; i<2; i++) {
                         if (windows[i].window_id == event.window_id) {
                              windows[i].surface->Clear( windows[i].surface, 0xff, 0xff, 0xff, 0xff );

                              windows[i].surface->Flip( windows[i].surface, NULL, DSFLIP_NONE );
                         }
                    }
                    break;

               case DWET_LOSTFOCUS:
                    for (i=0; i<2; i++) {
                         if (windows[i].window_id == event.window_id) {
                              windows[i].surface->Clear( windows[i].surface, 0x00, 0x00, 0x00, 0xff );

                              windows[i].surface->Flip( windows[i].surface, NULL, DSFLIP_NONE );
                         }
                    }
                    break;

               case DWET_KEYDOWN:
                    switch (event.key_symbol) {
                         case DIKS_1:
                              windows[0].window->RequestFocus( windows[0].window );
                              break;

                         case DIKS_2:
                              windows[1].window->RequestFocus( windows[1].window );
                              break;

                         case DIKS_SMALL_X:
                              for (i=0; i<2; i++) {
                                   if (windows[i].window_id == event.window_id)
                                        windows[i].window->SetOpacity( windows[i].window, 0 );
                              }
                              break;

                         default:
                              break;
                    }
                    break;

               default:
                    break;
          }
     }

     return DFB_TRUE;
}

/**************************************************************************************************/

int
main( int argc, char *argv[] )
{
     int          i;
     DFBRectangle rects[2] = {
          { 100, 100, 200, 200 },
          { 200, 400, 200, 200 }
     };

     if (init_directfb( &argc, &argv ))
          return -1;

     for (i=0; i<2; i++) {
          if (init_window( &windows[i], &rects[i], 0x000400, DWSC_LOWER, DWCAPS_NONE, DWOP_NONE, NULL )) {
               destroy_directfb();
               return -2;
          }

          show_window( &windows[i], true );
     }

     windows[0].window->SetCursorFlags( windows[0].window, DWCF_NONE );
     windows[1].window->SetCursorFlags( windows[1].window, DWCF_INVISIBLE );

     windows[1].window->RequestFocus( windows[1].window );

     while (dispatch_events())
          m_events->WaitForEvent( m_events );

     for (i=0; i<2; i++) {
          destroy_window( &windows[i] );
     }

     destroy_directfb();

     return 0;
}

