include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
include_directories ("${PROJECT_SOURCE_DIR}/gfxdrivers")

if (ENABLE_FONTPROVIDER_FREETYPE)

	include_directories ("${FREETYPE_INCLUDE_DIRS}")

	set (FONTPROVIDER_FREETYPE_SRC
		idirectfbfont_ft2.c
	)

	add_library (fontprovider_freetype MODULE ${FONTPROVIDER_FREETYPE_SRC})

	target_link_libraries (fontprovider_freetype
		pthread dl rt
		directfb
		${FREETYPE_LIBRARIES}
	)

	set_target_properties(fontprovider_freetype PROPERTIES OUTPUT_NAME idirectfbfont_ft2)

	install (TARGETS fontprovider_freetype LIBRARY DESTINATION ${FONTPROVIDER_DIR})
endif()

if (ENABLE_FONTPROVIDER_DGIFF)

	set (FONTPROVIDER_DGIFF_SRC
		idirectfbfont_dgiff.c
	)

	add_library (fontprovider_dgiff MODULE ${FONTPROVIDER_DGIFF_SRC})

	target_link_libraries (fontprovider_dgiff
		pthread dl rt
		directfb
	)

	set_target_properties(fontprovider_dgiff PROPERTIES OUTPUT_NAME idirectfbfont_dgiff)

	install (TARGETS fontprovider_dgiff LIBRARY DESTINATION ${FONTPROVIDER_DIR})
endif()
