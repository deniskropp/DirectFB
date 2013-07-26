include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
include_directories ("${PROJECT_BINARY_DIR}/lib/wayland-dfb")
include_directories ("${PROJECT_SOURCE_DIR}/lib/wayland-dfb")
include_directories ("${PROJECT_BINARY_DIR}/lib/wayland-egl")
include_directories ("${PROJECT_SOURCE_DIR}/lib/wayland-egl")
include_directories ("${WAYLAND_SERVER_INCLUDE_DIR}")
include_directories ("${WAYLAND_CLIENT_INCLUDE_DIR}")


set (DFBEGLCOREWAYLAND_LIBS
	directfb
	EGL
	wayland-dfb
)

DEFINE_DIRECTFB_MODULE( dfbegl_core_wayland
	dfbegl_core_wayland EGLDisplayWayland.cpp "${DFBEGLCOREWAYLAND_LIBS}" ${MODULEDIR}/dfbegl_core
)

