#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <errno.h>
#include <fcntl.h>
#include <termios.h>

#include <math.h>
#include <directfb.h>

static IDirectFB *dfb = NULL;
static IDirectFBSurface *primary = NULL;
static IDirectFBEventBuffer *buffer = NULL;
static IDirectFBInputDevice *mouse = NULL;
static IDirectFBFont *gara_reg_12 = NULL;

static int screen_width  = 0;
static int screen_height = 0;

#define DFBCHECK(x...)                                       \
{                                                            \
   DFBResult err = x;                                        \
                                                             \
   if (err != DFB_OK)                                        \
   {                                                         \
      fprintf( stderr, "%s <%d>:\n\t", __FILE__, __LINE__ ); \
      DirectFBErrorFatal( #x, err );                         \
   }                                                         \
}


#define BUFFER_SIZE         64          /* size of reception buffer */
#define GUNZE_MAXPHYSCOORD  1023
#define GUNZE_MAXCOORD      (64*1024-1) /* oversampled, synthetic value */
#define FLAG_TAPPING        1
#define FLAG_WAS_UP         2
#define BAUDRATE            B9600
static const char *default_options[] =
{
	"BaudRate", "9600",
	"StopBits", "1",
	"DataBits", "8",
	"Parity", "None",
	"Vmin", "1",
	"Vtime", "10",
	"FlowControl", "None",
	NULL
};

typedef struct 
{
    char	*gunDevice;	/* device file name */
    int		flags;		/* various flags */
    int		gunType;        /* TYPE_SERIAL, etc */
    int		gunBaud;	/* 9600 or 19200 */
    int		gunDlen;	/* data length (3 or 11) */
    int		gunAvgX;	/* previous X position */
    int		gunAvgY;	/* previous Y position */
    int		gunSmooth;	/* how smooth the motion is */
    int		gunTapping;	/* move-and-tap (or press-only) */
    int		gunPrevButton;	/* previous button state */
    int		gunBytes;	/* number of bytes read */
    unsigned char gunData[16];	/* data read on the device */
    struct timeval *gunTv;      /* release time */
    char	*gunConfig;     /* filename for configuration */
    int     fd;
} GunzeDevice, *GunzeDevicePtr;

enum devicetypeitems {
    TYPE_UNKNOWN = 0,
    TYPE_SERIAL = 1,
    TYPE_PS2,
    TYPE_USB
};

#define GUNZE_SERIAL_DLEN 11
#define GUNZE_PS2_DLEN     3

#define GUNZE_SECTION_NAME    "GunzeTS"
#define GUNZE_DEFAULT_CFGFILE "/etc/gunzets.calib"


#define SYSCALL(call) while(((call) == -1) && (errno == EINTR))
struct axis_coord {
    int x;
    int y;
} coords;

static void GetCalibData(GunzeDevicePtr priv)
{
    unsigned char *pkt = priv->gunData;
    int len, loop;
    int x, y;
    coords.x = 0;
    coords.y = 0;
    unsigned char buffer[BUFFER_SIZE];

    while ((len = read( priv->fd, buffer, BUFFER_SIZE)) >= 0 || errno == EINTR)
    {
        if (len <= 0) 
        {
            fprintf(stderr,"error reading Gunze touch screen device %d %d\n",errno,priv->fd);
            perror(NULL);
            return;
        }

        for(loop=0; loop<len; loop++) 
        {
            // if first byte, ensure that the packet is syncronized
            if (priv->gunBytes == 0) 
            {
                int error  = 0;
                if (priv->gunDlen == GUNZE_SERIAL_DLEN) 
                {
                    // First byte is 'R' (0x52) or 'T' (0x54)
                    if ((buffer[loop] != 'R') && (buffer[loop] != 'T'))
                        error = 1;
                }
                else // PS/2 
                {
                    if ( !(buffer[loop] & 0x80) || (len > loop+1 && !(buffer[loop+1] & 0x80)) || (len > loop+2 && (buffer[loop+2]  & 0x80)))
                        error = 1;
                }
                if (error) 
                {
                    fprintf(stderr,"GunzeReadInput: bad first byte 0x%x %c\n",buffer[loop],buffer[loop]);
                    continue;
                }
            }

            pkt[priv->gunBytes++] = buffer[loop];

            // Hack: sometimes a serial packet gets corrupted. If so, drop it
            if (buffer[loop] == 0x0d && priv->gunBytes != priv->gunDlen && priv->gunDlen == GUNZE_SERIAL_DLEN) 
            {
                pkt[priv->gunBytes-1] = '\0';
                fprintf(stderr,"Bad packet \"%s\", dropping it\n", pkt);
                priv->gunBytes = 0; // for next time
                continue;
            }

            // if whole packet collected, decode it
            if (priv->gunBytes == priv->gunDlen)
            {
                priv->gunBytes = 0; // for next time
                if (priv->gunDlen == GUNZE_SERIAL_DLEN) 
                {
                    x = atoi((char *)pkt+1);
                    y = atoi((char *)pkt+6);
                }

                if (x>1023 || x<0 || y>1023 || y<0) 
                {
                    fprintf(stderr,"Bad packet \"%s\" -> %i,%i\n", pkt, x, y);
                    priv->gunBytes = 0; // for next time 
                    continue;
                }

                if (pkt[0] == 'R')
                {
                    printf ("Returning %c %d %d\n",pkt[0],x,y);
                    coords.x = x;
                    coords.y = y;
                    return;
                }
                printf ("Continuing %c %d %d\n",pkt[0],x,y);

                priv->gunAvgX = x;
                priv->gunAvgY = y;
            }
        }
    }
}


static int GunzeOpen(GunzeDevicePtr priv)
{
    struct termios  newtio,termios_tty;
    int err;

    // Is it a serial port or something else? 
    priv->gunType = TYPE_SERIAL;
    priv->gunDlen = GUNZE_SERIAL_DLEN;

    SYSCALL(priv->fd = open(priv->gunDevice, O_RDWR | O_NOCTTY));
    if (priv->fd == -1) 
    {
        fprintf(stderr,"Error opening device %s\n",priv->gunDevice);
        return -1;
    }

    SYSCALL(err = tcgetattr(priv->fd, &newtio));

    if (err == -1) {
        fprintf(stderr,"Gunze touch screen tcgetattr\n");
        return -1;
    }

    memset(&newtio,0, sizeof(newtio)); /* clear struct for new port settings */

/*
    BAUDRATE: Set bps rate. You could also use cfsetispeed and cfsetospeed.
    CRTSCTS : output hardware flow control (only used if the cable has
            all necessary lines. See sect. 7 of Serial-HOWTO)
    CS8     : 8n1 (8bit,no parity,1 stopbit)
    CLOCAL  : local connection, no modem contol
    CREAD   : enable receiving characters
*/
    newtio.c_cflag = BAUDRATE | CRTSCTS | CS8 | CLOCAL | CREAD;
/*
    IGNPAR  : ignore bytes with parity errors
    ICRNL   : map CR to NL (otherwise a CR input on the other computer will not terminate input)
    otherwise make device raw (no other input processing)
*/
    newtio.c_iflag = IGNPAR | ICRNL;
//  Raw output
    newtio.c_oflag = 0;
/*
    ICANON  : enable canonical input
    disable all echo functionality, and don't send signals to calling program
*/
    newtio.c_lflag = ICANON;

/*
    initialize all control characters 
    default values can be found in /usr/include/termios.h, and are given
    in the comments, but we don't need them here
*/
    newtio.c_cc[VINTR]    = 0;     /* Ctrl-c */ 
    newtio.c_cc[VQUIT]    = 0;     /* Ctrl-\ */
    newtio.c_cc[VERASE]   = 0;     /* del */
    newtio.c_cc[VKILL]    = 0;     /* @ */
    newtio.c_cc[VEOF]     = 4;     /* Ctrl-d */
    newtio.c_cc[VTIME]    = 0;     /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 1;     /* blocking read until 1 character arrives */
    newtio.c_cc[VSWTC]    = 0;     /* '\0' */
    newtio.c_cc[VSTART]   = 0;     /* Ctrl-q */ 
    newtio.c_cc[VSTOP]    = 0;     /* Ctrl-s */
    newtio.c_cc[VSUSP]    = 0;     /* Ctrl-z */
    newtio.c_cc[VEOL]     = 0;     /* '\0' */
    newtio.c_cc[VREPRINT] = 0;     /* Ctrl-r */
    newtio.c_cc[VDISCARD] = 0;     /* Ctrl-u */
    newtio.c_cc[VWERASE]  = 0;     /* Ctrl-w */
    newtio.c_cc[VLNEXT]   = 0;     /* Ctrl-v */
    newtio.c_cc[VEOL2]    = 0;     /* '\0' */

    //now clean the modem line and activate the settings for the port

    tcflush(priv->fd, TCIFLUSH);
    err = tcsetattr(priv->fd, TCSANOW, &newtio);
    if (err == -1) {
        fprintf(stderr,"Gunze touch screen tcsetattr TCSANOW\n");
        return -1;
    }

/*
    DBG(2, fprintf(stderr,"%s opened as fd %d\n", priv->gunDevice, priv->fd));
    DBG(1, fprintf(stderr,"initializing Gunze touch screen\n"));
*/
    // Hmm... close it, so it doens't say anything before we're ready
    // FIX ME

    // Clear any pending input
    tcflush(priv->fd, TCIFLUSH);
    // FIX ME: is there something to write-and-read here?

    return 0;
}

static GunzeDevicePtr GunzeAllocate()
{
    GunzeDevicePtr priv = (GunzeDevicePtr)malloc(sizeof(GunzeDevice));

    priv->gunDevice = "/dev/ttyS0";         /* device file name */
    priv->gunConfig = GUNZE_DEFAULT_CFGFILE;
    priv->gunDlen = 0;            /* unknown */
    priv->gunType = TYPE_SERIAL;
    priv->gunBaud = 9600;
    priv->gunTapping = 0;         /* default */
    priv->gunSmooth = 9;          /* quite smooth */
    priv->gunAvgX = -1;           /* previous (avg) X position */
    priv->gunAvgY = -1;           /* previous (avg) Y position */
    priv->gunPrevButton = 0;      /* previous buttons state */
    priv->flags = FLAG_WAS_UP;    /* first event is button-down */
    priv->gunBytes = 0;           /* number of bytes read */
    priv->fd = -1;

    return priv;
}

//-----------------------------------------------------------------------------------------------
void SetupFonts()
{
    DFBFontDescription font_dsc;
    font_dsc.flags = DFDESC_HEIGHT;
    font_dsc.height=25;
    DFBCHECK(dfb->CreateFont(dfb,"/usr/local/gnatfb/GARA.TTF",&font_dsc,&gara_reg_12));
}

//-----------------------------------------------------------------------------------------------
void ReleaseFonts()
{
    DFBCHECK(gara_reg_12->Release(gara_reg_12));
}

void DrawRects(char *input)
{
    int wid = screen_width; //[winfo screenwidth .]
    int hei = screen_height; //[winfo screenheight .]

    int x = screen_width/8;//[expr $wid/8]
    int y = screen_height/8;//[expr $hei/8]
    int LX = wid-x;//[expr $wid - $x]
    int LY = hei-y;//[expr $hei - $y]
    int hx = screen_width/2;//[expr $wid/2]
    int hy = screen_height/2;//[expr $hei/2]
    int cwid = 2*x;//[expr 2*$x]
    int chei = 2*y;//[expr 2*$y]

    DFBCHECK(primary->Clear(primary,55,18,5,0));
    DFBCHECK(primary->Flip(primary,NULL,DSFLIP_NONE));
    DFBCHECK(primary->Clear(primary,55,18,5,0));
    DFBCHECK(primary->SetFont(primary,gara_reg_12));

    DFBCHECK(primary->SetColor(primary,0xFF,0xEF,0xB5,0));
    //Bottom Rectangle
    DFBCHECK(primary->DrawRectangle(primary,50,hei-chei,cwid,chei));
    // Top Rectangle
    DFBCHECK(primary->DrawRectangle(primary,wid-cwid-50,0,cwid,chei));

    // Draw Two Cross Hairs
    DFBCHECK(primary->DrawLine(primary,50,hei-chei+50,150,hei-chei+50));
    DFBCHECK(primary->DrawLine(primary,100,hei-chei,100,hei-chei+100));

    DFBCHECK(primary->DrawLine(primary,wid-(cwid/2), 0 ,wid-(cwid/2), 100));
    DFBCHECK(primary->DrawLine(primary,wid-(cwid/2)-50, 50 ,wid-(cwid/2)+50, 50));
    DFBCHECK(primary->DrawString(primary,input,-1,screen_width/2,screen_height/2,DSTF_CENTER));
    DFBCHECK(primary->Flip(primary,NULL,DSFLIP_NONE));
    sleep(1);
}

//-----------------------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    int quit = 0;

    DFBSurfaceDescription dsc;
    DFBCHECK (DirectFBInit (&argc, &argv));
    DFBCHECK (DirectFBCreate (&dfb));

    DFBCHECK (dfb->SetCooperativeLevel (dfb,DFSCL_EXCLUSIVE));// DFSCL_FULLSCREEN));
    DFBCHECK (dfb->SetVideoMode(dfb,1024,768,16));
    dsc.flags = DSDESC_CAPS | DSDESC_WIDTH | DSDESC_HEIGHT;
    dsc.caps = DSCAPS_PRIMARY | DSCAPS_FLIPPING;
    dsc.width = 1024;
    dsc.height = 768;

    // Surface Creation
    DFBCHECK (dfb->CreateSurface(dfb, &dsc, &primary));
    DFBCHECK (primary->GetSize(primary, &screen_width, &screen_height));
    DFBCHECK (primary->SetBlittingFlags(primary,DSBLIT_BLEND_ALPHACHANNEL));

    // Event Handlers
    DFBCHECK(dfb->CreateInputEventBuffer(dfb,DICAPS_ALL,DFB_FALSE,&buffer));
    //Cursor

    SetupFonts();

    GunzeDevice *priv = GunzeAllocate();
    if(GunzeOpen(priv) != 0){
        fprintf(stderr,"Error Opening device!\n");
        return -1;
    }

    //The offset variables account for window manager borders etc

    //DFBCHECK(primary->DrawLine(primary,wid-cwid-50, 0 ,wid-cwid+50, 50));
    DrawRects("Touch Left Cross");

    GetCalibData(priv);
    int x1 = coords.x;
    int y1 = coords.y;
    DrawRects("Touch Right Cross");

    GetCalibData(priv);
    int x2 = coords.x;
    int y2 = coords.y;

    DrawRects("Done");
    char tmp[100];
    memset(tmp,0,100);
    sprintf(tmp,"X1:%d Y1:%d X2:%d Y2:%d",x1,y1,x2,y2);
    DFBCHECK(primary->DrawString(primary,tmp,-1,screen_width/2,(screen_height/2)-150,DSTF_CENTER));
    DFBCHECK(primary->Flip(primary,NULL,DSFLIP_NONE));
    memset(tmp,0,100);
    sprintf(tmp,"# Calibration coordinates for Gunze Device\n%d %d %d %d",x1,y1,x2,y2);
    int fd = 0;
    SYSCALL(fd = open(priv->gunConfig, O_RDWR | O_TRUNC | O_CREAT));
    SYSCALL(write(fd,tmp,strlen(tmp)));
    close(fd);

    DFBInputEvent event;

    while(!quit)
    {
        buffer->WaitForEvent(buffer);
        while ( buffer->GetEvent(buffer, DFB_EVENT(&event)) == DFB_OK)
        {
            if(event.type == DIET_KEYPRESS)
            {
                if(event.key_id == DIKI_ESCAPE || event.key_id == DIKI_Q)
                {
                    printf("Q||ESC KEY pressed event\n");
                    fflush(NULL);

                    quit = 1;
                }
            }
        }
    }

    close(priv->fd);
    free(priv);

    ReleaseFonts();
    if(mouse != NULL)
        mouse->Release(mouse);

    buffer->Release(buffer);
    primary->Release(primary);
    dfb->Release(dfb);
    return 0;
}
