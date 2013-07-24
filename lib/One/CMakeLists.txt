include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")

configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/one.pc.in"
                "${CMAKE_CURRENT_BINARY_DIR}/one.pc" @ONLY)

set (LIBONE_SRC
	One.c
)


install (FILES
	One.h
	OneQueue.h
	OneThread.h
	OneTypes.h

	DESTINATION include/One
)

add_library (one SHARED ${LIBONE_SRC} ${LIBONE_NON_PURE_VOODOO_SRC} ${LIBONE_SHM_SOURCES})

target_link_libraries (one
	direct
)

INSTALL_DIRECTFB_LIB (one)

install(
	FILES ${CMAKE_CURRENT_BINARY_DIR}/one.pc
	DESTINATION lib/pkgconfig
)
