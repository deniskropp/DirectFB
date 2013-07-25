include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")

configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/build.h.in"
                "${CMAKE_CURRENT_BINARY_DIR}/build.h" )

configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/egl.pc.in"
                "${CMAKE_CURRENT_BINARY_DIR}/egl.pc" @ONLY)

add_custom_command(
	OUTPUT dfbegl_int_names.h
	COMMAND echo "#ifndef __DFBEGL_INT_NAMES_H__" > dfbegl_int_names.h
	COMMAND echo "#define __DFBEGL_INT_NAMES_H__" >> dfbegl_int_names.h
	COMMAND echo >> dfbegl_int_names.h
	COMMAND echo "#include <EGL/egl.h>" >> dfbegl_int_names.h
	COMMAND echo "#include <EGL/eglext.h>" >> dfbegl_int_names.h
	COMMAND echo "#include <EGL/egldfbext.h>" >> dfbegl_int_names.h
	COMMAND echo "struct DFBEGLIntName {" >> dfbegl_int_names.h
	COMMAND echo "     EGLint      value;" >> dfbegl_int_names.h
	COMMAND echo "     const char *name;" >> dfbegl_int_names.h
	COMMAND echo "};" >> dfbegl_int_names.h
	COMMAND echo >> dfbegl_int_names.h
	COMMAND echo "#define DirectFBEGLIntNames(Identifier) struct DFBEGLIntName Identifier[] = {\\" >> dfbegl_int_names.h	
	COMMAND grep -hE "^\#define[[:blank:]]+EGL_[[:alnum:]_]+[[:blank:]]+(0x[[:alnum:]][[:alnum:]][[:alnum:]][[:alnum:]])\\b" ${CMAKE_CURRENT_SOURCE_DIR}/EGL/egl.h | perl -p -e "s/\#define[ \t]+EGL_([0-9A-Za-z_]+)[ \t]+(0x[0-9a-zA-Z][0-9a-zA-Z][0-9a-zA-Z][0-9a-zA-Z]).*/      \\{ EGL_\\1 \\/\\*\\2\\*\\/, \"\\1\"\\}, \\\\/" >> dfbegl_int_names.h
	COMMAND echo "     { 0, \"NULL\" } \\" >> dfbegl_int_names.h
	COMMAND echo "};" >> dfbegl_int_names.h
	COMMAND echo >> dfbegl_int_names.h
	COMMAND echo "#endif"  >> dfbegl_int_names.h
	VERBATIM
	DEPENDS EGL/egl.h
)



set (LIBDFBEGL_SRC
	dfbegl_int_names.h
	config.cpp
	context.cpp
	core.cpp
	display.cpp
	library.cpp
	image.cpp
	option.cpp
	surface.cpp
	tls.cpp
	util.cpp
)

install (FILES
	${CMAKE_CURRENT_BINARY_DIR}/build.h
	dfbegl.h
	${CMAKE_CURRENT_BINARY_DIR}/dfbegl_int_names.h

	DESTINATION include/directfb/egl
)

install (FILES
	EGL/egldfbext.h
	EGL/egldfbmangle.h
	EGL/egldfbunmangle.h
	EGL/eglext.h
	EGL/egl.h
	EGL/eglplatform.h

	DESTINATION include/directfb/egl/EGL
)

install (FILES
	KHR/khrplatform.h

	DESTINATION include/directfb/egl/KHR
)

add_library (DFBEGL SHARED ${LIBDFBEGL_SRC})

target_link_libraries (DFBEGL
	direct
)

INSTALL_DIRECTFB_LIB (DFBEGL)

add_library (EGL SHARED api.cpp)

target_link_libraries (EGL
	direct
	DFBEGL
)

set_target_properties (EGL PROPERTIES SOVERSION 1 VERSION 1.0.0)
install (TARGETS EGL LIBRARY DESTINATION lib/dfb-egl)

install(
	FILES ${CMAKE_CURRENT_BINARY_DIR}/egl.pc
	DESTINATION lib/pkgconfig
)


