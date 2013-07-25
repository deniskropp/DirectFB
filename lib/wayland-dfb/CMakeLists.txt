include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories ("${WAYLAND_INCLUDE_DIR}")

configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/wayland-dfb.pc.in"
                "${CMAKE_CURRENT_BINARY_DIR}/wayland-dfb.pc" @ONLY)


set (LIBWAYLANDDFB_SRC
	wayland-dfb.cpp
	wayland-dfb-protocol.c
)

install (FILES
	wayland-dfb.h

	DESTINATION include/directfb
)

add_library (wayland-dfb SHARED ${LIBWAYLANDDFB_SRC})

target_link_libraries (wayland-dfb
	${WAYLAND_LIBRARIES}
)


set_target_properties (wayland-dfb PROPERTIES SOVERSION 1 VERSION 1.0.0)
install (TARGETS wayland-dfb LIBRARY DESTINATION lib)

install(
	FILES ${CMAKE_CURRENT_BINARY_DIR}/wayland-dfb.pc
	DESTINATION lib/pkgconfig
)
