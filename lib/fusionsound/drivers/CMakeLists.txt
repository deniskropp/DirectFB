include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories ("${CMAKE_CURRENT_BINARY_DIR}")

DEFINE_DIRECTFB_MODULE (fusionsound_driver_dummy fusionsound_dummy dummy.c fusionsound ${MODULEDIR}/snddrivers)

if (ENABLE_FUSIONSOUND_DRIVER_ALSA)
	include_directories ("${ALSA_INCLUDE_DIRS}")

	set (FUSIONSOUND_DRIVER_ALSA_LIBS
		${ALSA_LIBRARIES}
		fusionsound
	)

	DEFINE_DIRECTFB_MODULE (fusionsound_driver_alsa
		fusionsound_alsa alsa.c "${FUSIONSOUND_DRIVER_ALSA_LIBS}" ${MODULEDIR}/snddrivers
	)
endif()

if (ENABLE_FUSIONSOUND_DRIVER_OSS)
	DEFINE_DIRECTFB_MODULE (fusionsound_driver_oss
		fusionsound_oss oss.c fusionsound ${MODULEDIR}/snddrivers
	)
endif()

if (ENABLE_FUSIONSOUND_DRIVER_WAVE)
	DEFINE_DIRECTFB_MODULE (fusionsound_driver_wave
		fusionsound_wave wave.c fusionsound ${MODULEDIR}/snddrivers
	)
endif()
