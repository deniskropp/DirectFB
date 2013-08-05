include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
include_directories ("${PROJECT_SOURCE_DIR}/gfxdrivers")
include_directories ("${X11_INCLUDE_DIR}")

set (SYSTEM_X11_SRC
	X11EGLImpl.cpp
	idirectfbgl.c
	primary.c
	surfacemanager.c
	vpsmem_surface_pool.c
	x11.c
	x11image.c
	x11input.c
	x11_surface_pool.c
	x11_window_pool.c
	xwindow.c
)


set (SYSTEM_X11_LIBS
	${X11_LIBRARIES}
	${X11_Xcomposite_LIB}
	directfb
)

DEFINE_DIRECTFB_MODULE (system_x11 directfb_x11 "${SYSTEM_X11_SRC}" "${SYSTEM_X11_LIBS}" ${SYSTEMS_DIR})


set (LIBDFBEGL_CORE_X11_LIBS
	${X11_LIBRARIES}
	xcb
	directfb
	EGL
)

DEFINE_DIRECTFB_MODULE (dfbegl_core_x11 dfbegl_core_x11 EGLDisplayX11.cpp "${LIBDFBEGL_CORE_X11_LIBS}" ${MODULEDIR}/dfbegl_core)
