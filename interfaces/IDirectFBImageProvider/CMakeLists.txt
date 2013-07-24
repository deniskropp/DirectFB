include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories ("${PROJECT_SOURCE_DIR}/gfxdrivers")

if (ENABLE_IMAGEPROVIDER_PNG)
	include_directories ("${PNG_INCLUDE_DIRS}")

	set (IMAGEPROVIDER_PNG_LIBS
		${PNG_LIBRARIES}
		directfb
	)

	DEFINE_DIRECTFB_MODULE( imageprovider_png
		idirectfbimageprovider_png idirectfbimageprovider_png.c "${IMAGEPROVIDER_PNG_LIBS}" ${IMAGEPROVIDER_DIR}
	)
endif()

if (ENABLE_IMAGEPROVIDER_JPEG)
	include_directories ("${JPEG_INCLUDE_DIR}")

	set (IMAGEPROVIDER_JPEG_LIBS
		${JPEG_LIBRARIES}
		directfb
	)

	DEFINE_DIRECTFB_MODULE( imageprovider_jpeg
		idirectfbimageprovider_jpeg idirectfbimageprovider_jpeg.c "${IMAGEPROVIDER_JPEG_LIBS}" ${IMAGEPROVIDER_DIR}
	)
endif()
