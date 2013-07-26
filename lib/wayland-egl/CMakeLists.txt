include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories ("${WAYLAND_SERVER_INCLUDE_DIR}")
include_directories ("${WAYLAND_CLIENT_INCLUDE_DIR}")

configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/wayland-egl.pc.in"
                "${CMAKE_CURRENT_BINARY_DIR}/wayland-egl.pc" @ONLY)


add_library (wayland-egl SHARED wayland-egl.c)

target_link_libraries (wayland-egl ${WAYLAND_SERVER_LIBRARIES} ${WAYLAND_CLIENT_LIBRARIES})


set_target_properties (wayland-egl PROPERTIES SOVERSION 1 VERSION 1.0.0)
install (TARGETS wayland-egl LIBRARY DESTINATION lib)

install(
	FILES ${CMAKE_CURRENT_BINARY_DIR}/wayland-egl.pc
	DESTINATION lib/pkgconfig
)
