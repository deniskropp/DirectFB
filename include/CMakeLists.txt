include_directories(${CMAKE_CURRENT_BINARY_DIR}) 
set (MKNAMES ${PROJECT_SOURCE_DIR}/tools/mknames.sh)


add_custom_command(
	OUTPUT directfb_keynames.h
	COMMAND echo "\\#ifndef __DIRECTFB_KEYNAMES_H__" >  directfb_keynames.h
	COMMAND echo "\\#define __DIRECTFB_KEYNAMES_H__" >> directfb_keynames.h
	COMMAND ${MKNAMES} DFBInputDeviceKeySymbol DIKS NULL KeySymbol symbol ${CMAKE_CURRENT_SOURCE_DIR}/directfb_keyboard.h | grep -v DIKS_ENTER >> directfb_keynames.h
	COMMAND ${MKNAMES} DFBInputDeviceKeyIdentifier DIKI UNKNOWN KeyIdentifier identifier ${CMAKE_CURRENT_SOURCE_DIR}/directfb_keyboard.h | grep -v DIKI_NUMBER_OF_KEYS | grep -v DIKI_KEYDEF_END >> directfb_keynames.h
	COMMAND echo >> directfb_keynames.h
	COMMAND echo "\\#endif"  >> directfb_keynames.h
	DEPENDS directfb_keyboard.h
)

add_custom_command(
	OUTPUT directfb_strings.h
	COMMAND echo "\\#ifndef __DIRECTFB_STRINGS_H__" >  directfb_strings.h
	COMMAND echo "\\#define __DIRECTFB_STRINGS_H__" >> directfb_strings.h
	COMMAND echo "\\#include \\<directfb.h\\>"      >> directfb_strings.h
	COMMAND ${MKNAMES} DFBSurfacePixelFormat DSPF UNKNOWN PixelFormat format  ${CMAKE_CURRENT_SOURCE_DIR}/directfb.h >> directfb_strings.h
	COMMAND ${MKNAMES} DFBSurfacePorterDuffRule DSPD NONE PorterDuffRule rule ${CMAKE_CURRENT_SOURCE_DIR}/directfb.h >> directfb_strings.h
	COMMAND ${MKNAMES} DFBSurfaceCapabilities DSCAPS NONE SurfaceCapabilities capability ${CMAKE_CURRENT_SOURCE_DIR}/directfb.h | grep -v DSCAPS_FLIPPING | grep -v DSCAPS_ALL >> directfb_strings.h
	COMMAND ${MKNAMES} DFBSurfaceColorSpace DSCS UNKNOWN ColorSpace colorspace ${CMAKE_CURRENT_SOURCE_DIR}/directfb.h >> directfb_strings.h
	COMMAND ${MKNAMES} DFBInputDeviceTypeFlags DIDTF NONE InputDeviceTypeFlags type ${CMAKE_CURRENT_SOURCE_DIR}/directfb.h | grep -v DIDTF_ALL >> directfb_strings.h
	COMMAND ${MKNAMES} DFBSurfaceDrawingFlags DSDRAW NOFX SurfaceDrawingFlags flag ${CMAKE_CURRENT_SOURCE_DIR}/directfb.h | grep -v DSDRAW_ALL >> directfb_strings.h
	COMMAND ${MKNAMES} DFBSurfaceBlittingFlags DSBLIT NOFX SurfaceBlittingFlags flag ${CMAKE_CURRENT_SOURCE_DIR}/directfb.h | grep -v DSBLIT_ALL >> directfb_strings.h
	COMMAND ${MKNAMES} DFBSurfaceFlipFlags DSFLIP NONE SurfaceFlipFlags flag ${CMAKE_CURRENT_SOURCE_DIR}/directfb.h | grep -v DSFLIP_ALL >> directfb_strings.h
	COMMAND ${MKNAMES} DFBSurfaceBlendFunction DSBF UNKNOWN SurfaceBlendFunction function ${CMAKE_CURRENT_SOURCE_DIR}/directfb.h >> directfb_strings.h
	COMMAND ${MKNAMES} DFBInputDeviceCapabilities DICAPS NONE InputDeviceCapabilities capability ${CMAKE_CURRENT_SOURCE_DIR}/directfb.h | grep -v DICAPS_ALL | grep -v DICAPS_ALPHACHANNEL | grep -v DICAPS_COLORKEY >> directfb_strings.h
	COMMAND ${MKNAMES} DFBDisplayLayerTypeFlags DLTF NONE DisplayLayerTypeFlags type ${CMAKE_CURRENT_SOURCE_DIR}/directfb.h | grep -v DLTF_ALL >> directfb_strings.h
	COMMAND ${MKNAMES} DFBDisplayLayerCapabilities DLCAPS NONE DisplayLayerCapabilities capability ${CMAKE_CURRENT_SOURCE_DIR}/directfb.h grep -v DLCAPS_ALL >> directfb_strings.h
	COMMAND ${MKNAMES} DFBDisplayLayerBufferMode DLBM UNKNOWN DisplayLayerBufferMode mode ${CMAKE_CURRENT_SOURCE_DIR}/directfb.h | grep -v DLBM_DONTCARE | grep -v DLBM_COLOR | grep -v DLBM_IMAGE | grep -v DLBM_TILE >> directfb_strings.h
	COMMAND ${MKNAMES} DFBWindowCapabilities DWCAPS NONE WindowCapabilities capability ${CMAKE_CURRENT_SOURCE_DIR}/directfb.h | grep -v DWCAPS_ALL >> directfb_strings.h
	COMMAND ${MKNAMES} DFBWindowOptions DWOP NONE WindowOptions option ${CMAKE_CURRENT_SOURCE_DIR}/directfb.h | grep -v DWOP_ALL >> directfb_strings.h
	COMMAND ${MKNAMES} DFBScreenCapabilities DSCCAPS NONE ScreenCapabilities capability ${CMAKE_CURRENT_SOURCE_DIR}/directfb.h | grep -v DSCCAPS_ALL >> directfb_strings.h
	COMMAND ${MKNAMES} DFBScreenEncoderCapabilities DSECAPS NONE ScreenEncoderCapabilities capability ${CMAKE_CURRENT_SOURCE_DIR}/directfb.h | grep -v DSECAPS_ALL >> directfb_strings.h
	COMMAND ${MKNAMES} DFBScreenEncoderType DSET UNKNOWN ScreenEncoderType type ${CMAKE_CURRENT_SOURCE_DIR}/directfb.h | grep -v DSET_ALL >> directfb_strings.h
	COMMAND ${MKNAMES} DFBScreenEncoderTVStandards DSETV UNKNOWN ScreenEncoderTVStandards standard ${CMAKE_CURRENT_SOURCE_DIR}/directfb.h | grep -v DSETV_ALL >> directfb_strings.h
	COMMAND ${MKNAMES} DFBScreenOutputCapabilities DSOCAPS NONE ScreenOutputCapabilities capability ${CMAKE_CURRENT_SOURCE_DIR}/directfb.h | grep -v DSOCAPS_ALL >> directfb_strings.h
	COMMAND ${MKNAMES} DFBScreenOutputConnectors DSOC UNKNOWN ScreenOutputConnectors connector ${CMAKE_CURRENT_SOURCE_DIR}/directfb.h | grep -v DSOC_ALL >> directfb_strings.h
	COMMAND ${MKNAMES} DFBScreenOutputSignals DSOS NONE ScreenOutputSignals signal ${CMAKE_CURRENT_SOURCE_DIR}/directfb.h | grep -v DSOS_ALL >> directfb_strings.h
	COMMAND ${MKNAMES} DFBScreenOutputSlowBlankingSignals DSOSB OFF ScreenOutputSlowBlankingSignals slow_signal ${CMAKE_CURRENT_SOURCE_DIR}/directfb.h | grep -v DSOSB_ALL >> directfb_strings.h
	COMMAND ${MKNAMES} DFBScreenOutputResolution DSOR UNKNOWN ScreenOutputResolution resolution ${CMAKE_CURRENT_SOURCE_DIR}/directfb.h | grep -v DSOR_ALL >> directfb_strings.h
	COMMAND ${MKNAMES} DFBScreenMixerCapabilities DSMCAPS NONE ScreenMixerCapabilities capability ${CMAKE_CURRENT_SOURCE_DIR}/directfb.h | grep -v DSMCAPS_ALL >> directfb_strings.h
	COMMAND ${MKNAMES} DFBScreenMixerTree DSMT UNKNOWN ScreenMixerTree tree ${CMAKE_CURRENT_SOURCE_DIR}/directfb.h | grep -v DSMT_ALL >> directfb_strings.h
	COMMAND ${MKNAMES} DFBScreenEncoderTestPicture DSETP OFF ScreenEncoderTestPicture test_picture ${CMAKE_CURRENT_SOURCE_DIR}/directfb.h | grep -v DSETP_ALL >> directfb_strings.h
	COMMAND ${MKNAMES} DFBScreenEncoderScanMode DSESM UNKNOWN ScreenEncoderScanMode scan_mode ${CMAKE_CURRENT_SOURCE_DIR}/directfb.h | grep -v DSESM_ALL >> directfb_strings.h
	COMMAND ${MKNAMES} DFBScreenEncoderConfigFlags DSECONF UNKNOWN ScreenEncoderConfigFlags config_flags ${CMAKE_CURRENT_SOURCE_DIR}/directfb.h | grep -v DSECONF_ALL >> directfb_strings.h
	COMMAND ${MKNAMES} DFBScreenEncoderFrequency DSEF UNKNOWN ScreenEncoderFrequency frequency ${CMAKE_CURRENT_SOURCE_DIR}/directfb.h | grep -v DSEF_ALL >> directfb_strings.h
	COMMAND ${MKNAMES} DFBScreenEncoderPictureFraming DSEPF UNKNOWN ScreenEncoderPictureFraming framing ${CMAKE_CURRENT_SOURCE_DIR}/directfb.h | grep -v DSEPF_ALL >> directfb_strings.h
	COMMAND ${MKNAMES} DFBAccelerationMask DFXL NONE AccelerationMask mask ${CMAKE_CURRENT_SOURCE_DIR}/directfb.h | grep -v DFXL_ALL >> directfb_strings.h
	COMMAND echo >> directfb_strings.h
	COMMAND echo "\\#endif" >> directfb_strings.h
	DEPENDS directfb.h
)

add_custom_command(
	OUTPUT directfb_water_strings.h
	COMMAND echo "\\#ifndef __DIRECTFB_WATER_STRINGS_H__" >  directfb_water_strings.h
	COMMAND echo "\\#define __DIRECTFB_WATER_STRINGS_H__" >> directfb_water_strings.h
	COMMAND echo "\\#include \\<directfb.h\\>"            >> directfb_water_strings.h
	COMMAND echo "\\#include \\<directfb_water.h\\>"      >> directfb_water_strings.h
	COMMAND ${MKNAMES}  WaterElementType WET UNKNOWN WaterElementType type ${CMAKE_CURRENT_SOURCE_DIR}/directfb_water.h | grep -v WET_ALL >> directfb_water_strings.h
	COMMAND echo >> directfb_water_strings.h
	COMMAND echo "\\#endif"  >> directfb_water_strings.h
	DEPENDS directfb_water.h
)

add_custom_target( includes ALL SOURCES
	${CMAKE_CURRENT_BINARY_DIR}/directfb_keynames.h
	${CMAKE_CURRENT_BINARY_DIR}/directfb_strings.h
	${CMAKE_CURRENT_BINARY_DIR}/directfb_water_strings.h
)

install (FILES
	directfb.h
	dfb_types.h
	directfbgl.h
	directfb_keyboard.h
	directfb_util.h
	${CMAKE_CURRENT_BINARY_DIR}/directfb_water_strings.h
	dfiff.h
	${CMAKE_CURRENT_BINARY_DIR}/directfb_build.h
	directfb_graphics.h
	${CMAKE_CURRENT_BINARY_DIR}/directfb_keynames.h
	${CMAKE_CURRENT_BINARY_DIR}/directfb_version.h
	directfb_windows.h
	dgiff.h
	directfbgl2.h
	directfb.h
	${CMAKE_CURRENT_BINARY_DIR}/directfb_strings.h
	directfb_water.h
	
	DESTINATION include/directfb
)

