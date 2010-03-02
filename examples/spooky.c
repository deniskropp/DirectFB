#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <math.h>

#include <directfb.h>
#include <directfb_keynames.h>

#include <divine.h>


static const DirectFBKeySymbolNames(m_symbols);


static IDiVine *m_divine;
static int      m_x;
static int      m_y;

static int      m_sin16_16[360 * 16];


static inline int
sin_16_16( int deg16_16 )
{
     return m_sin16_16[ (deg16_16 >> 12) % (360 * 16) ];
}

static inline int
cos_16_16( int deg16_16 )
{
     return m_sin16_16[ ((deg16_16 + 90 * 65536) >> 12) % (360 * 16) ];
}


typedef void (*HandlerProcess)( FILE *stream );

typedef struct {
     HandlerProcess process;
} Handler;


static void
process_button( FILE *stream )
{
     DFBInputEvent event;

     event.flags = DIEF_NONE;

     if (fgetc(stream) == '+')
          event.type = DIET_BUTTONPRESS;
     else
          event.type = DIET_BUTTONRELEASE;

     event.button = fgetc(stream) - '0';

     m_divine->SendEvent( m_divine, &event );
}

static Handler m_button = {
     .process = process_button
};


static void
process_motion( FILE *stream )
{
     DFBInputEvent event;

     event.flags = DIEF_AXISABS;
     event.type  = DIET_AXISMOTION;
     event.axis  = fgetc(stream) - 'x';

     if (fscanf( stream, "%d", &event.axisabs ) != 1)
          return;

     switch (event.axis) {
          case DIAI_X:
               m_x = event.axisabs;
               break;

          case DIAI_Y:
               m_y = event.axisabs;
               break;

          default:
               break;
     }

     m_divine->SendEvent( m_divine, &event );
}

static Handler m_motion = {
     .process = process_motion
};


static void
process_motion_special( FILE *stream )
{
     int           i, ms, ex, ey, _x = m_x, _y = m_y;
     int           count = 1;
     int           step  = 36;
     DFBInputEvent event;

     if (fscanf( stream, "%d", &ms ) != 1)
          return;

     while (!feof(stream)) {
          switch (fgetc(stream)) {
               case 'c':
                    if (fscanf( stream, "%d", &count ) != 1)
                         return;
                    break;

               case 's':
                    if (fscanf( stream, "%d", &step ) != 1)
                         return;
                    break;

               case 'o':
                    if (fscanf( stream, "%d", &ex ) != 1)
                         return;

                    for (i=0; i<=360*count; i+=step) {
                         event.flags = DIEF_AXISABS | DIEF_FOLLOW;
                         event.type  = DIET_AXISMOTION;
                         event.axis  = DIAI_X;

                         event.axisabs = _x = m_x + ((ex * cos_16_16( (i % 360) << 16 )) >> 16) - ex;

                         m_divine->SendEvent( m_divine, &event );


                         event.flags = DIEF_AXISABS;
                         event.type  = DIET_AXISMOTION;
                         event.axis  = DIAI_Y;

                         event.axisabs = _y = m_y + ((ex * sin_16_16( (i % 360) << 16 )) >> 16);

                         m_divine->SendEvent( m_divine, &event );


                         usleep( ms * 1000 );
                    }

                    m_x = _x;
                    m_y = _y;
                    break;

               case 'l':
                    if (fscanf( stream, "%d,%d", &ex, &ey ) != 2)
                         return;

                    for (i=0; i<count; i++) {
                         event.flags = DIEF_AXISABS | DIEF_FOLLOW;
                         event.type  = DIET_AXISMOTION;
                         event.axis  = DIAI_X;

                         event.axisabs = _x = m_x + ex * i;

                         m_divine->SendEvent( m_divine, &event );


                         event.flags = DIEF_AXISABS;
                         event.type  = DIET_AXISMOTION;
                         event.axis  = DIAI_Y;

                         event.axisabs = _y = m_y + ey * i;

                         m_divine->SendEvent( m_divine, &event );


                         usleep( ms * 1000 );
                    }

                    m_x = _x;
                    m_y = _y;
                    break;

               default:
                    return;
          }
     }
}

static Handler m_motion_special = {
     .process = process_motion_special
};


static void
process_pause( FILE *stream )
{
     int ms = 0;

     fscanf( stream, "%d", &ms );

     usleep( ms * 1000 );
}

static Handler m_pause = {
     .process = process_pause
};


static void
process_text( FILE *stream )
{
     int  ms      = 0;
     bool leading = true;

     fscanf( stream, "%d", &ms );

     while (!feof( stream )) {
          int c = fgetc( stream );

          if (c == ' ' && leading)
               continue;

          if (c == '\n')
               break;

          m_divine->SendSymbol( m_divine, c );

          leading = false;

          usleep( ms * 1000 );
     }
}

static Handler m_text = {
     .process = process_text
};


static void
process_text_special( FILE *stream )
{
     int  i, ms = 0;
     char buf[31];

     fscanf( stream, "%d", &ms );

     while (!feof( stream )) {
          switch (fgetc( stream )) {
               case '\n':
                    return;

               case 'b':
                    m_divine->SendSymbol( m_divine, DIKS_BACKSPACE );
                    break;

               case 'd':
                    m_divine->SendSymbol( m_divine, DIKS_DELETE );
                    break;

               case 'r':
                    m_divine->SendSymbol( m_divine, DIKS_RETURN );
                    break;

               case 'C':
                    switch (fgetc( stream )) {
                         case 'l':
                              m_divine->SendSymbol( m_divine, DIKS_CURSOR_LEFT );
                              break;

                         case 'r':
                              m_divine->SendSymbol( m_divine, DIKS_CURSOR_RIGHT );
                              break;

                         case 'u':
                              m_divine->SendSymbol( m_divine, DIKS_CURSOR_UP );
                              break;

                         case 'd':
                              m_divine->SendSymbol( m_divine, DIKS_CURSOR_DOWN );
                              break;
                    }
                    break;

               case 'S':
                    fscanf( stream, "%30s", buf );

                    for (i=0; m_symbols[i].symbol != DIKS_NULL; i++) {
                         if (!strcasecmp( buf, m_symbols[i].name )) {
                              m_divine->SendSymbol( m_divine, m_symbols[i].symbol );
                              break;
                         }
                    }
                    break;
          }

          usleep( ms * 1000 );
     }
}

static Handler m_text_special = {
     .process = process_text_special
};


static Handler *m_handlers[] = {
     ['b'] = &m_button,
     ['m'] = &m_motion,
     ['M'] = &m_motion_special,
     ['p'] = &m_pause,
     ['t'] = &m_text,
     ['T'] = &m_text_special
};


int
main( int argc, char *argv[] )
{
     DFBResult  ret;
     int        c;
     FILE      *stream;

     DiVineInit( &argc, &argv );

     if (argc > 1) {
          if (!strcmp( argv[1], "--help" ) || argc > 2) {
               fprintf( stderr, "Spooky - DiVine event generator\n"
                                "\n"
                                "     Usage: spooky [filename]\n"
                                "\n"
                                "Reads from stdin unless filename is given."
                                "\n"
                                "Commands in stream:\n"
                                " Code  Description                Arguments & Options        Examples\n"
                                "  b    Press/release button       +/-  <button>              b+0 b-0\n"
                                "  m    Motion event (absolute)    <axis> <value>             mx23 my42\n"
                                "  p    Pause spooky               <milliseconds>             p20 p1000\n"
                                "  M    Special motion generator   <ms-between-events> {o|l}  M20o50\n"
                                "                                  c <count>                  M20c1000o50\n"
                                "               (linear)           l <x,y>                    M20c1000l10,20\n"
                                "               (circular)         o <radius>                 M20c1000o100\n"
                                "                                  s <step>                   M20c1000s30o50\n"
                                "  t    Entering text              <ms> <Text to enter>       t0 directfb.org\n"
                                "  T    Special characters/keys    <ms> {b|d|r|Cx|S<symbol>}  T3 CuClbbr Sprint Sf11\n"
                                "                                        a e e Cl (left)\n"
                                "                                        c l t Cr (right)\n"
                                "                                        k     Cu (up)\n"
                                "                                              Cd (down)\n"
                                "\n" );

               return -1;
          }

          stream = fopen( argv[1], "r" );
          if (!stream) {
               fprintf( stderr, "Spooky: Could not open '%s' for reading! (%s)\n", argv[1], strerror(errno) );
               return -2;
          }
     }
     else
          stream = stdin;

     /* open the connection to the input driver */
     ret = DiVineCreate( &m_divine );
     if (ret) {
          D_DERROR( ret, "DiVineCreate() failed!\n" );
          return ret;
     }

     /* Init 1/16th step sin table */
     for (c=0; c<360 * 16; c++)
          m_sin16_16[c] = (int)(sin( (double) c / 16.0 * M_PI / 180.0 ) * 65536.0);

     /* Main loop */
     while (!feof(stream) && (c = fgetc(stream)) != '$') {
          if (c < sizeof(m_handlers)/sizeof(m_handlers[0]) && m_handlers[c])
               m_handlers[c]->process( stream );
     }

     /* close the connection */
     m_divine->Release( m_divine );

     if (stream != stdin)
          fclose( stream );

     return 0;
}

