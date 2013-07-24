include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories ("${CMAKE_CURRENT_BINARY_DIR}")

set (SYSTEM_FBDEV_SRC
	agp.c
	fbdev.c
	fbdev_surface_pool.c
	surfacemanager.c
	vt.c
)

DEFINE_DIRECTFB_MODULE( system_fbdev directfb_fbdev "${SYSTEM_FBDEV_SRC}" directfb ${SYSTEMS_DIR} )
