/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#ifndef __DIRECTFB_H__
#define __DIRECTFB_H__

#include <asm/types.h>
#include <sys/time.h> /* struct timeval */

#include <directfb_keyboard.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * Forward declaration macro for interfaces.
 */
#define DECLARE_INTERFACE( IFACE )                \
     struct _##IFACE;                             \
     typedef struct _##IFACE IFACE;

/*
 * Macro for an interface definition.
 */
#define DEFINE_INTERFACE( IFACE, IDATA... )       \
     struct _##IFACE     {                        \
          void       *priv;                       \
          DFBResult (*AddRef)( IFACE *thiz );     \
          DFBResult (*Release)( IFACE *thiz );    \
                                                  \
          IDATA                                   \
     };


     /*
      * Version handling.
      */
     extern const unsigned int directfb_major_version;
     extern const unsigned int directfb_minor_version;
     extern const unsigned int directfb_micro_version;
     extern const unsigned int directfb_binary_age;
     extern const unsigned int directfb_interface_age;

     /*
      * Check against certain DirectFB version.
      * In case of an error a message is returned describing the mismatch.
      */
     char * DirectFBCheckVersion (unsigned int required_major,
                                  unsigned int required_minor,
                                  unsigned int required_micro);


     /*
      * The only interface with a global "Create" function,
      * any other functionality goes from here.
      */
     DECLARE_INTERFACE( IDirectFB )

     /*
      * Layer configuration, creation of windows and background
      * configuration.
      */
     DECLARE_INTERFACE( IDirectFBDisplayLayer )

     /*
      * Surface locking, setting colorkeys and other drawing
      * parameters, clipping, flipping, blitting, drawing.
      */
     DECLARE_INTERFACE( IDirectFBSurface )

     /*
      * Moving, resizing, raising and lowering.
      * Getting an interface to the window's surface.
      * Setting opacity and handling events.
      */
     DECLARE_INTERFACE( IDirectFBWindow )

     /*
      * Creation of input buffers and explicit state queries.
      */
     DECLARE_INTERFACE( IDirectFBInputDevice )

     /*
      * An even buffer puts events from devices or windows into a FIFO.
      */
     DECLARE_INTERFACE( IDirectFBEventBuffer )

     /*
      * Getting font metrics and pixel width of a string.
      */
     DECLARE_INTERFACE( IDirectFBFont )

     /*
      * Getting information about and loading one image from file.
      */
     DECLARE_INTERFACE( IDirectFBImageProvider )

     /*
      * Rendering video data into a surface.
      */
     DECLARE_INTERFACE( IDirectFBVideoProvider )

     /*
      * OpenGL context of a surface.
      */
     DECLARE_INTERFACE( IDirectFBGL )


     /*
      * DirectFB interface functions return code.
      */
     typedef enum {
          DFB_OK,             /* no error */
          DFB_FAILURE,        /* general/unknown error, should not be used */
          DFB_INIT,           /* general initialization error */
          DFB_BUG,            /* internal bug */
          DFB_DEAD,           /* dead interface */
          DFB_UNSUPPORTED,    /* not supported */
          DFB_UNIMPLEMENTED,  /* yet unimplemented */
          DFB_ACCESSDENIED,   /* access denied */
          DFB_INVARG,         /* invalid argument */
          DFB_NOSYSTEMMEMORY, /* out of system memory */
          DFB_NOVIDEOMEMORY,  /* out of video memory */
          DFB_LOCKED,         /* resource locked */
          DFB_BUFFEREMPTY,    /* buffer is empty */
          DFB_FILENOTFOUND,   /* file not found */
          DFB_IO,             /* general I/O error */
          DFB_BUSY,           /* resource/device busy */
          DFB_NOIMPL,         /* no implementation for that interface */
          DFB_MISSINGFONT,    /* no font has been set */
          DFB_TIMEOUT,        /* operation timed out */
          DFB_MISSINGIMAGE,   /* no image has been set */
          DFB_THIZNULL,       /* 'thiz' pointer is NULL */
          DFB_IDNOTFOUND,     /* layer/device/... id not found */
          DFB_INVAREA,        /* invalid area specified or detected */
          DFB_DESTROYED       /* object (e.g. a window) has been destroyed */
     } DFBResult;

     /*
      * Point specified by x/y coordinates.
      */
     typedef struct {
          int            x;   /* X coordinate of it */
          int            y;   /* Y coordinate of it */
     } DFBPoint;

     /*
      * Dimension specified by width and height.
      */
     typedef struct {
          int            w;   /* width of it */
          int            h;   /* height of it */
     } DFBDimension;

     /*
      * Rectangle specified by a point and a dimension.
      */
     typedef struct {
          int            x;   /* X coordinate of its top-left point */
          int            y;   /* Y coordinate of its top-left point */
          int            w;   /* width of it */
          int            h;   /* height of it */
     } DFBRectangle;

     /*
      * Region specified by two points.
      *
      * The defined region includes both endpoints.
      */
     typedef struct {
          int            x1;  /* X coordinate of top-left point */
          int            y1;  /* Y coordinate of top-left point */
          int            x2;  /* X coordinate of lower-right point */
          int            y2;  /* Y coordinate of lower-right point */
     } DFBRegion;

     /*
      * Triangle.
      */
     typedef struct {
          int            x1;  /* X coordinate of first edge */
          int            y1;  /* Y coordinate of first edge */
          int            x2;  /* X coordinate of second edge */
          int            y2;  /* Y coordinate of second edge */
          int            x3;  /* X coordinate of third edge */
          int            y3;  /* Y coordinate of third edge */
     } DFBTriangle;

     /*
      * A color defined by channels with 8bit each.
      */
     typedef struct {
          __u8           a;   /* alpha channel */
          __u8           r;   /* red channel */
          __u8           g;   /* green channel */
          __u8           b;   /* blue channel */
     } DFBColor;

     /*
      * Print a description of the result code along with an
      * optional message that is put in front with a colon.
      */
     void DirectFBError(
                              const char  *msg,    /* optional message */
                              DFBResult    result  /* result code to interpret */
                       );

     /*
      * Behaves like DirectFBError, but shuts down the calling application.
      */
     void DirectFBErrorFatal(
                              const char  *msg,    /* optional message */
                              DFBResult    result  /* result code to interpret */
                           );

     /*
      * Returns a string describing 'result'.
      */
     const char *DirectFBErrorString(
                              DFBResult    result
                           );

     /*
      * Retrieves information about supported command-line flags in the
      * form of a user-readable string formatted suitable to be printed
      * as usage information.
      */
     const char *DirectFBUsageString( void );

     /*
      * Parses the command-line and initializes some variables. You
      * absolutely need to call this before doing anything else.
      * Removes all options used by DirectFB from argv.
      */
     DFBResult DirectFBInit(
                              int         *argc,   /* main()'s argc */
                              char       **argv[]  /* main()'s argv */
                           );

     /*
      * Sets configuration parameters supported on command line and in
      * config file. Has to be called before DirectFBCreate but after
      * DirectFBInit.
      */
     DFBResult DirectFBSetOption(
                              char        *name,
                              char        *value
                           );

     /*
      * Creates the super interface
      */
     DFBResult DirectFBCreate(
                               IDirectFB **interface  /* pointer to the
                                                         created interface */
                             );



     /*
      * The cooperative level controls the super interface's behaviour
      * in functions like SetVideoMode or CreateSurface for the primary.
      */
     typedef enum {
          DFSCL_NORMAL        = 0x00000000,  /* normal shared access, primary
                                                surface will be the backbuffer
                                                of an implicitly created window
                                                at the resolution given by
                                                SetVideoMode() */
          DFSCL_FULLSCREEN,                  /* application grabs the primary
                                                layer, SetVideoMode automates
                                                layer control, primary surface
                                                is the primary layer surface */
          DFSCL_EXCLUSIVE                    /* all but the primary layer will
                                                be disabled, application has
                                                full control over layers if
                                                desired, other applications
                                                have no input/output/control,
                                                primary surface is the primary
                                                layer surface */
     } DFBCooperativeLevel;

     /*
      * Capabilities of a display layer
      */
     typedef enum {
          DLCAPS_SURFACE           = 0x00000001,  /* The layer has a surface
                                                     that can be drawn to. This
                                                     may not be provided by
                                                     layers that only display
                                                     realtime data, e.g. from an
                                                     MPEG decoder chip. */
          DLCAPS_OPACITY           = 0x00000002,  /* The layer supports blending
                                                     with layer(s) below by
                                                     a global alpha factor. */
          DLCAPS_ALPHACHANNEL      = 0x00000004,  /* The layer supports blending
                                                     with layer(s) below on
                                                     a pixel per pixel basis. */
          DLCAPS_SCREEN_LOCATION   = 0x00000008,  /* The layer location on the
                                                     screen can be changed, this
                                                     includes position and size
                                                     as normalized values,
                                                     default is 0, 0, 1, 1. */
          DLCAPS_FLICKER_FILTERING = 0x00000010,  /* Flicker filtering can be
                                                     enabled for this layer. */
          DLCAPS_INTERLACED_VIDEO  = 0x00000020,  /* The layer can display
                                                     interlaced video data. */
          DLCAPS_SRC_COLORKEY      = 0x00000040,  /* A specific color can be
                                                     declared as transparent. */
          DLCAPS_DST_COLORKEY      = 0x00000080,  /* A specific color can be
                                                     declared as transparent. */
          DLCAPS_BRIGHTNESS        = 0x00000100,  /* supports Brightness
                                                     adjustment */
          DLCAPS_CONTRAST          = 0x00000200,  /* supports Contrast
                                                     adjustment   */
          DLCAPS_HUE               = 0x00000400,  /* supports Hue adjustment */
          DLCAPS_SATURATION        = 0x00000800,  /* supports Saturation
                                                     adjustment */
          DLCAPS_LEVELS            = 0x00001000   /* level (z position) can be
                                                     adjusted */
     } DFBDisplayLayerCapabilities;

     /*
      * Used to enable some capabilities like flicker filtering or
      * colorkeying. Usage of capabilities not listed here depend on
      * their settings, e.g. if layer opacity is not set to 100% the
      * opacity capability is required and used.
      */
     typedef enum {
          DLOP_NONE                = 0x00000000,  /* None of these. */
          DLOP_ALPHACHANNEL        = 0x00000001,  /* Make usage of alpha channel
                                                     for blending on a pixel per
                                                     pixel basis. */
          DLOP_FLICKER_FILTERING   = 0x00000002,  /* Enable flicker
                                                     filtering. */
          DLOP_INTERLACED_VIDEO    = 0x00000004,  /* Source is interlaced. */
          DLOP_SRC_COLORKEY        = 0x00000008,  /* Enable source color key. */
          DLOP_DST_COLORKEY        = 0x00000010   /* Enable dest. color key. */
     } DFBDisplayLayerOptions;

     /*
      * Layer Buffer Mode
      */
     typedef enum {
          DLBM_FRONTONLY  = 0x00000000,      /* no backbuffer */
          DLBM_BACKVIDEO  = 0x00000001,      /* backbuffer in video memory */
          DLBM_BACKSYSTEM = 0x00000002       /* backbuffer in system memory */
     } DFBDisplayLayerBufferMode;

     /*
      * Flags defining which fields of a DFBSurfaceDescription are valid.
      */
     typedef enum {
          DSDESC_CAPS         = 0x00000001,  /* caps field is valid */
          DSDESC_WIDTH        = 0x00000002,  /* width field is valid */
          DSDESC_HEIGHT       = 0x00000004,  /* height field is valid */
          DSDESC_PIXELFORMAT  = 0x00000008,  /* pixelformat field is valid */
          DSDESC_PREALLOCATED = 0x00000010   /* Surface uses data that has been
                                                preallocated by the application.
                                                The field array 'preallocated'
                                                has to be set using the first
                                                element for the front buffer
                                                and eventually the second one
                                                for the back buffer. */
     } DFBSurfaceDescriptionFlags;

     /*
      * Capabilities of a surface.
      */
     typedef enum {
          DSCAPS_NONE         = 0x00000000,

          DSCAPS_PRIMARY      = 0x00000001,  /* it's the primary surface */
          DSCAPS_SYSTEMONLY   = 0x00000002,  /* permanently stored in system
                                                memory, no video memory
                                                allocation/storage */
          DSCAPS_VIDEOONLY    = 0x00000004,  /* permanently stored in video
                                                memory, no system memory
                                                allocation/storage */
          DSCAPS_FLIPPING     = 0x00000010,  /* surface is double buffered
                                                (use Flip to make changes
                                                 visible/usable) */
          DSCAPS_SUBSURFACE   = 0x00000020,  /* surface is just a sub area of
                                                another one */
          DSCAPS_INTERLACED   = 0x00000040   /* each surface buffer contains
                                                interlaced video data
                                                consisting of two fields which
                                                lines are interleaved,
                                                one field's height is a half
                                                of the surface's height */
     } DFBSurfaceCapabilities;

     /*
      * Flags controlling drawing commands.
      */
     typedef enum {
          DSDRAW_NOFX         = 0x00000000,  /* uses none of the effects */
          DSDRAW_BLEND        = 0x00000001,  /* uses source alpha from color */
          DSDRAW_DST_COLORKEY = 0x00000002   /* write to destination only if
                                                destination color key matches */
     } DFBSurfaceDrawingFlags;

     /*
      * Flags controlling blitting commands.
      */
     typedef enum {
          DSBLIT_NOFX               = 0x00000000, /* uses none of the effects */
          DSBLIT_BLEND_ALPHACHANNEL = 0x00000001, /* enables blending and uses
                                                     alphachannel from source */
          DSBLIT_BLEND_COLORALPHA   = 0x00000002, /* enables blending and uses
                                                     alpha value from color */
          DSBLIT_COLORIZE           = 0x00000004, /* modulates source color with
                                                     the color's r/g/b values */
          DSBLIT_SRC_COLORKEY       = 0x00000008, /* only blit pixels matching
                                                     the source color key */
          DSBLIT_DST_COLORKEY       = 0x00000010, /* write to destination only
                                                     if destination color
                                                     key matches */
          DSBLIT_SRC_PREMULTIPLY    = 0x00000020, /* modulates the source color
                                                     with the source alpha */
          DSBLIT_DST_PREMULTIPLY    = 0x00000040, /* modulates the dest. color
                                                     with the dest. alpha */
          DSBLIT_DEMULTIPLY         = 0x00000080  /* divides the color by the
                                                     alpha before writing the
                                                     data to the destination */
     } DFBSurfaceBlittingFlags;

     /*
      * Mask of accelerated functions
      */
     typedef enum {
          DFXL_NONE           = 0x00000000,  /* none of these */
          DFXL_FILLRECTANGLE  = 0x00000001,  /* FillRectangle */
          DFXL_DRAWRECTANGLE  = 0x00000002,  /* DrawRectangle */
          DFXL_DRAWLINE       = 0x00000004,  /* DrawLine */
          DFXL_FILLTRIANGLE   = 0x00000008,  /* FillTriangle */

          DFXL_BLIT           = 0x00010000,  /* Blit */
          DFXL_STRETCHBLIT    = 0x00020000,  /* StretchBlit */

          DFXL_ALL            = 0x0003000F   /* all drawing/blitting
                                                functions */
     } DFBAccelerationMask;

     #define DFB_DRAWING_FUNCTION(a)    ((a) & 0x0000FFFF)
     #define DFB_BLITTING_FUNCTION(a)   ((a) & 0xFFFF0000)

     /*
      * Rough information about hardware capabilities.
      */
     typedef struct {
          DFBAccelerationMask     acceleration_mask;   /* drawing/blitting
                                                          functions */
          DFBSurfaceDrawingFlags  drawing_flags;       /* drawing flags */
          DFBSurfaceBlittingFlags blitting_flags;      /* blitting flags */
          unsigned int            video_memory;        /* amount of video
                                                          memory in bytes */
     } DFBCardCapabilities;

     /*
      * Type of input device for basic classification.
      * Values may be or'ed together.
      */
     typedef enum {
          DIDTF_KEYBOARD      = 0x00000001,  /* can act as a keyboard */
          DIDTF_MOUSE         = 0x00000002,  /* can be used as a mouse */
          DIDTF_JOYSTICK      = 0x00000004,  /* can be used as a joystick */
          DIDTF_REMOTE        = 0x00000008,  /* device is a remote control */
          DIDTF_VIRTUAL       = 0x00000010,  /* virtual input device */

          DIDTF_ALL           = 0x0000001F   /* all type flags */
     } DFBInputDeviceTypeFlags;

     /*
      * Basic input device features.
      */
     typedef enum {
          DICAPS_KEYS         = 0x00000001,  /* device supports key events */
          DICAPS_AXES         = 0x00000002,  /* device supports axis events */
          DICAPS_BUTTONS      = 0x00000004,  /* device supports button events */

          DICAPS_ALL          = 0x00000007   /* all capabilities */
     } DFBInputDeviceCapabilities;

     /*
      * Identifier (index) for e.g. mouse or joystick buttons.
      */
     typedef enum {
          DIBI_LEFT           = 0x00000000,  /* left mouse button */
          DIBI_RIGHT          = 0x00000001,  /* right mouse button */
          DIBI_MIDDLE         = 0x00000002,  /* middle mouse button */

          DIBI_FIRST          = DIBI_LEFT,   /* other buttons:
                                                DIBI_FIRST + zero based index */
          DIBI_LAST           = 0x0000001F   /* 32 buttons maximum */
     } DFBInputDeviceButtonIdentifier;

     /*
      * Axis identifier (index) for e.g. mouse or joystick.
      *
      * The X, Y and Z axis are predefined. To access other axes,
      * use DIAI_FIRST plus a zero based index, e.g. the 4th axis
      * would be (DIAI_FIRST + 3).
      */
     typedef enum {
          DIAI_X              = 0x00000000,  /* X axis */
          DIAI_Y              = 0x00000001,  /* Y axis */
          DIAI_Z              = 0x00000002,  /* Z axis */

          DIAI_FIRST          = DIAI_X,      /* other axis:
                                                DIAI_FIRST + zero based index */
          DIAI_LAST           = 0x0000001F   /* 32 axes maximum */
     } DFBInputDeviceAxisIdentifier;

     /*
      * Flags defining which fields of a DFBWindowDescription are valid.
      */
     typedef enum {
          DWDESC_CAPS         = 0x00000001,  /* caps field is valid */
          DWDESC_WIDTH        = 0x00000002,  /* width field is valid */
          DWDESC_HEIGHT       = 0x00000004,  /* height field is valid */
          DWDESC_PIXELFORMAT  = 0x00000008,  /* pixelformat field is valid */
          DWDESC_POSX         = 0x00000010,  /* posx field is valid */
          DWDESC_POSY         = 0x00000020   /* posy field is valid */
     } DFBWindowDescriptionFlags;

     /*
      * Capabilities a window can have.
      */
     typedef enum {
          DWCAPS_NONE         = 0x00000000,  /* None of these. */
          DWCAPS_ALPHACHANNEL = 0x00000001,  /* The window has an alphachannel
                                                for pixel-per-pixel blending. */
          DWCAPS_DOUBLEBUFFER = 0x00000002,  /* The window's surface is double
                                                buffered. This is very useful
                                                to avoid visibility of content
                                                that is still in preparation.
                                                Normally a window's content can
                                                get visible before an update if
                                                there is another reason causing
                                                a window stack repaint. */
          DWCAPS_ALL          = 0x00000003   /* All valid flags. */
     } DFBWindowCapabilities;


     /*
      * Flags describing how to load a font.
      *
      * These flags describe how a font is loaded and affect how the
      * glyphs are drawn. There is no way to change this after the font
      * has been loaded. If you need to render a font with different
      * attributes, you have to create multiple FontProviders of the
      * same font file.
      */
     typedef enum {
           DFFA_NOKERNING     = 0x00000001,  /* don't use kerning */
           DFFA_NOHINTING     = 0x00000002,  /* don't use hinting */
           DFFA_MONOCHROME    = 0x00000004   /* don't use anti-aliasing */
     } DFBFontAttributes;

     /*
      * Flags defining which fields of a DFBFontDescription are valid.
      */
     typedef enum {
          DFDESC_ATTRIBUTES   = 0x00000001,  /* attributes field is valid */
          DFDESC_HEIGHT       = 0x00000002,  /* height is specified */
          DFDESC_WIDTH        = 0x00000004,  /* width is specified */
          DFDESC_INDEX        = 0x00000008   /* index is specified */
     } DFBFontDescriptionFlags;

     /*
      * Description of how to load glyphs from a font file.
      *
      * The attributes control how the glyphs are rendered. Width and
      * height can be used to specify the desired face size in pixels.
      * If you are loading a non-scalable font, you shouldn't specify
      * a font size. Please note that the height value in the
      * FontDescription doesn't correspond to the height returned by
      * the font's GetHeight() method.
      * 
      * The index field controls which face is loaded from a font file
      * that provides a collection of faces. This is rarely needed.
      */
     typedef struct {
          DFBFontDescriptionFlags            flags;

          DFBFontAttributes                  attributes;
          unsigned int                       height;
          unsigned int                       width;
          unsigned int                       index;
     } DFBFontDescription;

     /*
      * Pixel format of a surface.
      * Contains information about the format (see following definition).
      *
      * Format constants are encoded in the following way (bit 31 - 0):
      *
      * --ff:eeee | dddd:ccc- | bbbb:bbbb | aaaa:aaaa
      *
      * a) pixelformat index<br>
      * b) effective bits per pixel of format<br>
      * c) bytes per pixel in a row (1/8 fragment, i.e. bits)<br>
      * d) bytes per pixel in a row (decimal part, i.e. bytes)<br>
      * e) multiplier for planes minus one (1/16 fragment)<br>
      * f) multiplier for planes minus one (decimal part)
      */
     typedef enum {
          DSPF_UNKNOWN        = 0x00000000,  /* no specific format,
                                                unusual and unsupported */
          DSPF_RGB15          = 0x00200F01,  /* 15bit  RGB (2 bytes, red 5@10,
                                                green 5@5, blue 5@0) */
          DSPF_RGB16          = 0x00201002,  /* 16bit  RGB (2 bytes, red 5@11,
                                                green 6@5, blue 5@0) */
          DSPF_RGB24          = 0x00301803,  /* 24bit  RGB (3 bytes, red 8@16,
                                                green 8@8, blue 8@0) */
          DSPF_RGB32          = 0x00401804,  /* 24bit  RGB (4 bytes, nothing@24,
                                                red 8@16, green 8@8, blue 8@0)*/
          DSPF_ARGB           = 0x00402005,  /* 32bit ARGB (4 bytes, alpha 8@24,
                                                red 8@16, green 8@8, blue 8@0)*/
          DSPF_A8             = 0x00100806,  /* 8bit alpha (1 byte, alpha 8@0 ),
                                                e.g. anti-aliased text glyphs */
          DSPF_YUY2           = 0x00201007,  /* A macropixel (32bit / 2 pixel)
                                                contains YUYV (starting with
                                                the LOWEST byte on the LEFT) */
          DSPF_RGB332         = 0x00100808,  /* 8bit true color (1 byte,
                                                red 3@5, green 3@2, blue 2@0 */
          DSPF_UYVY           = 0x00201009,  /* A macropixel (32bit / 2 pixel)
                                                contains UYVY (starting with
                                                the LOWEST byte on the LEFT) */
          DSPF_I420           = 0x08100C0A,  /* 8 bit Y plane followed by 8 bit
                                                2x2 subsampled U and V planes */
          DSPF_YV12           = 0x08100C0B   /* 8 bit Y plane followed by 8 bit
                                                2x2 subsampled V and U planes */
     } DFBSurfacePixelFormat;

     /* Number of pixelformats defined */
     #define DFB_NUM_PIXELFORMATS            11

     /* These macros extract information about the pixel format. */
     #define DFB_BYTES_PER_PIXEL(fmt)        (((fmt) & 0xF00000) >> 20)

     #define DFB_BITS_PER_PIXEL(fmt)         (((fmt) & 0x00FF00) >>  8)

     #define DFB_PIXELFORMAT_INDEX(fmt)      (((fmt) & 0x0000FF) - 1)

     #define DFB_BYTES_PER_LINE(fmt,width)   (((((fmt) & 0xFE0000) >> 17) * \
                                               (width)) >> 3)

     #define DFB_PLANAR_PIXELFORMAT(fmt)     ((fmt) & 0x3F000000)

     #define DFB_PLANE_MULTIPLY(fmt,height)  ((((((fmt) & 0x3F000000) >> 24) + \
                                                0x10) * (height)) >> 4 )

     /*
      * Description of the surface that is to be created.
      */
     typedef struct {
          DFBSurfaceDescriptionFlags         flags;      /* field validation */

          DFBSurfaceCapabilities             caps;        /* capabilities */
          unsigned int                       width;       /* pixel width */
          unsigned int                       height;      /* pixel height */
          DFBSurfacePixelFormat              pixelformat; /* pixel format */

          struct {
               void                         *data;        /* data pointer of
                                                             existing buffer */
               int                           pitch;       /* pitch of buffer */
          } preallocated[2];
     } DFBSurfaceDescription;

     /*
      * Description of the input device capabilities.
      */
     typedef struct {
          DFBInputDeviceTypeFlags            type;        /* classification of
                                                             input device */
          DFBInputDeviceCapabilities         caps;        /* capabilities,
                                                             validates the
                                                             following fields */

          int                                min_keycode; /* minimum hardware
                                                             keycode or -1 if
                                                             no differentiation
                                                             between hardware
                                                             keys is made */
          int                                max_keycode; /* maximum hardware
                                                             keycode or -1 if
                                                             no differentiation
                                                             between hardware
                                                             keys is made */
          DFBInputDeviceAxisIdentifier       max_axis;    /* highest axis
                                                             identifier */
          DFBInputDeviceButtonIdentifier     max_button;  /* highest button
                                                             identifier */
     } DFBInputDeviceDescription;

     /*
      * Description of the window that is to be created.
      */
     typedef struct {
          DFBWindowDescriptionFlags          flags;       /* field validation */

          DFBWindowCapabilities              caps;        /* capabilities */
          unsigned int                       width;       /* pixel width */
          unsigned int                       height;      /* pixel height */
          DFBSurfacePixelFormat              pixelformat; /* pixel format */
          int                                posx;        /* distance from left
                                                             layer border */
          int                                posy;        /* distance from upper
                                                             layer border */
     } DFBWindowDescription;

     /*
      * Return value of callback function of enumerations.
      */
     typedef enum {
          DFENUM_OK           = 0x00000000,  /* Proceed with enumeration */
          DFENUM_CANCEL       = 0x00000001   /* Cancel enumeration */
     } DFBEnumerationResult;

     typedef unsigned int DFBDisplayLayerID;
     typedef unsigned int DFBWindowID;
     typedef unsigned int DFBInputDeviceID;

     /*
      * Called for each supported video mode.
      */
     typedef DFBEnumerationResult (*DFBVideoModeCallback) (
          unsigned int                       width,
          unsigned int                       height,
          unsigned int                       bpp,
          void                              *callbackdata
     );

     /*
      * Called for each existing display layer.
      * "layer_id" can be used to get an interface to the layer.
      */
     typedef DFBEnumerationResult (*DFBDisplayLayerCallback) (
          DFBDisplayLayerID                  layer_id,
          DFBDisplayLayerCapabilities        caps,
          void                              *callbackdata
     );

     /*
      * Called for each existing input device.
      * "device_id" can be used to get an interface to the device.
      */
     typedef DFBEnumerationResult (*DFBInputDeviceCallback) (
          DFBInputDeviceID                   device_id,
          DFBInputDeviceDescription          desc,
          void                              *callbackdata
     );

     /*
      * Called for each block of continous data requested, e.g. by a
      * Video Provider. Write as many data as you can but not more
      * than specified by length. Return the number of bytes written
      * or 'EOF' if no data is available anymore.
      */
     typedef int (*DFBGetDataCallback) (
          void                              *buffer,
          unsigned int                       length,
          void                              *callbackdata
     );

     /*
      * Information about an IDirectFBVideoProvider.
      */
     typedef enum {
          DVCAPS_BASIC      = 0x00000000,  /* basic ops (PlayTo, Stop)       */
          DVCAPS_SEEK       = 0x00000001,  /* supports SeekTo                */
          DVCAPS_SCALE      = 0x00000002,  /* can scale the video            */
          DVCAPS_INTERLACED = 0x00000004,  /* supports interlaced surfaces   */
          DVCAPS_BRIGHTNESS = 0x00000010,  /* supports Brightness adjustment */
          DVCAPS_CONTRAST   = 0x00000020,  /* supports Contrast adjustment   */
          DVCAPS_HUE        = 0x00000040,  /* supports Hue adjustment        */
          DVCAPS_SATURATION = 0x00000080   /* supports Saturation adjustment */
     } DFBVideoProviderCapabilities;

     /*
      * Flags defining which fields of a DFBColorAdjustment are valid.
      */
     typedef enum {
          DCAF_NONE         = 0x00000000,  /* none of these              */
          DCAF_BRIGHTNESS   = 0x00000001,  /* brightness field is valid  */
          DCAF_CONTRAST     = 0x00000002,  /* contrast field is valid    */
          DCAF_HUE          = 0x00000004,  /* hue field is valid         */
          DCAF_SATURATION   = 0x00000008   /* saturation field is valid  */
     } DFBColorAdjustmentFlags;

     /*
      * Color Adjustment used to adjust video colors.
      *
      * All fields are in the range 0x0 to 0xFFFF with
      *  0x8000 as default value (no adjustment).
      */
     typedef struct {
          DFBColorAdjustmentFlags  flags;

          __u16                    brightness;
          __u16                    contrast;
          __u16                    hue;
          __u16                    saturation;
     } DFBColorAdjustment;


     /*
      * <i><b>IDirectFB</b></i> is the main interface. It can be
      * retrieved by a call to <i>DirectFBCreate</i>. It's the only
      * interface with a global creation facility. Other interfaces
      * are created by this interface or interfaces created by it.
      *
      * <b>Hardware capabilities</b> such as the amount of video
      * memory or a list of supported drawing/blitting functions and
      * flags can be retrieved.  It also provides enumeration of all
      * supported video modes.
      *
      * <b>Input devices</b> and <b>display layers</b> that are
      * present can be enumerated via a callback mechanism. The
      * callback is given the capabilities and the device or layer
      * ID. An interface to specific input devices or display layers
      * can be retrieved by passing the device or layer ID to the
      * corresponding method.
      *
      * <b>Surfaces</b> for general purpose use can be created via
      * <i>CreateSurface</i>. These surfaces are so called "offscreen
      * surfaces" and could be used for sprites or icons.
      *
      * The <b>primary surface</b> is an abstraction and API shortcut
      * for getting a surface for visual output. Fullscreen games for
      * example have the whole screen as their primary
      * surface. Alternatively fullscreen applications can be forced
      * to run in a window. The primary surface is also created via
      * <i>CreateSurface</i> but with the special capability
      * DSCAPS_PRIMARY.
      *
      * The <b>cooperative level</b> selects the type of the primary
      * surface.  With a call to <i>SetCooperativeLevel</i> the
      * application can choose between the surface of an implicitly
      * created window and the surface of the primary layer
      * (deactivating the window stack). The application doesn't need
      * to have any extra functionality to run in a window. If the
      * application is forced to run in a window the call to
      * <i>SetCooperativeLevel</i> fails with DFB_ACCESSDENIED.
      * Applications that want to be "window aware" shouldn't exit on
      * this error.
      *
      * The <b>video mode</b> can be changed via <i>SetVideoMode</i>
      * and is the size and depth of the primary surface, i.e. the
      * screen when in exclusive cooperative level. Without exclusive
      * access <i>SetVideoMode</i> sets the size of the implicitly
      * created window.
      * 
      * <b>Event buffers</b> can be created with an option to
      * automatically attach input devices matching the specified
      * capabilities. If DICAPS_NONE is passed an event buffer with
      * nothing attached to is created. An event buffer can be
      * attached to input devices and windows.
      *
      * <b>Fonts, images and videos</b> are created by this
      * interface. There are different implementations for different
      * content types. On creation a suitable implementation is
      * automatically chosen.
      */
     DEFINE_INTERFACE(   IDirectFB,

        /** Cooperative level, video mode **/

          /*
           * Puts the interface into the specified cooperative level.
           *
           * Function fails with DFB_LOCKED if another instance already
           * is in a cooperative level other than DFSCL_NORMAL.
           */
          DFBResult (*SetCooperativeLevel) (
               IDirectFB                *thiz,
               DFBCooperativeLevel       level
          );

          /*
           * Switch the current video mode (primary layer).
           *
           * If in shared cooperative level this function sets the
           * resolution of the window that is created implicitly for
           * the primary surface.
           */
          DFBResult (*SetVideoMode) (
               IDirectFB                *thiz,
               unsigned int              width,
               unsigned int              height,
               unsigned int              bpp
          );


        /** Hardware capabilities **/

          /*
           * Get a rough description of all drawing/blitting functions
           * along with drawing/blitting flags supported by the hardware.
           *
           * For more detailed information use
           * IDirectFBSurface->GetAccelerationMask().
           */
          DFBResult (*GetCardCapabilities) (
               IDirectFB                *thiz,
               DFBCardCapabilities      *caps
          );

          /*
           * Enumerate supported video modes.
           *
           * Calls the given callback for all available video modes.
           * Useful to select a certain mode to be used with
           * IDirectFB->SetVideoMode().
           */
          DFBResult (*EnumVideoModes) (
               IDirectFB                *thiz,
               DFBVideoModeCallback      callback,
               void                     *callbackdata
          );


        /** Surfaces **/

          /*
           * Create a surface matching the specified description.
           */
          DFBResult (*CreateSurface) (
               IDirectFB                *thiz,
               DFBSurfaceDescription    *desc,
               IDirectFBSurface        **interface
          );


        /** Display Layers **/

          /*
           * Enumerate all existing display layers.
           *
           * Calls the given callback for all available display
           * layers. The callback is passed the layer id that can be
           * used to retrieve an interface on a specific layer using
           * IDirectFB->GetDisplayLayer().
           */
          DFBResult (*EnumDisplayLayers) (
               IDirectFB                *thiz,
               DFBDisplayLayerCallback   callback,
               void                     *callbackdata
          );

          /*
           * Retrieve an interface to a specific display layer.
           */
          DFBResult (*GetDisplayLayer) (
               IDirectFB                *thiz,
               DFBDisplayLayerID         layer_id,
               IDirectFBDisplayLayer   **interface
          );


        /** Input Devices **/

          /*
           * Enumerate all existing input devices.
           *
           * Calls the given callback for all available input devices.
           * The callback is passed the device id that can be used to
           * retrieve an interface on a specific device using
           * IDirectFB->GetInputDevice().
           */
          DFBResult (*EnumInputDevices) (
               IDirectFB                *thiz,
               DFBInputDeviceCallback    callback,
               void                     *callbackdata
          );

          /*
           * Retrieve an interface to a specific input device.
           */
          DFBResult (*GetInputDevice) (
               IDirectFB                *thiz,
               DFBInputDeviceID          device_id,
               IDirectFBInputDevice    **interface
          );

          /*
           * Create a buffer for events.
           *
           * Creates an event buffer and attaches all input devices
           * with matching capabilities. If no input devices match,
           * e.g. by specifying DICAPS_NONE, a buffer will be returned
           * that has no event sources connected to it.
           */
          DFBResult (*CreateEventBuffer) (
               IDirectFB                   *thiz,
               DFBInputDeviceCapabilities   caps,
               IDirectFBEventBuffer       **buffer
          );

        /** Media **/

          /*
           * Create an image provider for the specified file.
           */
          DFBResult (*CreateImageProvider) (
               IDirectFB                *thiz,
               const char               *filename,
               IDirectFBImageProvider  **interface
          );

          /*
           * Create a video provider.
           */
          DFBResult (*CreateVideoProvider) (
               IDirectFB                *thiz,
               const char               *filename,
               IDirectFBVideoProvider  **interface
          );

          /*
           * Create a streamed video provider that uses the callback
           * function to retrieve data. Callback is called one time
           * during creation to determine the data type.
           */
          /*DFBResult (*CreateStreamedVideoProvider) (
               IDirectFB                *thiz,
               DFBGetDataCallback        callback,
               void                     *callback_data,
               IDirectFBVideoProvider  **interface
          );*/

          /*
           * Load a font from the specified file given a description
           * of how to load the glyphs.
           */
          DFBResult (*CreateFont) (
               IDirectFB                *thiz,
               const char               *filename,
               DFBFontDescription       *desc,
               IDirectFBFont           **interface
          );


        /** Misc **/

          /*
           * Suspend DirectFB, no other calls to DirectFB are allowed
           * until Resume has been called.
           */
          DFBResult (*Suspend) (
               IDirectFB                *thiz
          );

          /*
           * Resume DirectFB, only to be called after Suspend.
           */
          DFBResult (*Resume) (
               IDirectFB                *thiz
          );

          /*
           * Wait until graphics card is idle,
           * i.e. finish all drawing/blitting functions.
           */
          DFBResult (*WaitIdle) (
               IDirectFB                *thiz
          );

          /*
           * Wait for next vertical retrace.
           */
          DFBResult (*WaitForSync) (
               IDirectFB                *thiz
          );
     )

     /* predefined layer ids */
     #define DLID_PRIMARY          0x00

     /* predefined input device ids */
     #define DIDID_KEYBOARD        0x0000    /* primary keyboard       */
     #define DIDID_MOUSE           0x0001    /* primary mouse          */
     #define DIDID_JOYSTICK        0x0002    /* primary joystick       */
     #define DIDID_REMOTE          0x0003    /* primary remote control */
     #define DIDID_ANY             0x1000    /* no primary device      */


     /*
      * Cooperative level handling the access permissions.
      */
     typedef enum {
          DLSCL_SHARED             = 0, /* shared access */
          DLSCL_EXCLUSIVE,              /* exclusive access,
                                           fullscreen/mode switching */
          DLSCL_ADMINISTRATIVE          /* administrative access,
                                           enumerate windows, control them */
     } DFBDisplayLayerCooperativeLevel;

     /*
      * Background mode defining how to erase/initialize the area
      * for a windowstack repaint
      */
     typedef enum {
          DLBM_DONTCARE            = 0, /* windowstack repainting does not
                                           clear the layer before */
          DLBM_COLOR,                   /* fill with solid color
                                           (SetBackgroundColor) */
          DLBM_IMAGE,                   /* use an image (SetBackgroundImage) */
          DLBM_TILE                     /* use a tiled image
                                           (SetBackgroundImage) */
     } DFBDisplayLayerBackgroundMode;

     /*
      * Layer configuration flags
      */
     typedef enum {
          DLCONF_WIDTH             = 0x00000001,
          DLCONF_HEIGHT            = 0x00000002,
          DLCONF_PIXELFORMAT       = 0x00000004,
          DLCONF_BUFFERMODE        = 0x00000008,
          DLCONF_OPTIONS           = 0x00000010
     } DFBDisplayLayerConfigFlags;

     /*
      * Layer configuration
      */
     typedef struct {
          DFBDisplayLayerConfigFlags    flags;       /* Which fields of the
                                                        configuration are set */

          unsigned int                  width;       /* Pixel width */
          unsigned int                  height;      /* Pixel height */
          DFBSurfacePixelFormat         pixelformat; /* Pixel format */
          DFBDisplayLayerBufferMode     buffermode;  /* Buffer mode */
          DFBDisplayLayerOptions        options;     /* Enable capabilities */
     } DFBDisplayLayerConfig;


     /*************************
      * IDirectFBDisplayLayer *
      *************************/

     /*
      * <i>No summary yet...</i>
      */
     DEFINE_INTERFACE(   IDirectFBDisplayLayer,

        /** Retrieving information **/

          /*
           * Get the unique layer ID.
           */
          DFBResult (*GetID) (
               IDirectFBDisplayLayer              *thiz,
               DFBDisplayLayerID                  *layer_id
          );

          /*
           * Get the layer's capabilities.
           */
          DFBResult (*GetCapabilities) (
               IDirectFBDisplayLayer              *thiz,
               DFBDisplayLayerCapabilities        *caps
          );


        /** Surface **/

          /*
           * Get an interface to layer's surface.
           *
           * Only available in exclusive mode.
           */
          DFBResult (*GetSurface) (
               IDirectFBDisplayLayer              *thiz,
               IDirectFBSurface                  **interface
          );


        /** Settings **/

          /*
           * Set cooperative level to get control over the layer
           * or the windows within this layer.
           */
          DFBResult (*SetCooperativeLevel) (
               IDirectFBDisplayLayer              *thiz,
               DFBDisplayLayerCooperativeLevel     level
          );

          /*
           * Set global alpha factor for blending with layer(s) below.
           */
          DFBResult (*SetOpacity) (
               IDirectFBDisplayLayer              *thiz,
               __u8                                opacity
          );

          /*
           * Set location on screen as normalized values.
           *
           * So the whole screen is 0, 0 - 1, 1.
           */
          DFBResult (*SetScreenLocation) (
               IDirectFBDisplayLayer              *thiz,
               float                               x,
               float                               y,
               float                               width,
               float                               height
          );

          /*
           * Set the source color key.
           *
           * If a pixel of the layer matches this color the underlying
           * pixel is visible at this point.
           */
          DFBResult (*SetSrcColorKey) (
               IDirectFBDisplayLayer              *thiz,
               __u8                                r,
               __u8                                g,
               __u8                                b
          );

          /*
           * Set the destination color key.
           *
           * The layer is only visible at points where the underlying
           * pixel matches this color.
           */
          DFBResult (*SetDstColorKey) (
               IDirectFBDisplayLayer              *thiz,
               __u8                                r,
               __u8                                g,
               __u8                                b
          );

          /*
           * Get the current display layer level.
           *
           * The level describes the z axis position of a layer. The
           * primary layer is always on level zero unless a special
           * driver adds support for level adjustment on the primary
           * layer.  Layers above have a positive level, e.g. video
           * overlays.  Layers below have a negative level, e.g. video
           * underlays or background layers.
           */
          DFBResult (*GetLevel) (
               IDirectFBDisplayLayer              *thiz,
               int                                *level
          );

          /*
           * Set the display layer level.
           *
           * Moves the layer to the specified level. The order of all
           * other layers won't be changed. Note that only a few
           * layers support level adjustment which is reflected by
           * their capabilities.
           */
          DFBResult (*SetLevel) (
               IDirectFBDisplayLayer              *thiz,
               int                                 level
          );


        /** Configuration handling **/

          /*
           * Get current layer configuration.
           */
          DFBResult (*GetConfiguration) (
               IDirectFBDisplayLayer              *thiz,
               DFBDisplayLayerConfig              *config
          );

          /*
           * Test layer configuration.
           *
           * If configuration fails and 'failed' is not NULL it will
           * indicate which fields of the configuration caused the
           * error.
           */
          DFBResult (*TestConfiguration) (
               IDirectFBDisplayLayer              *thiz,
               DFBDisplayLayerConfig              *config,
               DFBDisplayLayerConfigFlags         *failed
          );

          /*
           * Set layer configuration.
           *
           * Only available in exclusive or administrative mode.
           */
          DFBResult (*SetConfiguration) (
               IDirectFBDisplayLayer              *thiz,
               DFBDisplayLayerConfig              *config
          );


        /** Background handling **/

          /*
           * Set the erase behaviour for windowstack repaints.
           *
           * Only available in exclusive or administrative mode.
           */
          DFBResult (*SetBackgroundMode) (
               IDirectFBDisplayLayer              *thiz,
               DFBDisplayLayerBackgroundMode       mode
          );

          /*
           * Set the background image for the imaged background mode.
           *
           * Only available in exclusive or administrative mode.
           */
          DFBResult (*SetBackgroundImage) (
               IDirectFBDisplayLayer              *thiz,
               IDirectFBSurface                   *surface
          );

          /*
           * Set the color for a solid colored background.
           *
           * Only available in exclusive or administrative mode.
           */
          DFBResult (*SetBackgroundColor) (
               IDirectFBDisplayLayer              *thiz,
               __u8                                r,
               __u8                                g,
               __u8                                b,
               __u8                                a
          );

        /** Color adjustment **/

          /*
           * Get the layers color adjustment.
           */
          DFBResult (*GetColorAdjustment) (
               IDirectFBDisplayLayer              *thiz,
               DFBColorAdjustment                 *adj
          );

          /*
           * Set the layers color adjustment.
           *
           * Only available in exclusive or administrative mode.
           *
           * This function only has an effect if the underlying
           * hardware supports this operation. Check the layers
           * capabilities to find out if this is the case.
           */
          DFBResult (*SetColorAdjustment) (
               IDirectFBDisplayLayer              *thiz,
               DFBColorAdjustment                 *adj
          );

        /** Windows **/

          /*
           * Create a window within this layer given a
           * description of the window that is to be created.
           */
          DFBResult (*CreateWindow) (
               IDirectFBDisplayLayer              *thiz,
               DFBWindowDescription               *desc,
               IDirectFBWindow                   **interface
          );

          /*
           * Retrieve an interface to an existing window.
           *
           * The window is identified by its window id.
           */
          DFBResult (*GetWindow) (
               IDirectFBDisplayLayer              *thiz,
               DFBWindowID                         id,
               IDirectFBWindow                   **interface
          );


        /** Cursor handling **/

          /*
           * Enable/disable the mouse cursor for this layer.
           *
           * Windows on a layer will only receive motion events if
           * the cursor is enabled. This function is only available
           * in exclusive/administrative mode.
           */
          DFBResult (*EnableCursor) (
               IDirectFBDisplayLayer              *thiz,
               int                                 enable
          );

          /*
           * Returns the x/y coordinates of the layer's mouse cursor.
           */
          DFBResult (*GetCursorPosition) (
               IDirectFBDisplayLayer              *thiz,
               int                                *x,
               int                                *y
          );

          /*
           * Move cursor to specified position.
           *
           * Handles movement like a real one, i.e. generates events.
           */
          DFBResult (*WarpCursor) (
               IDirectFBDisplayLayer              *thiz,
               int                                 x,
               int                                 y
          );

          /*
           * Set cursor acceleration.
           *
           * Sets the acceleration of cursor movements. The amount
           * beyond the 'threshold' will be multiplied with the
           * acceleration factor. The acceleration factor is
           * 'numerator/denominator'.
           */
          DFBResult (*SetCursorAcceleration) (
               IDirectFBDisplayLayer              *thiz,
               int                                 numerator,
               int                                 denominator,
               int                                 threshold
          );

          /*
           * Set the cursor shape and the hotspot.
           */
          DFBResult (*SetCursorShape) (
               IDirectFBDisplayLayer              *thiz,
               IDirectFBSurface                   *shape,
               int                                 hot_x,
               int                                 hot_y
          );

          /*
           * Set the cursor opacity.
           *
           * This function is especially useful if you want
           * to hide the cursor but still want windows on this
           * display layer to receive motion events. In this
           * case, simply set the cursor opacity to zero.
           */
          DFBResult (*SetCursorOpacity) (
               IDirectFBDisplayLayer              *thiz,
               __u8                                opacity
          );
     )


     /*
      * Flipping flags controlling the behaviour of Flip().
      */
     typedef enum {
          DSFLIP_WAITFORSYNC  = 0x00000001,  /* flip surface while display
                                                is in vertical retrace */
          DSFLIP_BLIT         = 0x00000002   /* copy backbuffer into
                                                frontbuffer rather than
                                                just swapping them */
     } DFBSurfaceFlipFlags;

     /*
      * Flags controlling the text layout.
      */
     typedef enum {
          DSTF_LEFT           = 0x00000000,  /* left aligned */
          DSTF_CENTER         = 0x00000001,  /* horizontally centered */
          DSTF_RIGHT          = 0x00000002,  /* right aligned */

          DSTF_TOP            = 0x00000004,  /* y specifies the top
                                                instead of the baseline */
          DSTF_BOTTOM         = 0x00000008,  /* y specifies the bottom
                                                instead of the baseline */

          DSTF_TOPLEFT        = DSTF_TOP | DSTF_LEFT,
          DSTF_TOPCENTER      = DSTF_TOP | DSTF_CENTER,
          DSTF_TOPRIGHT       = DSTF_TOP | DSTF_RIGHT,

          DSTF_BOTTOMLEFT     = DSTF_BOTTOM | DSTF_LEFT,
          DSTF_BOTTOMCENTER   = DSTF_BOTTOM | DSTF_CENTER,
          DSTF_BOTTOMRIGHT    = DSTF_BOTTOM | DSTF_RIGHT
     } DFBSurfaceTextFlags;

     /*
      * Flags defining the type of data access.
      * These are important for surface swapping management.
      */
     typedef enum {
          DSLF_READ           = 0x00000001,  /* request read access while
                                                surface is locked */
          DSLF_WRITE          = 0x00000002   /* request write access */
     } DFBSurfaceLockFlags;

     /*
      * Available Porter/Duff rules.
      */
     typedef enum {
                                   /* pixel = (source * fs + destination * fd),
                                      sa = source alpha,
                                      da = destination alpha */
          DSPD_NONE           = 0, /* fs: sa      fd: 1.0-sa (defaults) */
          DSPD_CLEAR          = 1, /* fs: 0.0     fd: 0.0    */
          DSPD_SRC            = 2, /* fs: 1.0     fd: 0.0    */
          DSPD_SRC_OVER       = 3, /* fs: 1.0     fd: 1.0-sa */
          DSPD_DST_OVER       = 4, /* fs: 1.0-da  fd: 1.0    */
          DSPD_SRC_IN         = 5, /* fs: da      fd: 0.0    */
          DSPD_DST_IN         = 6, /* fs: 0.0     fd: sa     */
          DSPD_SRC_OUT        = 7, /* fs: 1.0-da  fd: 0.0    */
          DSPD_DST_OUT        = 8  /* fs: 0.0     fd: 1.0-sa */
     } DFBSurfacePorterDuffRule;

     /*
      * Blend functions to use for source and destination blending
      */
     typedef enum {
          DSBF_ZERO               = 1,  /* */
          DSBF_ONE                = 2,  /* */
          DSBF_SRCCOLOR           = 3,  /* */
          DSBF_INVSRCCOLOR        = 4,  /* */
          DSBF_SRCALPHA           = 5,  /* */
          DSBF_INVSRCALPHA        = 6,  /* */
          DSBF_DESTALPHA          = 7,  /* */
          DSBF_INVDESTALPHA       = 8,  /* */
          DSBF_DESTCOLOR          = 9,  /* */
          DSBF_INVDESTCOLOR       = 10, /* */
          DSBF_SRCALPHASAT        = 11  /* */
     } DFBSurfaceBlendFunction;

     /********************
      * IDirectFBSurface *
      ********************/

     /*
      * <i>No summary yet...</i>
      */
     DEFINE_INTERFACE(   IDirectFBSurface,

        /** Retrieving information **/

          /*
           * Return the capabilities of this surface.
           */
          DFBResult (*GetCapabilities) (
               IDirectFBSurface         *thiz,
               DFBSurfaceCapabilities   *caps
          );

          /*
           * Get the surface's width and height in pixels.
           */
          DFBResult (*GetSize) (
               IDirectFBSurface         *thiz,
               unsigned int             *width,
               unsigned int             *height
          );

          /*
           * Created sub surfaces might be clipped by their parents,
           * this function returns the resulting rectangle relative
           * to this surface.
           *
           * For non sub surfaces this function returns
           * { 0, 0, width, height }.
           */
          DFBResult (*GetVisibleRectangle) (
               IDirectFBSurface         *thiz,
               DFBRectangle             *rect
          );

          /*
           * Get the current pixel format.
           */
          DFBResult (*GetPixelFormat) (
               IDirectFBSurface         *thiz,
               DFBSurfacePixelFormat    *format
          );

          /*
           * Get a mask of drawing functions that are hardware
           * accelerated with the current settings.
           *
           * If a source surface is specified the mask will also
           * contain accelerated blitting functions.  Note that there
           * is no guarantee that these will actually be accelerated
           * since the surface storage (video/system) is examined only
           * when something actually gets drawn or blitted.
           */
          DFBResult (*GetAccelerationMask) (
               IDirectFBSurface         *thiz,
               IDirectFBSurface         *source,
               DFBAccelerationMask      *mask
          );


        /** Buffer operations **/

          /*
           * Lock the surface for the access type specified.
           *
           * Returns a data pointer and the line pitch of it.
           */
          DFBResult (*Lock) (
               IDirectFBSurface         *thiz,
               DFBSurfaceLockFlags       flags,
               void                    **ptr,
               int                      *pitch
          );

          /*
           * Unlock the surface after direct access.
           */
          DFBResult (*Unlock) (
               IDirectFBSurface         *thiz
          );

          /*
           * Flip the two buffers of the surface.
           *
           * If no region is specified the whole surface is flipped,
           * otherwise blitting is used to update the region.
           * This function fails if the surfaces capabilities don't
           * include DSCAPS_FLIPPING.
           */
          DFBResult (*Flip) (
               IDirectFBSurface         *thiz,
               DFBRegion                *region,
               DFBSurfaceFlipFlags       flags
          );

          /*
           * Clear the surface with an extra color.
           *
           * Fills the whole (sub) surface with the specified color
           * ignoring drawing flags and color of the current state,
           * but limited to the current clip.
           *
           * As with all drawing and blitting functions the backbuffer
           * is written to. If you are initializing a double buffered
           * surface you may want to clear both buffers by doing a
           * Clear-Flip-Clear sequence.
           */
          DFBResult (*Clear) (
               IDirectFBSurface         *thiz,
               __u8                      r,
               __u8                      g,
               __u8                      b,
               __u8                      a
          );


        /** Drawing/blitting control **/

          /*
           * Set the clipping region used to limitate the area for
           * drawing, blitting and text functions.
           *
           * If no region is specified (NULL passed) the clip is set
           * to the surface extents (initial clip).
           */
          DFBResult (*SetClip) (
               IDirectFBSurface         *thiz,
               DFBRegion                *clip
          );

          /*
           * Set the color used for drawing/text functions or
           * alpha/color modulation (blitting functions).
           *
           * If you are not using the alpha value it should be set to
           * 0xff to ensure visibility when the code is ported to or
           * used for surfaces with an alpha channel.
           */
          DFBResult (*SetColor) (
               IDirectFBSurface         *thiz,
               __u8                      r,
               __u8                      g,
               __u8                      b,
               __u8                      a
          );

          /*
           * Set the blend function that applies to the source.
           */
          DFBResult (*SetSrcBlendFunction) (
               IDirectFBSurface         *thiz,
               DFBSurfaceBlendFunction   function
          );

          /*
           * Set the blend function that applies to the destination.
           */
          DFBResult (*SetDstBlendFunction) (
               IDirectFBSurface         *thiz,
               DFBSurfaceBlendFunction   function
          );

          /*
           * Set the source and destination blend function by
           * specifying a Porter/Duff rule.
           */
          DFBResult (*SetPorterDuff) (
               IDirectFBSurface         *thiz,
               DFBSurfacePorterDuffRule  rule
          );

          /*
           * Set the source color key, i.e. the color that is excluded
           * when blitting FROM this surface TO another that has
           * source color keying enabled.
           */
          DFBResult (*SetSrcColorKey) (
               IDirectFBSurface         *thiz,
               __u8                      r,
               __u8                      g,
               __u8                      b
          );

          /*
           * Set the destination color key, i.e. the only color that
           * gets overwritten by drawing and blitting to this surface
           * when destination color keying is enabled.
           */
          DFBResult (*SetDstColorKey) (
               IDirectFBSurface         *thiz,
               __u8                      r,
               __u8                      g,
               __u8                      b
          );


        /** Blitting functions **/

          /*
           * Set the flags for all subsequent blitting commands.
           */
          DFBResult (*SetBlittingFlags) (
               IDirectFBSurface         *thiz,
               DFBSurfaceBlittingFlags   flags
          );

          /*
           * Blit an area from the source to this surface.
           *
           * Pass a NULL rectangle to use the whole source surface.
           * Source may be the same surface.
           */
          DFBResult (*Blit) (
               IDirectFBSurface         *thiz,
               IDirectFBSurface         *source,
               DFBRectangle             *source_rect,
               int                       x,
               int                       y
          );

          /*
           * Blit an area from the source tiled to this surface.
           *
           * Pass a NULL rectangle to use the whole source surface.
           * Source may be the same surface.
           */
          DFBResult (*TileBlit) (
               IDirectFBSurface         *thiz,
               IDirectFBSurface         *source,
               DFBRectangle             *source_rect,
               int                       x,
               int                       y
          );

          /*
           * Blit an area scaled from the source to the destination
           * rectangle.
           *
           * Pass a NULL rectangle to use the whole source surface.
           */
          DFBResult (*StretchBlit) (
               IDirectFBSurface         *thiz,
               IDirectFBSurface         *source,
               DFBRectangle             *source_rect,
               DFBRectangle             *destination_rect
          );


        /** Drawing functions **/

          /*
           * Set the flags for all subsequent drawing commands.
           */
          DFBResult (*SetDrawingFlags) (
               IDirectFBSurface         *thiz,
               DFBSurfaceDrawingFlags    flags
          );

          /*
           * Fill the specified rectangle with the given color
           * following the specified flags.
           */
          DFBResult (*FillRectangle) (
               IDirectFBSurface         *thiz,
               int                       x,
               int                       y,
               int                       w,
               int                       h
          );

          /*
           * Draw an outline of the specified rectangle with the given
           * color following the specified flags.
           */
          DFBResult (*DrawRectangle) (
               IDirectFBSurface         *thiz,
               int                       x,
               int                       y,
               int                       w,
               int                       h
          );

          /*
           * Draw a line from one point to the other with the given color
           * following the drawing flags.
           */
          DFBResult (*DrawLine) (
               IDirectFBSurface         *thiz,
               int                       x1,
               int                       y1,
               int                       x2,
               int                       y2
          );

          /*
           * Draw 'num_lines' lines with the given color following the
           * drawing flags. Each line specified by a DFBRegion.
           */
          DFBResult (*DrawLines) (
               IDirectFBSurface         *thiz,
               DFBRegion                *lines,
               unsigned int              num_lines
          );

          /*
           * Fill a non-textured triangle.
           */
          DFBResult (*FillTriangle) (
               IDirectFBSurface         *thiz,
               int                       x1,
               int                       y1,
               int                       x2,
               int                       y2,
               int                       x3,
               int                       y3
          );


        /** Text functions **/

          /*
           * Set the font used by DrawString() and DrawGlyph().
           */
          DFBResult (*SetFont) (
               IDirectFBSurface         *thiz,
               IDirectFBFont            *font
          );

          /*
           * Get the font associated with a surface.
           *
           * This function increases the font's reference count.
           */
          DFBResult (*GetFont) (
               IDirectFBSurface         *thiz,
               IDirectFBFont           **font
          );

          /*
           * Draw an UTF-8 string at the specified position with the
           * given color following the specified flags.
           *
           * Bytes specifies the number of bytes to take from the
           * string or -1 for the complete NULL-terminated string. You
           * need to set a font using the SetFont() method before
           * calling this function.
           */
          DFBResult (*DrawString) (
               IDirectFBSurface         *thiz,
               const char               *text,
               int                       bytes,
               int                       x,
               int                       y,
               DFBSurfaceTextFlags       flags
          );

          /*
           * Draw a single glyph specified by its Unicode index at the
           * specified position with the given color following the
           * specified flags.
           *
           * You need to set a font using the SetFont() method before
           * calling this function.
           */
          DFBResult (*DrawGlyph) (
               IDirectFBSurface         *thiz,
               unsigned int              index,
               int                       x,
               int                       y,
               DFBSurfaceTextFlags       flags
          );

        /** Lightweight helpers **/

          /*
           * Get an interface to a sub area of this surface.
           *
           * No image data is duplicated, this is a clipped graphics
           * within the original surface. This is very helpful for
           * lightweight components in a GUI toolkit.  The new
           * surface's state (color, drawingflags, etc.) is
           * independent from this one. So it's a handy graphics
           * context.  If no rectangle is specified, the whole surface
           * (or a part if this surface is a subsurface itself) is
           * represented by the new one.
           */
          DFBResult (*GetSubSurface) (
               IDirectFBSurface         *thiz,
               DFBRectangle             *rect,
               IDirectFBSurface        **interface
          );


        /** OpenGL **/

          /*
           * Get an OpenGL context for this surface.
           */
          DFBResult (*GetGL) (
               IDirectFBSurface         *thiz,
               IDirectFBGL             **interface
          );
     )

     
     /*
      * Specifies whether a key is currently down.
      */
     typedef enum {
          DIKS_UP             = 0x00000000,  /* key is not pressed */
          DIKS_DOWN           = 0x00000001   /* key is pressed */
     } DFBInputDeviceKeyState;

     /*
      * Specifies whether a button is currently pressed.
      */
     typedef enum {
          DIBS_UP             = 0x00000000,  /* button is not pressed */
          DIBS_DOWN           = 0x00000001   /* button is pressed */
     } DFBInputDeviceButtonState;

     /*
      * Flags specifying which buttons are currently down.
      */
     typedef enum {
          DIBM_LEFT           = 0x00000001,  /* left mouse button */
          DIBM_RIGHT          = 0x00000002,  /* right mouse button */
          DIBM_MIDDLE         = 0x00000004   /* middle mouse button */
     } DFBInputDeviceButtonMask;

     /*
      * Flags specifying which modifiers are currently pressed.
      */
     typedef enum {
          DIMM_SHIFT     = (1 << DIMKI_SHIFT),    /* Shift key is pressed */
          DIMM_CONTROL   = (1 << DIMKI_CONTROL),  /* Control key is pressed */
          DIMM_ALT       = (1 << DIMKI_ALT),      /* Alt key is pressed */
          DIMM_ALTGR     = (1 << DIMKI_ALTGR),    /* AltGr key is pressed */
          DIMM_META      = (1 << DIMKI_META),     /* Meta key is pressed */
          DIMM_SUPER     = (1 << DIMKI_SUPER),    /* Super key is pressed */
          DIMM_HYPER     = (1 << DIMKI_HYPER)     /* Hyper key is pressed */
     } DFBInputDeviceModifierMask;
     

     /************************
      * IDirectFBInputDevice *
      ************************/

     /*
      * <i>No summary yet...</i>
      */
     DEFINE_INTERFACE(   IDirectFBInputDevice,

        /** Retrieving information **/

          /*
           * Get the unique device ID.
           */
          DFBResult (*GetID) (
               IDirectFBInputDevice          *thiz,
               DFBInputDeviceID              *device_id
          );

          /*
           * Get a description of this device, i.e. the capabilities.
           */
          DFBResult (*GetDescription) (
               IDirectFBInputDevice          *thiz,
               DFBInputDeviceDescription     *desc
          );


        /** Key mapping **/

          /*
           * Fetch one entry from the keymap for a specific hardware keycode.
           */
          DFBResult (*GetKeymapEntry) (
               IDirectFBInputDevice          *thiz,
               int                            keycode,
               DFBInputDeviceKeymapEntry     *entry
          );


        /** Event buffers **/

          /*
           * Create an event buffer for this device and attach it.
           */
          DFBResult (*CreateEventBuffer) (
               IDirectFBInputDevice          *thiz,
               IDirectFBEventBuffer         **buffer
          );

          /*
           * Attach an existing event buffer to this device.
           *
           * NOTE: Attaching multiple times generates multiple events.
           *
           */
          DFBResult (*AttachEventBuffer) (
               IDirectFBInputDevice          *thiz,
               IDirectFBEventBuffer          *buffer
          );


        /** General state queries **/

          /*
           * Get the current state of one key.
           */
          DFBResult (*GetKeyState) (
               IDirectFBInputDevice          *thiz,
               DFBInputDeviceKeyIdentifier    key_id,
               DFBInputDeviceKeyState        *state
          );

          /*
           * Get the current modifier mask.
           */
          DFBResult (*GetModifiers) (
               IDirectFBInputDevice          *thiz,
               DFBInputDeviceModifierMask    *modifiers
          );

          /*
           * Get the current state of the key locks.
           */
          DFBResult (*GetLockState) (
               IDirectFBInputDevice          *thiz,
               DFBInputDeviceLockState       *locks
          );

          /*
           * Get a mask of currently pressed buttons.
           *
           * The first button corrensponds to the right most bit.
           */
          DFBResult (*GetButtons) (
               IDirectFBInputDevice          *thiz,
               DFBInputDeviceButtonMask      *buttons
          );

          /*
           * Get the state of a button.
           */
          DFBResult (*GetButtonState) (
               IDirectFBInputDevice          *thiz,
               DFBInputDeviceButtonIdentifier button,
               DFBInputDeviceButtonState     *state
          );

          /*
           * Get the current value of the specified axis.
           */
          DFBResult (*GetAxis) (
               IDirectFBInputDevice          *thiz,
               DFBInputDeviceAxisIdentifier   axis,
               int                           *pos
          );


        /** Specialized queries **/

          /*
           * Utility function combining two calls to GetAxis().
           *
           * You may leave one of the x/y arguments NULL.
           */
          DFBResult (*GetXY) (
               IDirectFBInputDevice          *thiz,
               int                           *x,
               int                           *y
          );
     )


     /*
      * Event class.
      */
     typedef enum {
          DFEC_NONE           = 0x00,   /* none of these */
          DFEC_INPUT          = 0x01,   /* raw input event */
          DFEC_WINDOW         = 0x02,   /* windowing event */
          DFEC_USER           = 0x03    /* custom events for
                                           the user of this library */
     } DFBEventClass;

     /*
      * The type of an input event.
      */
     typedef enum {
          DIET_UNKNOWN        = 0,      /* unknown event */
          DIET_KEYPRESS,                /* a key is been pressed */
          DIET_KEYRELEASE,              /* a key is been released */
          DIET_BUTTONPRESS,             /* a (mouse) button is been pressed */
          DIET_BUTTONRELEASE,           /* a (mouse) button is been released */
          DIET_AXISMOTION               /* mouse/joystick movement */
     } DFBInputEventType;

     /*
      * Flags defining which additional (optional) event fields are valid.
      */
     typedef enum {
          DIEF_NONE           = 0x000,   /* no additional fields */
          DIEF_TIMESTAMP      = 0x001,   /* timestamp is valid */
          DIEF_AXISABS        = 0x002,   /* axis and axisabs are valid */
          DIEF_AXISREL        = 0x004,   /* axis and axisrel are valid */

          DIEF_KEYCODE        = 0x008,   /* used internally by the input core,
                                            always set at application level */
          DIEF_KEYID          = 0x010,   /* used internally by the input core,
                                            always set at application level */
          DIEF_KEYSYMBOL      = 0x020,   /* used internally by the input core,
                                            always set at application level */
          DIEF_MODIFIERS      = 0x040,   /* used internally by the input core,
                                            always set at application level */
          DIEF_LOCKS          = 0x080,   /* used internally by the input core,
                                            always set at application level */
          DIEF_BUTTONS        = 0x100    /* used internally by the input core,
                                            always set at application level */
     } DFBInputEventFlags;

     /*
      * An input event, item of an input buffer.
      */
     typedef struct {
          DFBEventClass                   clazz;      /* clazz of event */

          DFBInputEventType               type;       /* type of event */
          DFBInputDeviceID                device_id;  /* source of event */
          DFBInputEventFlags              flags;      /* which optional fields
                                                         are valid? */

          /* additionally (check flags) */
          struct timeval                  timestamp;  /* time of event
                                                         creation (optional) */

     /* DIET_KEYPRESS, DIET_KEYRELEASE */
          int                             key_code;   /* hardware keycode, no
                                                         mapping, -1 if device
                                                         doesn't differentiate
                                                         between several keys */
          DFBInputDeviceKeyIdentifier     key_id;     /* basic mapping,
                                                         modifier independent */
          DFBInputDeviceKeySymbol         key_symbol; /* advanced mapping,
                                                         unicode compatible,
                                                         modifier dependent */
          /* additionally (check flags) */
          DFBInputDeviceModifierMask      modifiers;  /* pressed modifiers
                                                         (optional) */
          DFBInputDeviceLockState         locks;      /* active locks
                                                         (optional) */

     /* DIET_BUTTONPRESS, DIET_BUTTONRELEASE */
          DFBInputDeviceButtonIdentifier  button;     /* in case of a button
                                                         event */
          DFBInputDeviceButtonMask        buttons;    /* mask of currently
                                                         pressed buttons */

     /* DIET_AXISMOTION */
          DFBInputDeviceAxisIdentifier    axis;       /* in case of an axis
                                                         event */
          /* one of these two (check flags) */
          int                             axisabs;    /* absolute mouse/
                                                         joystick coordinate */
          int                             axisrel;    /* relative mouse/
                                                         joystick movement */
     } DFBInputEvent;

     /*
      * Window Event Types - can also be used as flags for event filters.
      */
     typedef enum {
          DWET_POSITION       = 0x00000001,  /* window has been moved by
                                                window manager or the
                                                application itself */
          DWET_SIZE           = 0x00000002,  /* window has been resized
                                                by window manager or the
                                                application itself */
          DWET_CLOSE          = 0x00000004,  /* closing this window has been
                                                requested only */
          DWET_DESTROYED      = 0x00000008,  /* window got destroyed by global
                                                deinitialization function or
                                                the application itself */
          DWET_GOTFOCUS       = 0x00000010,  /* window got focus */
          DWET_LOSTFOCUS      = 0x00000020,  /* window lost focus */

          DWET_KEYDOWN        = 0x00000100,  /* a key has gone down while
                                                window has focus */
          DWET_KEYUP          = 0x00000200,  /* a key has gone up while
                                                window has focus */

          DWET_BUTTONDOWN     = 0x00010000,  /* mouse button went down in
                                                the window */
          DWET_BUTTONUP       = 0x00020000,  /* mouse button went up in
                                                the window */
          DWET_MOTION         = 0x00040000,  /* mouse cursor changed its
                                                position in window */
          DWET_ENTER          = 0x00080000,  /* mouse cursor entered
                                                the window */
          DWET_LEAVE          = 0x00100000,  /* mouse cursor left the window */

          DWET_WHEEL          = 0x00200000,  /* mouse wheel was moved while
                                                window has focus */

          DWET_POSITION_SIZE  = DWET_POSITION | DWET_SIZE,/* initially sent to
                                                             window when it's
                                                             created */

          DWET_ALL            = 0x003F033F   /* all event types */
     } DFBWindowEventType;

     /*
      * Event from the windowing system.
      */
     typedef struct {
          DFBEventClass                   clazz;      /* clazz of event */

          DFBWindowEventType              type;       /* type of event */
          DFBWindowID                     window_id;  /* source of event */

          /* used by DWET_MOVE, DWET_MOTION, DWET_BUTTONDOWN, DWET_BUTTONUP,
             DWET_ENTER, DWET_LEAVE */
          int                             x;          /* x position of window
                                                         or coordinate within
                                                         window */
          int                             y;          /* y position of window
                                                         or coordinate within
                                                         window */

          /* used by DWET_MOTION, DWET_BUTTONDOWN, DWET_BUTTONUP,
             DWET_ENTER, DWET_LEAVE */
          int                             cx;         /* x cursor position */
          int                             cy;         /* y cursor position */

          /* used by DWET_WHEEL */
          int                             step;       /* wheel step */

          /* used by DWET_RESIZE */
          unsigned int                    w;          /* width of window */
          unsigned int                    h;          /* height of window */

          /* used by DWET_KEYDOWN, DWET_KEYUP */
          int                             key_code;   /* hardware keycode, no
                                                         mapping, -1 if device
                                                         doesn't differentiate
                                                         between several keys */
          DFBInputDeviceKeyIdentifier     key_id;     /* basic mapping,
                                                         modifier independent */
          DFBInputDeviceKeySymbol         key_symbol; /* advanced mapping,
                                                         unicode compatible,
                                                         modifier dependent */
          DFBInputDeviceModifierMask      modifiers;  /* pressed modifiers */
          DFBInputDeviceLockState         locks;      /* active locks */

          /* used by DWET_BUTTONDOWN, DWET_BUTTONUP */
          DFBInputDeviceButtonIdentifier  button;     /* button being
                                                         pressed or released */
          DFBInputDeviceButtonMask        buttons;    /* mask of currently
                                                         pressed buttons */
     } DFBWindowEvent;

     /*
      * Event for usage by the user of this library.
      */
     typedef struct {
          DFBEventClass                   clazz;      /* clazz of event */

          unsigned int                    type;       /* custom type */
          void                           *data;       /* custom data */
     } DFBUserEvent;

     /*
      * General container for a DirectFB Event.
      */
     typedef union {
          DFBEventClass            clazz;   /* clazz of event */
          DFBInputEvent            input;   /* field for input events */
          DFBWindowEvent           window;  /* field for window events */
          DFBUserEvent             user;    /* field for user-defined events */
     } DFBEvent;

     #define DFB_EVENT(e)          ((DFBEvent *) (e))

     /************************
      * IDirectFBEventBuffer *
      ************************/

     /*
      * <i>No summary yet...</i>
      */
     DEFINE_INTERFACE(   IDirectFBEventBuffer,


        /** Buffer handling **/

          /*
           * Clear all events stored in this buffer.
           */
          DFBResult (*Reset) (
               IDirectFBEventBuffer     *thiz
          );


        /** Waiting for events **/

          /*
           * Wait for the next event to occur.
           * Thread is idle in the meantime.
           */
          DFBResult (*WaitForEvent) (
               IDirectFBEventBuffer     *thiz
          );

          /*
           * Block until next event to occur or timeout is reached.
           * Thread is idle in the meantime.
           */
          DFBResult (*WaitForEventWithTimeout) (
               IDirectFBEventBuffer     *thiz,
               unsigned int              seconds,
               unsigned int              milli_seconds
          );


        /** Fetching events **/

          /*
           * Get the next event and remove it from the FIFO.
           */
          DFBResult (*GetEvent) (
               IDirectFBEventBuffer     *thiz,
               DFBEvent                 *event
          );

          /*
           * Get the next event but leave it there, i.e. do a preview.
           */
          DFBResult (*PeekEvent) (
               IDirectFBEventBuffer     *thiz,
               DFBEvent                 *event
          );

          /*
           * Check if there is a pending event in the queue. This
           * function returns DFB_OK if there is at least one event,
           * DFB_BUFFER_EMPTY otherwise.
           */
          DFBResult (*HasEvent) (
               IDirectFBEventBuffer     *thiz
          );


        /** Sending events **/

          /*
           * Put an event into the FIFO.
           *
           * This function does not wait until the event got fetched.
           */
          DFBResult (*PostEvent) (
               IDirectFBEventBuffer     *thiz,
               DFBEvent                 *event
          );
     )

     /*
      * Flags controlling the appearance and behaviour of the window.
      */
     typedef enum {
          DWOP_NONE           = 0x00000000,  /* none of these */
          DWOP_COLORKEYING    = 0x00000001,  /* enable color key */
          DWOP_KEEP_POSITION  = 0x00000010,  /* window can't be moved
                                                with the mouse */
          DWOP_KEEP_SIZE      = 0x00000020,  /* window can't be resized
                                                with the mouse */
          DWOP_KEEP_STACKING  = 0x00000040,  /* window can't be raised
                                                or lowered with the mouse */
          DWOP_GHOST          = 0x00001000,  /* never get focus or input,
                                                clicks will go through,
                                                implies DWOP_KEEP... */
          DWOP_INDESTRUCTIBLE = 0x00002000,  /* window can't be destroyed
                                                by internal shortcut */
          DWOP_ALL            = 0x00003071   /* all possible options */
     } DFBWindowOptions;

     /*
      * The stacking class restricts the stacking order of windows.
      */
     typedef enum {
          DWSC_MIDDLE         = 0x00000000,  /* This is the default stacking
                                                class of new windows. */
          DWSC_UPPER          = 0x00000001,  /* Window is always above windows
                                                in the middle stacking class.
                                                Only windows that are also in
                                                the upper stacking class can
                                                get above them. */
          DWSC_LOWER          = 0x00000002   /* Window is always below windows
                                                in the middle stacking class.
                                                Only windows that are also in
                                                the lower stacking class can
                                                get below them. */
     } DFBWindowStackingClass;

     /*******************
      * IDirectFBWindow *
      *******************/

     /*
      * <i>No summary yet...</i>
      */
     DEFINE_INTERFACE(   IDirectFBWindow,

        /** Retrieving information **/

          /*
           * Get the unique window ID.
           */
          DFBResult (*GetID) (
               IDirectFBWindow          *thiz,
               DFBWindowID              *window_id
          );

          /*
           * Get the current position of this window.
           */
          DFBResult (*GetPosition) (
               IDirectFBWindow          *thiz,
               int                      *x,
               int                      *y
          );

          /*
           * Get the size of the window in pixels.
           */
          DFBResult (*GetSize) (
               IDirectFBWindow          *thiz,
               unsigned int             *width,
               unsigned int             *height
          );


        /** Event handling **/

          /*
           * Create an event buffer for this window and attach it.
           */
          DFBResult (*CreateEventBuffer) (
               IDirectFBWindow          *thiz,
               IDirectFBEventBuffer    **buffer
          );

          /*
           * Attach an existing event buffer to this window.
           *
           * NOTE: Attaching multiple times generates multiple events.
           *
           */
          DFBResult (*AttachEventBuffer) (
               IDirectFBWindow          *thiz,
               IDirectFBEventBuffer     *buffer
          );

          /*
           * Enable specific events to be sent to the window.
           *
           * The argument is a mask of events that will be set in the
           * window's event mask. The default event mask is DWET_ALL.
           */
          DFBResult (*EnableEvents) (
               IDirectFBWindow          *thiz,
               DFBWindowEventType        mask
          );

          /*
           * Disable specific events from being sent to the window.
           *
           * The argument is a mask of events that will be cleared in
           * the window's event mask. The default event mask is DWET_ALL.
           */
          DFBResult (*DisableEvents) (
               IDirectFBWindow          *thiz,
               DFBWindowEventType        mask
          );


        /** Surface handling **/

          /*
           * Get an interface to the backing store surface.
           *
           * This surface has to be flipped to make previous drawing
           * commands visible, i.e. to repaint the windowstack for
           * that region.
           */
          DFBResult (*GetSurface) (
               IDirectFBWindow          *thiz,
               IDirectFBSurface        **surface
          );


        /** Appearance and Behaviour **/

          /*
           * Set options controlling appearance and behaviour of the window.
           */
          DFBResult (*SetOptions) (
               IDirectFBWindow          *thiz,
               DFBWindowOptions          options
          );
          
          /*
           * Get options controlling appearance and behaviour of the window.
           */
          DFBResult (*GetOptions) (
               IDirectFBWindow          *thiz,
               DFBWindowOptions         *options
          );
          
          /*
           * Set the window color key.
           *
           * If a pixel of the window matches this color the
           * underlying window or the background is visible at this
           * point.
           */
          DFBResult (*SetColorKey) (
               IDirectFBWindow          *thiz,
               __u8                      r,
               __u8                      g,
               __u8                      b
          );
          
          /*
           * Set the window's global opacity factor.
           *
           * Set it to "0" to hide a window.
           * Setting it to "0xFF" makes the window opaque if
           * it has no alpha channel.
           */
          DFBResult (*SetOpacity) (
               IDirectFBWindow          *thiz,
               __u8                      opacity
          );

          /*
           * Get the current opacity factor of this window.
           */
          DFBResult (*GetOpacity) (
               IDirectFBWindow          *thiz,
               __u8                     *opacity
          );


        /** Focus handling **/

          /*
           * Pass the focus to this window.
           */
          DFBResult (*RequestFocus) (
               IDirectFBWindow          *thiz
          );

          /*
           * Grab the keyboard, i.e. all following keyboard events are
           * sent to this window ignoring the focus.
           */
          DFBResult (*GrabKeyboard) (
               IDirectFBWindow          *thiz
          );

          /*
           * Ungrab the keyboard, i.e. switch to standard key event
           * dispatching.
           */
          DFBResult (*UngrabKeyboard) (
               IDirectFBWindow          *thiz
          );

          /*
           * Grab the pointer, i.e. all following mouse events are
           * sent to this window ignoring the focus.
           */
          DFBResult (*GrabPointer) (
               IDirectFBWindow          *thiz
          );

          /*
           * Ungrab the pointer, i.e. switch to standard mouse event
           * dispatching.
           */
          DFBResult (*UngrabPointer) (
               IDirectFBWindow          *thiz
          );

        
        /** Position and Size **/

          /*
           * Move the window by the specified distance.
           */
          DFBResult (*Move) (
               IDirectFBWindow          *thiz,
               int                       dx,
               int                       dy
          );

          /*
           * Move the window to the specified coordinates.
           */
          DFBResult (*MoveTo) (
               IDirectFBWindow          *thiz,
               int                       x,
               int                       y
          );

          /*
           * Resize the window.
           */
          DFBResult (*Resize) (
               IDirectFBWindow          *thiz,
               unsigned int              width,
               unsigned int              height
          );


        /** Stacking **/

          /*
           * Put the window into a specific stacking class.
           */
          DFBResult (*SetStackingClass) (
               IDirectFBWindow          *thiz,
               DFBWindowStackingClass    stacking_class
          );

          /*
           * Raise the window by one within the window stack.
           */
          DFBResult (*Raise) (
               IDirectFBWindow          *thiz
          );

          /*
           * Lower the window by one within the window stack.
           */
          DFBResult (*Lower) (
               IDirectFBWindow          *thiz
          );

          /*
           * Put the window on the top of the window stack.
           */
          DFBResult (*RaiseToTop) (
               IDirectFBWindow          *thiz
          );

          /*
           * Send a window to the bottom of the window stack.
           */
          DFBResult (*LowerToBottom) (
               IDirectFBWindow          *thiz
          );

          /*
           * Put a window on top of another window.
           */
          DFBResult (*PutAtop) (
               IDirectFBWindow          *thiz,
               IDirectFBWindow          *lower
          );

          /*
           * Put a window below another window.
           */
          DFBResult (*PutBelow) (
               IDirectFBWindow          *thiz,
               IDirectFBWindow          *upper
          );


        /** Closing **/

          /*
           * Send a close message to the window.
           *
           * This function sends a message of type DWET_CLOSE to the window.
           * It does NOT actually close it.
           */
          DFBResult (*Close) (
               IDirectFBWindow          *thiz
          );

          /*
           * Destroys the window and sends a destruction message.
           *
           * This function sends a message of type DWET_DESTROY to
           * the window after removing it from the window stack and
           * freeing its data.  Some functions called from this
           * interface will return DFB_DESTROYED after that.
           */
          DFBResult (*Destroy) (
               IDirectFBWindow          *thiz
          );
     )


     /*****************
      * IDirectFBFont *
      *****************/

     /*
      * <i>No summary yet...</i>
      */
     DEFINE_INTERFACE(   IDirectFBFont,

        /** Retrieving information **/

          /*
           * Get the distance from the baseline to the top of the
           * logical extents of this font.
           */
          DFBResult (*GetAscender) (
               IDirectFBFont       *thiz,
               int                 *ascender
          );

          /*
           * Get the distance from the baseline to the bottom of
           * the logical extents of this font.
           *
           * This is a negative value!
           */
          DFBResult (*GetDescender) (
               IDirectFBFont       *thiz,
               int                 *descender
          );

          /*
           * Get the logical height of this font. This is the vertical
           * distance from one baseline to the next when writing
           * several lines of text. Note that this value does not
           * correspond the height value specified when loading the
           * font.
           */
          DFBResult (*GetHeight) (
               IDirectFBFont       *thiz,
               int                 *height
          );

          /*
           * Get the maximum character width.
           *
           * This is a somewhat dubious value. Not all fonts
           * specify it correcly. It can give you an idea of
           * the maximum expected width of a rendered string.
           */
          DFBResult (*GetMaxAdvance) (
               IDirectFBFont       *thiz,
               int                 *maxadvance
          );

          /*
           * Get the kerning to apply between two glyphs specified by
           * their Unicode indices.
           */
          DFBResult (*GetKerning) (
               IDirectFBFont       *thiz,
               unsigned int         prev_index,
               unsigned int         current_index,
               int                 *kern_x,
               int                 *kern_y
          );

        /** String extents measurement **/

          /*
           * Get the logical width of the specified UTF-8 string
           * as if it were drawn with this font.
           *
           * Bytes specifies the number of bytes to take from the
           * string or -1 for the complete NULL-terminated string.
           *
           * The returned width may be different than the actual drawn
           * width of the text since this function returns the logical
           * width that should be used to layout the text. A negative
           * width indicates right-to-left rendering.
           */
          DFBResult (*GetStringWidth) (
               IDirectFBFont       *thiz,
               const char          *text,
               int                  bytes,
               int                 *width
          );

          /*
           * Get the logical and real extents of the specified
           * UTF-8 string as if it were drawn with this font.
           *
           * Bytes specifies the number of bytes to take from the
           * string or -1 for the complete NULL-terminated string.
           *
           * The logical rectangle describes the typographic extents
           * and should be used to layout text. The ink rectangle
           * describes the smallest rectangle containing all pixels
           * that are touched when drawing the string. If you only
           * need one of the rectangles, pass NULL for the other one.
           *
           * The ink rectangle is guaranteed to be a valid rectangle
           * with positive width and height, while the logical
           * rectangle may have negative width indicating right-to-left
           * layout.
           *
           * The rectangle offsets are reported relative to the
           * baseline and refer to the text being drawn using
           * DSTF_LEFT.
           */
          DFBResult (*GetStringExtents) (
               IDirectFBFont       *thiz,
               const char          *text,
               int                  bytes,
               DFBRectangle        *logical_rect,
               DFBRectangle        *ink_rect
          );
 
          /*
           * Get the extents of a glyph specified by its Unicode
           * index.
           *
           * The rectangle describes the the smallest rectangle
           * containing all pixels that are touched when drawing the
           * glyph. It is reported relative to the baseline. If you
           * only need the advance, pass NULL for the rectangle.
           *
           * The advance describes the horizontal offset to the next
           * glyph (without kerning applied). It may be a negative
           * value indicating left-to-right rendering. If you don't
           * need this value, pass NULL for advance.
           */
         DFBResult (*GetGlyphExtents) (
               IDirectFBFont       *thiz,
               unsigned int         index,
               DFBRectangle        *rect,
               int                 *advance
          );
      )

     /*
      * Capabilities of an image.
      */
     typedef enum {
          DICAPS_NONE            = 0x00000000,  /* None of these.            */
          DICAPS_ALPHACHANNEL    = 0x00000001,  /* The image data contains an
                                                   alphachannel.             */
          DICAPS_COLORKEY        = 0x00000002   /* The image has a colorkey,
                                                   e.g. the transparent color
                                                   of a GIF image.           */
     } DFBImageCapabilities;

     /*
      * Information about an image including capabilities and values
      * belonging to available capabilities.
      */
     typedef struct {
          DFBImageCapabilities     caps;        /* capabilities              */

          __u8                     colorkey_r;  /* colorkey red channel      */
          __u8                     colorkey_g;  /* colorkey green channel    */
          __u8                     colorkey_b;  /* colorkey blue channel     */
     } DFBImageDescription;

     /**************************
      * IDirectFBImageProvider *
      **************************/

     /*
      * <i>No summary yet...</i>
      */
     DEFINE_INTERFACE(   IDirectFBImageProvider,

        /** Retrieving information **/

          /*
           * Get a surface description that best matches the image
           * contained in the file.
           *
           * For opaque image formats the pixel format of the primary
           * layer is used. For images with alpha channel an ARGB
           * surface description is returned.
           */
          DFBResult (*GetSurfaceDescription) (
               IDirectFBImageProvider   *thiz,
               DFBSurfaceDescription    *dsc
          );

          /*
           * Get a description of the image.
           *
           * This includes stuff that does not belong into the surface
           * description, e.g. a colorkey of a transparent GIF.
           */
          DFBResult (*GetImageDescription) (
               IDirectFBImageProvider   *thiz,
               DFBImageDescription      *dsc
          );


        /** Rendering **/

          /*
           * Render the file contents into the destination contents
           * doing automatic scaling and color format conversion.
           *
           * If the image file has an alpha channel it is rendered
           * with alpha channel if the destination surface is of the
           * ARGB pixelformat. Otherwise, transparent areas are
           * blended over a black background.
           *
           * If a destination rectangle is specified, the rectangle is
           * clipped to the destination surface. If NULL is passed as
           * destination rectangle, the whole destination surface is
           * taken. The image is stretched to fill the rectangle.
           */
          DFBResult (*RenderTo) (
               IDirectFBImageProvider   *thiz,
               IDirectFBSurface         *destination,
               DFBRectangle             *destination_rect
          );
     )

     /*
      * Called for each written frame.
      */
     typedef int (*DVFrameCallback)(void *ctx);


     /**************************
      * IDirectFBVideoProvider *
      **************************/

     /*
      * <i>No summary yet...</i>
      */
     DEFINE_INTERFACE(   IDirectFBVideoProvider,

        /** Retrieving information **/

          /*
           * Retrieve information about the video provider's
           * capabilities.
           */
          DFBResult (*GetCapabilities) (
               IDirectFBVideoProvider        *thiz,
               DFBVideoProviderCapabilities  *caps
          );

          /*
           * Get a surface description that best matches the video
           * contained in the file.
           */
          DFBResult (*GetSurfaceDescription) (
               IDirectFBVideoProvider   *thiz,
               DFBSurfaceDescription    *dsc
          );


        /** Playback **/

          /*
           * Play the video rendering it into the specified rectangle
           * of the destination surface.
           *
           * Optionally a callback can be registered that is called
           * for each rendered frame. This is especially important if
           * you are playing to a flipping surface. In this case, you
           * should flip the destination surface in your callback.
           */
          DFBResult (*PlayTo) (
               IDirectFBVideoProvider   *thiz,
               IDirectFBSurface         *destination,
               DFBRectangle             *destination_rect,
               DVFrameCallback           callback,
               void                     *ctx
          );

          /*
           * Stop rendering into the destination surface.
           */
          DFBResult (*Stop) (
               IDirectFBVideoProvider   *thiz
          );


        /** Media Control **/

          /*
           * Seeks to a position within the stream.
           */
          DFBResult (*SeekTo) (
               IDirectFBVideoProvider   *thiz,
               double                    seconds
          );

          /*
           * Gets current position within the stream.
           */
          DFBResult (*GetPos) (
               IDirectFBVideoProvider   *thiz,
               double                   *seconds
          );

          /*
           * Gets the length of the stream.
           */
          DFBResult (*GetLength) (
               IDirectFBVideoProvider   *thiz,
               double                   *seconds
          );

        /** Color Adjustment **/

          /*
           * Gets the current video color settings.
           */
          DFBResult (*GetColorAdjustment) (
               IDirectFBVideoProvider   *thiz,
               DFBColorAdjustment       *adj
          );

          /*
           * Adjusts the video colors.
           *
           * This function only has an effect if the video provider
           * supports this operation. Check the providers capabilities
           * to find out if this is the case.
           */
          DFBResult (*SetColorAdjustment) (
               IDirectFBVideoProvider   *thiz,
               DFBColorAdjustment       *adj
          );
     )

#ifdef __cplusplus
}
#endif

#endif

