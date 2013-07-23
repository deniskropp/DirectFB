include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
include_directories ("${PROJECT_SOURCE_DIR}/gfxdrivers")

if (ENABLE_FONTPROVIDER_FREETYPE)

	include_directories ("${FREETYPE_INCLUDE_DIRS}")

	set (FONTPROVIDER_FREETYPE_LIBS
		directfb
		${FREETYPE_LIBRARIES}
	)

	DEFINE_DIRECTFB_MODULE (fontprovider_freetype
		 idirectfbfont_ft2 idirectfbfont_ft2.c ${FONTPROVIDER_FREETYPE_LIBS} ${FONTPROVIDER_DIR}
	)
endif()

if (ENABLE_FONTPROVIDER_DGIFF)
	DEFINE_DIRECTFB_MODULE (fontprovider_dgiff
		idirectfbfont_dgiff idirectfbfont_dgiff.c directfb ${FONTPROVIDER_DIR}
	)
endif()
