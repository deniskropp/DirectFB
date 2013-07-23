include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories ("${CMAKE_CURRENT_BINARY_DIR}")

configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/build.h.in"
                "${CMAKE_CURRENT_BINARY_DIR}/build.h" )

configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/voodoo.pc.in"
                "${CMAKE_CURRENT_BINARY_DIR}/voodoo.pc" )

set (LIBVOODOO_SRC
	client.c
	conf.c
	connection.cpp
	connection_link.cpp
	connection_packet.cpp
	connection_raw.cpp
	dispatcher.cpp
	init.c
	instance.cpp
	interface.c
	ivoodooplayer.c
	ivoodooplayer_dispatcher.c
	ivoodooplayer_requestor.c
	manager.cpp
	manager_c.cpp
	play.c
	play_server.c
	server.c
)

if (LINUX)
	set (LIBVOODOO_UNIX_SRC
		unix/interfaces_unix.c
		unix/link_unix.c
	)
endif()

install (FILES
	app.h
	${CMAKE_CURRENT_BINARY_DIR}/build.h
	client.h
	conf.h
	connection.h
	connection_link.h
	connection_packet.h
	connection_raw.h
	dispatcher.h
	init.h
	instance.h
	interface.h
	ivoodooplayer.h
	ivoodooplayer_dispatcher.h
	link.h
	manager.h
	message.h
	server.h
	packet.h
	play.h
	play_internal.h
	play_server.h
	types.h

	DESTINATION include/directfb/voodoo
)

add_library (voodoo SHARED ${LIBVOODOO_SRC} ${LIBVOODOO_UNIX_SRC})

target_link_libraries (voodoo
	pthread
	dl
	rt
	directfb
)

INSTALL_DIRECTFB_LIB (voodoo)

install(
	FILES ${CMAKE_CURRENT_BINARY_DIR}/voodoo.pc
	DESTINATION lib/pkgconfig
)
