include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")

configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/sawman.pc.in"
                "${CMAKE_CURRENT_BINARY_DIR}/sawman.pc" @ONLY)


add_custom_command(
	OUTPUT  sawman_strings.h
	COMMAND echo "\\#ifndef __SAWMAN__SAWMAN_STRINGS_H__" >  sawman_strings.h
	COMMAND echo "\\#define __SAWMAN__SAWMAN_STRINGS_H__" >> sawman_strings.h
	COMMAND echo "\\#include \\<sawman.h\\>"    >> sawman_strings.h
	COMMAND ${MKNAMES} SaWManProcessFlags SWMPF NONE SaWManProcessFlags flag ${CMAKE_CURRENT_SOURCE_DIR}/sawman.h | grep -v	SWMPF_ALL >> sawman_strings.h
	COMMAND echo               >> sawman_strings.h
	COMMAND echo "\\#endif"    >> sawman_strings.h
	DEPENDS sawman.h
)


FLUX_FILE_SAWMAN (SaWMan)
FLUX_FILE_SAWMAN (SaWManManager)
FLUX_FILE_SAWMAN (SaWManProcess)

set (LIBSAWMAN_SRC
	${CMAKE_CURRENT_BINARY_DIR}/sawman_strings.h
	Debug.cpp
	${CMAKE_CURRENT_BINARY_DIR}/SaWMan.cpp
	SaWMan_real.cpp
	${CMAKE_CURRENT_BINARY_DIR}/SaWManManager.cpp
	SaWManManager_real.cpp
	${CMAKE_CURRENT_BINARY_DIR}/SaWManProcess.cpp
	SaWManProcess_real.cpp
	isawman.c
	isawmanmanager.c
	region.c
	sawman_core.c
	sawman_config.c
	sawman_draw.c
	sawman_updates.c
	sawman_window.c 
)

install (FILES
	Debug.h
	sawman.h
	${CMAKE_CURRENT_BINARY_DIR}/sawman_strings.h
	sawman_types.h

	DESTINATION include/sawman
)

add_library (sawman SHARED ${LIBSAWMAN_SRC})

target_link_libraries (sawman
	directfb
)

INSTALL_DIRECTFB_LIB (sawman)

install(
	FILES ${CMAKE_CURRENT_BINARY_DIR}/sawman.pc
	DESTINATION lib/pkgconfig
)
