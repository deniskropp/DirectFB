/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

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
      * Layer configuration, creation of windows and background configuration.
      */
     DECLARE_INTERFACE( IDirectFBDisplayLayer )

     /*
      * Surface locking, setting colorkeys and other drawing parameters,
      * clipping, flipping, blitting, drawing.
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
      * An input buffer puts all events of one device into a FIFO.
      */
     DECLARE_INTERFACE( IDirectFBInputBuffer )

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
          DFB_MISSINGIMAGE    /* no image has been set */
     } DFBResult;

     /*
      * Rectangle specified by a point and a dimension.
      */
     typedef struct {
          int            x;   /* X coordinate of top-left point */
          int            y;   /* Y coordinate of top-left point */
          int            w;   /* width of that rectangle */
          int            h;   /* height of that rectangle */
     } DFBRectangle;

     /*
      * Region specified by two points.
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
      * Parses the command-line and initializes some variables. You
      * absolutely need to call this before doing anything else.
      * Removes all options used by DirectFB from argv.
      */
     DFBResult DirectFBInit(
                              int         *argc,   /* main()'s argc */
                              char       **argv[]  /* main()'s argv */
                           );

     /*
      * Sets configuration parameters supported on command line
      * and in config file. Has to be called before DirectFBCreate but after
      * DirectFBInit.
      */
     DFBResult DirectFBSetOption(
                              char        *name,
                              char        *value
                           );

     /*
      * Create the super interface
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
                                                     default is 0, 0 - 1, 1. */
          DLCAPS_FLICKER_FILTERING = 0x00000010,  /* Flicker filtering can be
                                                     enabled for this layer. */
          DLCAPS_INTERLACED_VIDEO  = 0x00000020,  /* The layer can display
                                                     interlaced video data. */
          DLCAPS_COLORKEYING       = 0x00000040   /* A specific color can be
                                                     declared as transparent. */
     } DFBDisplayLayerCapabilities;

     /*
      * Used to enable some capabilities like flicker filtering
      * or colorkeying. Usage of capabilities not listed here depend
      * on their settings, e.g. if layer opacity is not set to 100% the opacity
      * capability is required and used.
      */
     typedef enum {
          DLOP_ALPHACHANNEL        = 0x00000001,  /* Make usage of alpha channel
                                                     for blending on a pixel per
                                                     pixel basis. */
          DLOP_FLICKER_FILTERING   = 0x00000002,  /* Enable flicker
                                                     filtering. */
          DLOP_INTERLACED_VIDEO    = 0x00000004,  /* Source is interlaced. */
          DLOP_COLORKEYING         = 0x00000008   /* Enable colorkeying. */
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
          DSDESC_PIXELFORMAT  = 0x00000008   /* pixelformat field is valid */
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
          DSCAPS_SUBSURFACE   = 0x00000020   /* surface is just a sub area of
                                                another one */
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
          DSBLIT_DST_COLORKEY       = 0x00000010  /* write to destination only
                                                     if destination color
                                                     key matches */
     } DFBSurfaceBlittingFlags;

     /*
      * Mask of accelerated functions
      */
     typedef enum {
          DFXL_NONE           = 0x00000000,  /* none of these */
          DFXL_FILLRECTANGLE  = 0x00000001,  /* FillRectangle */
          DFXL_DRAWRECTANGLE  = 0x00000002,  /* DrawRectangle */
          DFXL_DRAWLINE       = 0x00000004,  /* DrawLine */
          DFXL_DRAWSTRING     = 0x00000008,  /* DrawString */
          DFXL_FILLTRIANGLE   = 0x00000010,  /* FillTriangle */

          DFXL_BLIT           = 0x00010000,  /* Blit */
          DFXL_STRETCHBLIT    = 0x00020000,  /* StretchBlit */

          DFXL_ALL            = 0x0003000F   /* all drawing/blitting
                                                functions */
     } DFBAccelerationMask;

     /*
      * Rough information about hardware capabilities.
      */
     typedef struct {
          DFBAccelerationMask     acceleration_mask;   /* drawing/blitting
                                                          functions */
          DFBSurfaceDrawingFlags  drawing_flags;       /* drawing flags */
          DFBSurfaceBlittingFlags blitting_flags;      /* blitting flags */
     } DFBCardCapabilities;

     /*
      * Type of input device for basic classification.
      * Values may be or'ed together.
      */
     typedef enum {
          DIDTF_KEYBOARD      = 0x00000001,  /* can act as a keyboard */
          DIDTF_MOUSE         = 0x00000002,  /* can be used as a mouse */
          DIDTF_JOYSTICK      = 0x00000004,  /* can be used as a joystick */
          DIDTF_REMOTE        = 0x00000008   /* device is a remote control */
     } DFBInputDeviceTypeFlags;

     /*
      * Basic input device features.
      */
     typedef enum {
          DICAPS_KEYS         = 0x00000001,  /* device supports key events */
          DICAPS_AXIS         = 0x00000002,  /* device supports axis events */
          DICAPS_BUTTONS      = 0x00000004   /* device supports button events */
     } DFBInputDeviceCapabilities;

     /*
      * Identifier (index) for e.g. mouse or joystick buttons.
      */
     typedef enum {
          DIBI_LEFT           = 0x00000000,  /* left mouse button */
          DIBI_RIGHT          = 0x00000001,  /* right mouse button */
          DIBI_MIDDLE         = 0x00000002,  /* middle mouse button */

          DIBI_FIRST          = DIBI_LEFT    /* other buttons:
                                                DIBI_FIRST + zero based index */
     } DFBInputDeviceButtonIdentifier;

     /*
      * Axis identifier (index) for e.g. mouse or joystick.
      */
     typedef enum {
          DIAI_X              = 0x00000000,  /* X axis */
          DIAI_Y              = 0x00000001,  /* Y axis */
          DIAI_Z              = 0x00000002,  /* Z axis */

          DIAI_FIRST          = DIAI_X       /* other axis:
                                                DIAI_FIRST + zero based index */
     } DFBInputDeviceAxisIdentifier;

     /*
      * Flags defining which fields of a DFBWindowDescription are valid.
      */
     typedef enum {
          DWDESC_CAPS         = 0x00000001,  /* caps field is valid */
          DWDESC_WIDTH        = 0x00000002,  /* width field is valid */
          DWDESC_HEIGHT       = 0x00000004,  /* height field is valid */
          DWDESC_POSX         = 0x00000008,  /* posx field is valid */
          DWDESC_POSY         = 0x00000010   /* posy field is valid */
     } DFBWindowDescriptionFlags;

     /*
      * Capabilities a window can have.
      */
     typedef enum {
          DWCAPS_ALPHACHANNEL = 0x00000001   /* window has an alphachannel */
     } DFBWindowCapabilities;


     /*
      * Flags describing how to load a font.
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
          DFDESC_HEIGHT       = 0x00000002   /* height is specified */
     } DFBFontDescriptionFlags;

     /* Pixel format of a surface.
      * Contains information about the format (see following macros).
      */
     typedef enum {
          DSPF_UNKNOWN        = 0x00000000,  /* no specific format,
                                                unusual and unsupported */
          DSPF_RGB15          = 0x00020F01,  /* 15bit  RGB (2 bytes, red 5@10,
                                                green 5@5, blue 5@0) */
          DSPF_RGB16          = 0x00021002,  /* 16bit  RGB (2 bytes, red 5@11,
                                                green 6@5, blue 5@0) */
          DSPF_RGB24          = 0x00031803,  /* 24bit  RGB (3 bytes, red 8@16,
                                                green 8@8, blue 8@0) */
          DSPF_RGB32          = 0x00041804,  /* 24bit  RGB (4 bytes, nothing@24,
                                                red 8@16, green 8@8, blue 8@0)*/
          DSPF_ARGB           = 0x00042005,  /* 32bit ARGB (4 bytes, alpha 8@24,
                                                red 8@16, green 8@8, blue 8@0)*/
          DSPF_A8             = 0x00010806,  /* 8bit alpha (1 byte, alpha 8@0 ),
                                                e.g. anti-aliased text glyphs */
          DSPF_A1             = 0x00010107   /* 1bit alpha (8 pixel ber byte),
                                                e.g. bitmap text glyphs, support
                                                for this pixel format is
                                                currently broken */
     } DFBSurfacePixelFormat;


     /*
      * Description of the surface that is to be created.
      */
     typedef struct {
          DFBSurfaceDescriptionFlags         flags;      /* field validation */

          DFBSurfaceCapabilities             caps;        /* capabilities */
          unsigned int                       width;       /* pixel width */
          unsigned int                       height;      /* pixel height */
          DFBSurfacePixelFormat              pixelformat; /* pixel format */
     } DFBSurfaceDescription;

     /*
      * Description of the input device capabilities.
      */
     typedef struct {
          DFBInputDeviceTypeFlags            type;       /* classification of
                                                            input device */
          DFBInputDeviceCapabilities         caps;       /* capabilities,
                                                            validates the
                                                            following fields */

          DFBInputDeviceAxisIdentifier       max_axis;   /* highest axis
                                                            identifier */
          DFBInputDeviceButtonIdentifier     max_button; /* highest button
                                                            identifier */
     } DFBInputDeviceDescription;

     /*
      * Description of the window that is to be created.
      */
     typedef struct {
          DFBWindowDescriptionFlags          flags;      /* field validation */

          DFBWindowCapabilities              caps;       /* capabilities */
          unsigned int                       width;      /* pixel width */
          unsigned int                       height;     /* pixel height */
          int                                posx;       /* distance from left
                                                            layer border */
          int                                posy;       /* distance from upper
                                                            layer border */
     } DFBWindowDescription;

     /*
      * Called for each supported video mode.
      */
     typedef int (*DFBVideoModeCallback) (
          unsigned int                       width,
          unsigned int                       height,
          unsigned int                       bpp,
          void                              *callbackdata
     );

     /*
      * Called for each existing display layer.
      * "layer_id" can be used to get an interface to the layer.
      */
     typedef int (*DFBDisplayLayerCallback) (
          unsigned int                       layer_id,
          DFBDisplayLayerCapabilities        caps,
          void                              *callbackdata
     );

     /*
      * Called for each existing input device.
      * "device_id" can be used to get an interface to the device.
      */
     typedef int (*DFBInputDeviceCallback) (
          unsigned int                       device_id,
          DFBInputDeviceDescription          desc,
          void                              *callbackdata
     );

     /*
      * Called for each block of continous data requested,
      * e.g. by a Video Provider. Write as many data as you can but
      * not more than specified by length. Return the number of bytes written
      * or 'EOF' if no data is available anymore.
      */
     typedef int (*DFBGetDataCallback) (
          void                              *buffer,
          unsigned int                       length,
          void                              *callbackdata
     );

     /*
      * Description of how to load the glyphs from the font file.
      */
     typedef struct {
          DFBFontDescriptionFlags            flags;

          unsigned int                       attributes;
          unsigned int                       height;
     } DFBFontDescription;

     /* 
      * Information about an IDirectFBVideoProvider.
      */
     typedef enum {
          DVCAPS_BASIC      = 0x00000000,  /* basic ops (PlayTo, Stop)       */
          DVCAPS_SEEK       = 0x00000001,  /* supports SeekTo                */
          DVCAPS_SCALE      = 0x00000002,  /* can scale the video            */
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


     /*************
      * IDirectFB *
      *************/

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
           * If in shared cooperative level this function sets the resolution
           * of the window that is created implicitly for the primary surface.
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
               unsigned int              layer_id,
               IDirectFBDisplayLayer   **interface
          );


        /** Input Devices **/

          /*
           * Enumerate all existing input devices.
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
               unsigned int              device_id,
               IDirectFBInputDevice    **interface
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
           * Create a streamed video provider that uses the callback function
           * to retrieve data. Callback is called one time during creation
           * to determine the data type.
           */
          DFBResult (*CreateStreamedVideoProvider) (
               IDirectFB                *thiz,
               DFBGetDataCallback        callback,
               void                     *callback_data,
               IDirectFBVideoProvider  **interface
          );

          /*
           * Load a font from the specified file given a description of how
           * to load the glyphs.
           */
          DFBResult (*CreateFont) (
               IDirectFB                *thiz,
               const char               *filename,
               DFBFontDescription       *desc,
               IDirectFBFont           **interface
          );


        /** Misc **/

          /*
           * Suspend DirectFB, no other calls to DirectFB allowed until
           * Resume has been called.
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
     #define DIDID_KEYBOARD        0    /* primary keyboard       */
     #define DIDID_MOUSE           1    /* primary mouse          */
     #define DIDID_JOYSTICK        2    /* primary joystick       */
     #define DIDID_REMOTE          3    /* primary remote control */


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
          DLBM_IMAGE                    /* use an image (SetBackgroundImage) */
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

     DEFINE_INTERFACE(   IDirectFBDisplayLayer,

        /** Capabilities, surface **/

          /*
           * Get the layer's capabilities.
           */
          DFBResult (*GetCapabilities) (
               IDirectFBDisplayLayer              *thiz,
               DFBDisplayLayerCapabilities        *caps
          );

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
           * Set color key, i.e. the color that makes a pixel transparent.
           *
           * Note: Parameters are subject to change.
           */
          DFBResult (*SetColorKey) (
               IDirectFBDisplayLayer              *thiz,
               __u32                               key
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
           * If configuration fails and 'failed' is not NULL it will indicate
           * which fields of the configuration caused the error.
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


        /** Cursor handling **/

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
          DSTF_LEFT           = 0x00000000,  /* use this rather than '0',
                                                type is valid (for C++) */
          DSTF_CENTER         = 0x00000001,  /* prints the string center
                                                aligned rather than left */
          DSTF_RIGHT          = 0x00000002,  /* right aligned */

          DSTF_TOP            = 0x00000004,  /* y specifies the top
                                                instead of the baseline */

          DSTF_TOPLEFT        = DSTF_TOP | DSTF_LEFT,
          DSTF_TOPCENTER      = DSTF_TOP | DSTF_CENTER,
          DSTF_TOPRIGHT       = DSTF_TOP | DSTF_RIGHT
     } DFBSurfaceTextFlags;

     /*
      * Flags defining the type of data access.
      * These are important for surface swapping management.
      */
     typedef enum {
          DSLF_READ           = 0x00000001,  /* request read access while
                                                surface is locked*/
          DSLF_WRITE          = 0x00000002   /* request write access */
     } DFBSurfaceLockFlags;


     /* These macros extract information about the pixel format. */
     #define BYTES_PER_PIXEL(format)    (((format) & 0xFF0000) >> 16)
     #define BITS_PER_PIXEL(format)     (((format) & 0x00FF00) >>  8)
     #define PIXELFORMAT_INDEX(format)  (((format) & 0x0000FF) - 1)

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
           * Get width and height in pixels.
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
           * For non sub surfaces this function returns { 0, 0, width, height }.
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
           * Get a mask of drawing functions that are
           * hardware accelerated with the current settings.
           *
           * If a source surface is specified the mask will
           * also contain accelerated blitting functions.
           * Note that there is no guarantee that these will
           * actually be accelerated since the surface storage
           * (video/system) is examined only when something
           * actually gets drawn or blitted.
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


        /** Drawing/blitting control **/

          /*
           * Set the clipping rectangle used to limitate the area
           * for drawing, blitting and text functions.
           */
          DFBResult (*SetClip) (
               IDirectFBSurface         *thiz,
               DFBRegion                *clip
          );

          /*
           * Set the color used for alpha/color modulation,
           * (blended) drawing and text functions.
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
           * Set the source colorkey, i.e. the pixel value that is
           * excluded from the source when blitting to this surface.
           */
          DFBResult (*SetSrcColorKey) (
               IDirectFBSurface         *thiz,
               __u32                     key
          );

          /*
           * Set the destination color key, i.e. the only pixel value
           * that gets overwritten by drawing, blitting and text functions.
           */
          DFBResult (*SetDstColorKey) (
               IDirectFBSurface         *thiz,
               __u32                     key
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
           * Pass a NULL rectangle to use the whole surface.
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
           * Blit an area scaled from the source to the destination rectangle.
           *
           * Pass a NULL rectangle to use the whole surface.
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
           * Draw an outline of the specified rectangle with the given color
           * following the specified flags.
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
           * Draw 'num_lines' lines with the given color
           * following the drawing flags. Each line specified by a DFBRegion.
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
           * Set the font used by DrawString().
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
           * Bytes specifies the number of bytes to take from the string
           * or -1 for the complete NULL-terminated string. You need to set
           * a font using the SetFont() method before calling this function.
           */
          DFBResult (*DrawString) (
               IDirectFBSurface         *thiz,
               const char               *text,
               int                       bytes,
               int                       x,
               int                       y,
               DFBSurfaceTextFlags       flags
          );


        /** Leightweight helpers **/

          /*
           * Get an interface to a sub area of this surface.
           *
           * No image data is duplicated, this is a clipped graphics within the
           * original surface. This is very helpful for leightweight components
           * in a GUI toolkit.
           * The new surface's state (color, drawingflags, etc.) is independent
           * from this one. So it's a handy graphics context.
           * If no rectangle is specified, the whole surface (or a part if this
           * surface is a subsurface itself) is represented by the new one.
           */
          DFBResult (*GetSubSurface) (
               IDirectFBSurface         *thiz,
               DFBRectangle             *rect,
               IDirectFBSurface        **interface
          );
     )



     /*
      * DirectFB keycodes
      */
     typedef enum {
          DIKC_UNKNOWN = 0,

          DIKC_A, DIKC_B, DIKC_C, DIKC_D, DIKC_E, DIKC_F, DIKC_G, DIKC_H,
          DIKC_I, DIKC_J, DIKC_K, DIKC_L, DIKC_M, DIKC_N, DIKC_O, DIKC_P,
          DIKC_Q, DIKC_R, DIKC_S, DIKC_T, DIKC_U, DIKC_V, DIKC_W, DIKC_X,
          DIKC_Y, DIKC_Z,

          DIKC_0, DIKC_1, DIKC_2, DIKC_3, DIKC_4, DIKC_5, DIKC_6, DIKC_7,
          DIKC_8, DIKC_9,

          DIKC_F1, DIKC_F2, DIKC_F3, DIKC_F4, DIKC_F5, DIKC_F6, DIKC_F7,
          DIKC_F8, DIKC_F9, DIKC_F10, DIKC_F11, DIKC_F12,

          DIKC_ESCAPE,
          DIKC_LEFT, DIKC_RIGHT, DIKC_UP, DIKC_DOWN,
          DIKC_CTRL, DIKC_SHIFT,
          DIKC_ALT, DIKC_ALTGR,
          DIKC_TAB, DIKC_ENTER, DIKC_SPACE, DIKC_BACKSPACE,
          DIKC_INSERT, DIKC_DELETE, DIKC_HOME, DIKC_END,
          DIKC_PAGEUP, DIKC_PAGEDOWN,
          DIKC_CAPSLOCK, DIKC_NUMLOCK, DIKC_SCRLOCK, DIKC_PRINT, DIKC_PAUSE,
          DIKC_KP_DIV, DIKC_KP_MULT, DIKC_KP_MINUS, DIKC_KP_PLUS,
          DIKC_KP_ENTER,

          DIKC_OK, DIKC_CANCEL, DIKC_SELECT, DIKC_GOTO, DIKC_CLEAR,
          DIKC_POWER, DIKC_POWER2, DIKC_OPTION,
          DIKC_MENU, DIKC_HELP, DIKC_INFO, DIKC_TIME, DIKC_VENDOR,

          DIKC_ARCHIVE, DIKC_PROGRAM, DIKC_FAVORITES, DIKC_EPG,
          DIKC_LANGUAGE, DIKC_TITLE, DIKC_SUBTITLE, DIKC_ANGLE,
          DIKC_ZOOM, DIKC_MODE, DIKC_KEYBOARD, DIKC_PC, DIKC_SCREEN,

          DIKC_TV, DIKC_TV2, DIKC_VCR, DIKC_VCR2, DIKC_SAT, DIKC_SAT2,
          DIKC_CD, DIKC_TAPE, DIKC_RADIO, DIKC_TUNER, DIKC_PLAYER,
          DIKC_TEXT, DIKC_DVD, DIKC_AUX, DIKC_MP3,
          DIKC_AUDIO, DIKC_VIDEO, DIKC_INTERNET, DIKC_MAIL, DIKC_NEWS,

          DIKC_RED, DIKC_GREEN, DIKC_YELLOW, DIKC_BLUE,

          DIKC_CHANNELUP, DIKC_CHANNELDOWN, DIKC_BACK, DIKC_FORWARD,
          DIKC_VOLUMEUP, DIKC_VOLUMEDOWN, DIKC_MUTE, DIKC_AB,

          DIKC_PLAYPAUSE, DIKC_PLAY, DIKC_STOP, DIKC_RESTART,
          DIKC_SLOW, DIKC_FAST, DIKC_RECORD, DIKC_EJECT, DIKC_SHUFFLE,
          DIKC_REWIND, DIKC_FASTFORWARD, DIKC_PREVIOUS, DIKC_NEXT,

          DIKC_DIGITS, DIKC_TEEN, DIKC_TWEN, DIKC_ASTERISK, DIKC_HASH,

          DIKC_NUMBER_OF_KEYS
     } DFBInputDeviceKeyIdentifier;

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
      * Flags specifying the modifiers that are currently pressed.
      */
     typedef enum {
          DIMK_SHIFT          = 0x00000001,  /* any shift key down? */
          DIMK_CTRL           = 0x00000002,  /* any ctrl key down? */
          DIMK_ALT            = 0x00000004,  /* alt key down? */
          DIMK_ALTGR          = 0x00000008   /* altgr key down? */
     } DFBInputDeviceModifierKeys;

     /*
      * Flags specifying which buttons are currently down.
      */
     typedef enum {
          DIBM_LEFT           = 0x00000001,  /* left mouse button */
          DIBM_RIGHT          = 0x00000002,  /* right mouse button */
          DIBM_MIDDLE         = 0x00000004   /* middle mouse button */
     } DFBInputDeviceButtonMask;


     /************************
      * IDirectFBInputDevice *
      ************************/

     DEFINE_INTERFACE(   IDirectFBInputDevice,

        /** Input buffers **/

          /*
           * Create an input buffer for this device.
           */
          DFBResult (*CreateInputBuffer) (
               IDirectFBInputDevice          *thiz,
               IDirectFBInputBuffer         **buffer
          );


        /** Retrieving information **/

          /*
           * Get a description of this device, i.e. the capabilities.
           */
          DFBResult (*GetDescription) (
               IDirectFBInputDevice          *thiz,
               DFBInputDeviceDescription     *desc
          );


        /** General state queries **/

          /*
           * Get the current state of one key.
           */
          DFBResult (*GetKeyState) (
               IDirectFBInputDevice          *thiz,
               DFBInputDeviceKeyIdentifier    keycode,
               DFBInputDeviceKeyState        *state
          );

          /*
           * Get the current modifier mask.
           */
          DFBResult (*GetModifiers) (
               IDirectFBInputDevice          *thiz,
               DFBInputDeviceModifierKeys    *modifiers
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
      * Flags defining which event fields are valid.
      */
     typedef enum {
          DIEF_KEYCODE        = 0x01,   /* keycode is valid */
          DIEF_MODIFIERS      = 0x04,   /* modifiers are valid */
          DIEF_BUTTON         = 0x08,   /* button is valid */
          DIEF_AXISABS        = 0x10,   /* axis and axisabs are valid */
          DIEF_AXISREL        = 0x20    /* axis and axisrel are valid */
     } DFBInputEventFlags;

     /*
      * An input event, item of an input buffer.
      */
     typedef struct {
          DFBInputEventType             type;          /* type of event */
          DFBInputEventFlags            flags;         /* which fields are
                                                          valid? */

     /* DIET_KEYPRESS, DIET_KEYRELEASE, DIET_KEYREPEAT */
          DFBInputDeviceKeyIdentifier   keycode;       /* in case of a key
                                                          event */
          char                          key_ascii;
          __u32                         key_unicode;
          DFBInputDeviceModifierKeys    modifiers;     /* modifier keys as
                                                          a bitmask */

     /* DIET_BUTTONPRESS, DIET_BUTTONRELEASE */
          DFBInputDeviceButtonIdentifier     button;   /* in case of a button
                                                          event */

     /* DIET_AXISMOTION */
          DFBInputDeviceAxisIdentifier       axis;     /* in case of an axis
                                                          event */
          int                                axisabs;  /* absolute mouse/
                                                          joystick coordinate */
          int                                axisrel;  /* relative mouse/
                                                          joystick movement */
     } DFBInputEvent;


     /************************
      * IDirectFBInputBuffer *
      ************************/

     DEFINE_INTERFACE(   IDirectFBInputBuffer,


        /** Buffer handling **/

          /*
           * Clear all events stored in this buffer.
           */
          DFBResult (*Reset) (
               IDirectFBInputBuffer     *thiz
          );


        /** Event handling **/

          /*
           * Wait for the next event to occur.
           * Thread is idle in the meantime.
           */
          DFBResult (*WaitForEvent) (
               IDirectFBInputBuffer     *thiz
          );

          /*
           * Block until next event to occurs or timeout is reached.
           * Thread is idle in the meantime.
           */
          DFBResult (*WaitForEventWithTimeout) (
               IDirectFBInputBuffer     *thiz,
               long int                  seconds,
               long int                  nano_seconds
          );

          /*
           * Get the next event and remove from the FIFO.
           */
          DFBResult (*GetEvent) (
               IDirectFBInputBuffer     *thiz,
               DFBInputEvent            *event
          );

          /*
           * Get the next event but leave it there, i.e. do a preview.
           */
          DFBResult (*PeekEvent) (
               IDirectFBInputBuffer     *thiz,
               DFBInputEvent            *event
          );
     )



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
          DWET_CLOSE          = 0x00000004,  /* window got closed by window
                                                manager or the application
                                                itself */
          DWET_GOTFOCUS       = 0x00000008,  /* window got focus */
          DWET_LOSTFOCUS      = 0x00000010,  /* window lost focus */

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

          DWET_POSITION_SIZE  = DWET_POSITION | DWET_SIZE /* initially sent to
                                                             window when it's
                                                             created */
     } DFBWindowEventType;

     /*
      * Event from the windowing system.
      */
     typedef struct {
          DFBWindowEventType                 type;

          /* used by DWET_MOVE, DWET_MOTION, DWET_BUTTONDOWN, DWET_BUTTONUP,
             DWET_ENTER, DWET_LEAVE */
          int                                x;
          int                                y;

          /* used by DWET_MOTION, DWET_BUTTONDOWN, DWET_BUTTONUP,
             DWET_ENTER, DWET_LEAVE */
          int                                cx;
          int                                cy;

          /* used by DWET_RESIZE */
          unsigned int                       w;
          unsigned int                       h;

          /* used by DWET_KEYDOWN, DWET_KEYUP */
          DFBInputDeviceKeyIdentifier        keycode;
          __u8                               key_ascii;
          __u16                              key_unicode;
          DFBInputDeviceModifierKeys         modifiers;

          /* used by DWET_BUTTONDOWN, DWET_BUTTONUP */
          DFBInputDeviceButtonIdentifier     button;
     } DFBWindowEvent;


     /*******************
      * IDirectFBWindow *
      *******************/

     DEFINE_INTERFACE(   IDirectFBWindow,

        /** Retrieving information **/

          /*
           * Get the current position of this window.
           */
          DFBResult (*GetPosition) (
               IDirectFBWindow     *thiz,
               int                 *x,
               int                 *y
          );

          /*
           * Get the size of the window in pixels.
           */
          DFBResult (*GetSize) (
               IDirectFBWindow     *thiz,
               unsigned int        *width,
               unsigned int        *height
          );


        /** Surface handling **/

          /*
           * Get an interface to the backing store surface.
           *
           * This surface has to be flipped to make previous drawing
           * commands visible, i.e. to repaint the windowstack for that region.
           */
          DFBResult (*GetSurface) (
               IDirectFBWindow     *thiz,
               IDirectFBSurface   **surface
          );


        /** Appearance **/

          /*
           * Set the window's global opacity factor.
           *
           * Set it to "0" to hide a window.
           * Setting it to "0xFF" makes the window opaque if
           * it has no alpha channel.
           */
          DFBResult (*SetOpacity) (
               IDirectFBWindow     *thiz,
               __u8                 opacity
          );

          /*
           * Get the current opacity factor of this window.
           */
          DFBResult (*GetOpacity) (
               IDirectFBWindow     *thiz,
               __u8                *opacity
          );


        /** Focus handling **/

          /*
           * Pass the focus to this window.
           */
          DFBResult (*RequestFocus) (
               IDirectFBWindow     *thiz
          );

          /*
           * Grab the keyboard, i.e. all following keyboard events are sent to
           * this window ignoring the focus.
           */
          DFBResult (*GrabKeyboard) (
               IDirectFBWindow     *thiz
          );

          /*
           * Ungrab the keyboard, i.e. switch to standard key event dispatching.
           */
          DFBResult (*UngrabKeyboard) (
               IDirectFBWindow     *thiz
          );

          /*
           * Grab the pointer, i.e. all following mouse events are sent to
           * this window ignoring the focus.
           */
          DFBResult (*GrabPointer) (
               IDirectFBWindow     *thiz
          );

          /*
           * Ungrab the pointer, i.e. switch to standard mouse event dispatching.
           */
          DFBResult (*UngrabPointer) (
               IDirectFBWindow     *thiz
          );

        /** Positioning **/

          /*
           * Move the window by the specified distance.
           */
          DFBResult (*Move) (
               IDirectFBWindow     *thiz,
               int                  dx,
               int                  dy
          );

          /*
           * Move the window to the specified coordinates.
           */
          DFBResult (*MoveTo) (
               IDirectFBWindow     *thiz,
               int                  x,
               int                  y
          );

          /*
           * Resize the window.
           */
          DFBResult (*Resize) (
               IDirectFBWindow     *thiz,
               unsigned int         width,
               unsigned int         height
          );


        /** Stacking **/

          /*
           * Raise the window by one within the window stack.
           */
          DFBResult (*Raise) (
               IDirectFBWindow     *thiz
          );

          /*
           * Lower the window by one within the window stack.
           */
          DFBResult (*Lower) (
               IDirectFBWindow     *thiz
          );

          /*
           * Put the window on the top of the window stack.
           */
          DFBResult (*RaiseToTop) (
               IDirectFBWindow     *thiz
          );

          /*
           * Send a window to the bottom of the window stack.
           */
          DFBResult (*LowerToBottom) (
               IDirectFBWindow     *thiz
          );


        /** Event handling **/

          /*
           * Wait for the next event to occur.
           * Thread is idle in the meantime.
           */
          DFBResult (*WaitForEvent) (
               IDirectFBWindow     *thiz
          );

      /*
           * Block until next event to occurs or timeout is reached.
           * Thread is idle in the meantime.
           */
          DFBResult (*WaitForEventWithTimeout) (
               IDirectFBWindow     *thiz,
               long int                  seconds,
               long int                  nano_seconds
          );



          /*
           * Get the next event and remove from the FIFO.
           */
          DFBResult (*GetEvent) (
               IDirectFBWindow     *thiz,
               DFBWindowEvent      *event
          );

          /*
           * Get the next event but leave it there, i.e. do a preview.
           */
          DFBResult (*PeekEvent) (
               IDirectFBWindow     *thiz,
               DFBWindowEvent      *event
          );
     )


     /*****************
      * IDirectFBFont *
      *****************/

     DEFINE_INTERFACE(   IDirectFBFont,

        /** Retrieving information **/

          /*
           * Get the distance from the baseline to the top.
           */
          DFBResult (*GetAscender) (
               IDirectFBFont       *thiz,
               int                 *ascender
          );

          /*
           * Get the distance from the baseline to the bottom.
           *
           * This is a negative value!
           */
          DFBResult (*GetDescender) (
               IDirectFBFont       *thiz,
               int                 *descender
          );

          /*
           * Get the height of this font.
           */
          DFBResult (*GetHeight) (
               IDirectFBFont       *thiz,
               int                 *height
          );

          /*
           * Get the maximum character width.
           */
          DFBResult (*GetMaxAdvance) (
               IDirectFBFont       *thiz,
               int                 *maxadvance
          );


        /** String extents measurement **/

          /*
           * Get the logical width of the specified UTF-8 string
           * as if it were drawn with this font.
           *
           * Bytes specifies the number of bytes to take from
           * the string or -1 for the complete NULL-terminated
           * string.
           *
           * The returned width may be different than the actual
           * drawn width of the text since this function returns
           * the logical width that should be used to layout the
           * text. A negative width indicates right-to-left rendering.
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
           * Bytes specifies the number of bytes to take from
           * the string or -1 for the complete NULL-terminated
           * string.
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
           * The rectangles offsets are reported relative to the
           * baseline and refer to the text being drawn using DSTF_LEFT.
           */
          DFBResult (*GetStringExtents) (
               IDirectFBFont       *thiz,
               const char          *text,
               int                  bytes,
               DFBRectangle        *logical_rect,
               DFBRectangle        *ink_rect
          );
     )


     /**************************
      * IDirectFBImageProvider *
      **************************/

     DEFINE_INTERFACE(   IDirectFBImageProvider,

        /** Retrieving information **/

          /*
           * Get a surface description that best matches the image
           * contained in the file.
           *
           * For opaque image formats the pixel format of the
           * primary layer is used.
           */
          DFBResult (*GetSurfaceDescription) (
               IDirectFBImageProvider   *thiz,
               DFBSurfaceDescription    *dsc
          );


        /** Rendering **/

          /*
           * Render the file contents into the destination contents
           * doing automatic scaling and color format conversion.
           */
          DFBResult (*RenderTo) (
               IDirectFBImageProvider   *thiz,
               IDirectFBSurface         *destination
          );
     )

     /*
      * Called for each written frame.
      */
     typedef int (*DVFrameCallback)(void *ctx);


     /**************************
      * IDirectFBVideoProvider *
      **************************/

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
           * Optionally a callback can be registered that is
           * called for each frame.
           */
          DFBResult (*PlayTo) (
               IDirectFBVideoProvider   *thiz,
               IDirectFBSurface         *destination,
               DFBRectangle             *dstrect,
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

