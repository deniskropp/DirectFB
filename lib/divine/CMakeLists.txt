include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")

configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/divine.pc.in"
                "${CMAKE_CURRENT_BINARY_DIR}/divine.pc" @ONLY)


set (LIBDIVINE_SRC
	divine.c
	idivine.c
)

install (FILES
	divine.h

	DESTINATION include/divine
)

add_library (divine SHARED ${LIBDIVINE_SRC})

target_link_libraries (divine
	directfb
)

INSTALL_DIRECTFB_LIB (divine)

install(
	FILES ${CMAKE_CURRENT_BINARY_DIR}/divine.pc
	DESTINATION lib/pkgconfig
)
