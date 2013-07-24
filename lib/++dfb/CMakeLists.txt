include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories ("${PROJECT_SOURCE_DIR}/include/++dfb")

configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/++dfb.pc.in"
                "${CMAKE_CURRENT_BINARY_DIR}/++dfb.pc" @ONLY)


set (LIBPPDFB_SRC
	idirectfb.cpp
	idirectfbdatabuffer.cpp
	idirectfbdisplaylayer.cpp
	idirectfbeventbuffer.cpp
	idirectfbfont.cpp
	idirectfbimageprovider.cpp
	idirectfbinputdevice.cpp
	idirectfbpalette.cpp
	idirectfbscreen.cpp
	idirectfbsurface.cpp
	idirectfbvideoprovider.cpp
	idirectfbwindow.cpp
	ppdfb.cpp
)

add_library (++dfb SHARED ${LIBPPDFB_SRC})

target_link_libraries (++dfb
	directfb
)

INSTALL_DIRECTFB_LIB (++dfb)

install(
	FILES ${CMAKE_CURRENT_BINARY_DIR}/++dfb.pc
	DESTINATION lib/pkgconfig
)
