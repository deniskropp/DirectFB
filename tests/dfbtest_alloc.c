#include <config.h>
#include <string.h>
#include <unistd.h>
#include <directfb.h>

#define ALLOC_COUNT 10000
#define WIDTH_MAX   1920
#define HEIGHT_MAX  1080

#define FONT  DATADIR "/decker.dgiff"

static unsigned int rand_pool = 0x12345678;
static unsigned int rand_add  = 0x87654321;

static inline unsigned int myrand( void )
{
     rand_pool ^= ((rand_pool << 7) | (rand_pool >> 25));
     rand_pool += rand_add;
     rand_add  += rand_pool;

     return rand_pool;
}

static int
print_usage( const char *prg )
{

     fprintf (stderr, "\n");
     fprintf (stderr, "== DirectFB Memory Allocation Test (version %s) ==\n", DIRECTFB_VERSION);
     fprintf (stderr, "\n");
     fprintf (stderr, "Usage: %s [options]\n", prg);
     fprintf (stderr, "\n");
     fprintf (stderr, "Options:\n");
     fprintf (stderr, "  -h, --help                        Show this help message\n");
     fprintf (stderr, "  -v, --version                     Print version information\n");
     fprintf (stderr, "  -l, --local                       Force use of local (system) memory\n");
     return -1;
}

/* Simple test to constantly allocate and deallocate a surface */
int main( int argc, char *argv[] )
{
    IDirectFB             *dfb;
    IDirectFBDisplayLayer *primary_layer;
    IDirectFBSurface      *primary, *image, *image2;
    IDirectFBFont         *font;
    DFBFontDescription     font_desc;
    DFBResult              err;
    DFBDisplayLayerConfig  config;
    DFBSurfaceDescription  dsc, dsc2;
    int                    i, allocs, fontheight, pitch;
    void                  *ptr;
    bool                   local_mem = false;

    /* initalize DirectFB and pass arguments */
    DirectFBInit( &argc, &argv );

    DirectFBSetOption ("bg-none", NULL);
    DirectFBSetOption ("no-init-layer", NULL);

    /* Parse arguments. */
    for (i=1; i<argc; i++) {
         const char *arg = argv[i];

         if (strcmp( arg, "-h" ) == 0 || strcmp (arg, "--help") == 0)
              return print_usage( argv[0] );
         else if (strcmp (arg, "-v") == 0 || strcmp (arg, "--version") == 0) {
              fprintf (stderr, "dfbtest_alloc version %s\n", DIRECTFB_VERSION);
              return false;
         }
         else if (strcmp (arg, "-l") == 0 || strcmp (arg, "--local") == 0) {
              local_mem = true;
         }
    }

    /* create the super interface */
    DirectFBCreate( &dfb );

    /* set cooperative level to DFSCL_FULLSCREEN for exclusive access to the
       primary layer */
    dfb->SetCooperativeLevel( dfb, DFSCL_FULLSCREEN );

    /* Get the primary surface, i.e. the surface of the primary layer. */
    dfb->GetDisplayLayer( dfb, DLID_PRIMARY, &primary_layer );

    primary_layer->SetCooperativeLevel( primary_layer, DLSCL_EXCLUSIVE );

    primary_layer->GetConfiguration (primary_layer , &config);
    config.flags = DLCONF_BUFFERMODE | DLCONF_SURFACE_CAPS;
    config.buffermode = DLBM_FRONTONLY;
    config.surface_caps = DSCAPS_PRIMARY;
    if (local_mem == true)
         config.surface_caps |= DSCAPS_SYSTEMONLY;
    primary_layer->SetConfiguration (primary_layer , &config);
    primary_layer->GetSurface( primary_layer, &primary);

    dsc.flags        = DSDESC_CAPS | DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
    dsc.pixelformat  = DSPF_ARGB;
    if (local_mem == true)
         dsc.caps         = DSCAPS_SYSTEMONLY | DSCAPS_SHARED;
    else
         dsc.caps         = DSCAPS_VIDEOONLY  | DSCAPS_SHARED;

    dsc2.flags       = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT | DSDESC_CAPS;
    dsc2.pixelformat = DSPF_ARGB;
    dsc2.caps        = dsc.caps;

    font_desc.flags  = DFDESC_HEIGHT;
    font_desc.height = 24;

    dfb->CreateFont( dfb, FONT, &font_desc, &font );
    font->GetHeight( font, &fontheight );
    primary->SetFont( primary, font );

    primary->Clear( primary, 0x20, 0x20, 0x20, 0xff );
    primary->SetDrawingFlags( primary, DSDRAW_BLEND );
    primary->SetColor( primary, 0xFF, 0xFF, 0xFF, 0xFF );
    primary->DrawString( primary, "Testing Surface Allocation/Deallocation...", -1, 50, 50, DSTF_TOPLEFT );
    sleep(2);

    for (allocs = 0; allocs < ALLOC_COUNT; allocs++)
    {
        dsc.width       = myrand() & (WIDTH_MAX-1);
        dsc.height      = myrand() & (HEIGHT_MAX-1);

        dsc.width  = dsc.width  ? dsc.width  : 2;
        dsc.height = dsc.height ? dsc.height : 2;

        /* Create surface1 ... */
        err = dfb->CreateSurface( dfb, &dsc, &image );
        if (err) {
            fprintf(stderr, "ERROR: Could not create image1!\n");
            break;
        }
        /* Now try and allocate surface1 by locking it...*/
        err = image->Lock( image, DSLF_READ|DSLF_WRITE, &ptr, &pitch);

        if (err) {
            fprintf(stderr, "ERROR: Could not lock image1!\n");
            break;
        }
        dsc2.width       = myrand() & (WIDTH_MAX-1);
        dsc2.height      = myrand() & (HEIGHT_MAX-1);

        dsc2.width  = dsc2.width  ? dsc2.width  : 2;
        dsc2.height = dsc2.height ? dsc2.height : 2;

        /* Create surface2 ... */
        err = dfb->CreateSurface( dfb, &dsc2, &image2 );
        if (err) {
            fprintf(stderr, "ERROR: Could not create image2!\n");
            image->Release( image );
            break;
        }
        /* Now try and allocate surface2 by locking it...*/
        err = image2->Lock( image2, DSLF_READ|DSLF_WRITE, &ptr, &pitch);
        if (err) {
            fprintf(stderr, "ERROR: Could not lock image2!\n");
            break;
        }

        /* Now try and unlock the surfaces...*/
        err = image->Unlock( image );
        if (err) {
            fprintf(stderr, "ERROR: Could not unlock image1!\n");
            break;
        }
        err = image2->Unlock( image2 );
        if (err) {
            fprintf(stderr, "ERROR: Could not unlock image2!\n");
            break;
        }

        /* Now release the surfaces... */
        image->Release( image );
        image2->Release( image2 );
    }
    primary->Clear( primary, 0x20, 0x20, 0x20, 0xff );
    if (allocs < ALLOC_COUNT)
        primary->DrawString( primary, "Test Failed :-(", -1, 50, 50, DSTF_TOPLEFT );
    else
        primary->DrawString( primary, "Test Passed :-)", -1, 50, 50, DSTF_TOPLEFT );

    sleep(2);
    primary->Release( primary );
    dfb->Release( dfb );
    return 0;
}
