include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
include_directories ("${PROJECT_SOURCE_DIR}/gfxdrivers")
include_directories ("${X11_INCLUDE_DIR}")

set (SYSTEM_X11_SRC
        idirectfbgl.c
        primary.c
        surfacemanager.c
        vpsmem_surface_pool.c
        x11.c
        x11image.c
        x11input.c
        x11_surface_pool.c
	xwindow.c
)


add_library (system_x11 MODULE ${SYSTEM_X11_SRC})

target_link_libraries (system_x11
	pthread
	dl
	rt
	${X11_LIBRARIES}
	directfb
)

set_target_properties(system_x11 PROPERTIES OUTPUT_NAME directfb_x11)

install (TARGETS system_x11 LIBRARY DESTINATION ${SYSTEMS_DIR})
