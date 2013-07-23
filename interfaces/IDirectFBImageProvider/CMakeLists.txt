include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
include_directories ("${PROJECT_SOURCE_DIR}/gfxdrivers")

if (ENABLE_IMAGEPROVIDER_PNG)

	include_directories ("${PNG_INCLUDE_DIRS}")

	set (IMAGEPROVIDER_PNG_SRC
		idirectfbimageprovider_png.c
	)

	add_library (imageprovider_png MODULE ${IMAGEPROVIDER_PNG_SRC})

	target_link_libraries (imageprovider_png
		pthread
		dl
		rt
		${PNG_LIBRARIES}
		directfb
	)

	set_target_properties(imageprovider_png PROPERTIES OUTPUT_NAME idirectfbimageprovider_png)

	install (TARGETS imageprovider_png LIBRARY DESTINATION ${IMAGEPROVIDER_DIR})
endif()

if (ENABLE_IMAGEPROVIDER_JPEG)

	include_directories ("${JPEG_INCLUDE_DIR}")

	set (IMAGEPROVIDER_jpeg_SRC
		idirectfbimageprovider_jpeg.c
	)

	add_library (imageprovider_jpeg MODULE ${IMAGEPROVIDER_jpeg_SRC})

	target_link_libraries (imageprovider_jpeg
		pthread
		dl
		rt
		${JPEG_LIBRARIES}
		directfb
	)

	set_target_properties(imageprovider_jpeg PROPERTIES OUTPUT_NAME idirectfbimageprovider_jpeg)

	install (TARGETS imageprovider_jpeg LIBRARY DESTINATION ${IMAGEPROVIDER_DIR})
endif()
