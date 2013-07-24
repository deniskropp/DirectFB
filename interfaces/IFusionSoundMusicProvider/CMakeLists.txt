include_directories ("${PROJECT_BINARY_DIR}/lib/fusionsound")
include_directories ("${PROJECT_SOURCE_DIR}/lib/fusionsound")
include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")

set (MUSICPROVIDER_DIR "${INTERFACES_DIR}/IFusionSoundMusicProvider")

if (ENABLE_FUSIONSOUND_MUSICPROVIDER_MAD)

	include_directories ("${MAD_INCLUDE_DIRS}")

	set (MUSICPROVIDER_MAD_LIBS
		${MAD_LIBRARIES}
		fusionsound
	)

	DEFINE_DIRECTFB_MODULE (fusionsound_musicprivider_mad
		 ifusionsoundmusicprovider_mad ifusionsoundmusicprovider_mad.c "${MUSICPROVIDER_MAD_LIBS}" ${MUSICPROVIDER_DIR}
	)
endif()

if (ENABLE_FUSIONSOUND_MUSICPROVIDER_WAVE)
	DEFINE_DIRECTFB_MODULE (fusionsound_musicprovider_wave
		ifusionsoundmusicprovider_wave ifusionsoundmusicprovider_wave.c fusionsound ${MUSICPROVIDER_DIR}
	)
endif()
