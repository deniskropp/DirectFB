include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")


if (ENABLE_FUSIONSOUND_MULTICHANNEL)
	set (FS_MAX_CHANNELS 6)
else()
	set (FS_MAX_CHANNELS 2)
endif()

configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/fusionsound_limits.h.in"
                "${CMAKE_CURRENT_BINARY_DIR}/fusionsound_limits.h")

configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/fusionsound_version.h.in"
                "${CMAKE_CURRENT_BINARY_DIR}/fusionsound_version.h")

configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/fusionsound.pc.in"
                "${CMAKE_CURRENT_BINARY_DIR}/fusionsound.pc" @ONLY)

set (LIBFUSIONSOUND_SRC
	fusionsound.c
	ifusionsound.c
	ifusionsoundbuffer.c
	ifusionsoundplayback.c
	ifusionsoundstream.c

	core/core_sound.c
	core/playback.c
	core/sound_buffer.c
	core/sound_device.c

	media/ifusionsoundmusicprovider.c

	misc/sound_conf.c
	misc/sound_util.c
)

install (FILES
	fusionsound.h
	${CMAKE_CURRENT_BINARY_DIR}/fusionsound_limits.h
	${CMAKE_CURRENT_BINARY_DIR}/fusionsound_version.h

	DESTINATION include/fusionsound
)

add_library (fusionsound SHARED ${LIBFUSIONSOUND_SRC})

target_link_libraries (fusionsound
	direct
	fusion
)

INSTALL_DIRECTFB_LIB (fusionsound)

install(
	FILES ${CMAKE_CURRENT_BINARY_DIR}/fusionsound.pc
	DESTINATION lib/pkgconfig
)

add_subdirectory (drivers)
