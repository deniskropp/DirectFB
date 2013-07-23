include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories ("${CMAKE_CURRENT_BINARY_DIR}")

configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/build.h.in"
                "${CMAKE_CURRENT_BINARY_DIR}/build.h" )

configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/direct.pc.in"
                "${CMAKE_CURRENT_BINARY_DIR}/direct.pc" )

add_custom_command(
	OUTPUT  direct_result.c
	COMMAND echo "\\#include \\<config.h\\>"        > direct_result.c
	COMMAND echo                                   >> direct_result.c
	COMMAND echo "\\#include \\<direct/types.h\\>" >> direct_result.c
	COMMAND echo                                   >> direct_result.c
	COMMAND echo                                   >> direct_result.c
	COMMAND ${MKRESULT} DirectResult DR ${CMAKE_CURRENT_SOURCE_DIR}/result.h >> direct_result.c
	COMMAND echo                                   >> direct_result.c
	DEPENDS result.h
)

set (LIBDIRECT_SRC       
	String.cpp
	clock.c
	conf.c
	debug.c
	direct.c
	${CMAKE_CURRENT_BINARY_DIR}/direct_result.c
	fastlz.c
	fifo.c
	flz.c
	hash.c
	init.c
	interface.c
	list.c
	log.c
	log_domain.c
	map.c
	mem.c
	memcpy.c
	messages.c
	modules.c
	perf.c
	print.c
	processor.c
	result.c
	serial.c
	signals.c
	stream.c
	system.c
	trace.c
	tree.c
	thread.c
	utf8.c
	util.c
	uuid.c
)

set (LIBDIRECT_GLIBC_SRC
	os/linux/glibc/clock.c
	os/linux/glibc/deprecated.c
	os/linux/glibc/filesystem.c
	os/linux/glibc/log.c
	os/linux/glibc/mem.c
	os/linux/glibc/mutex.c
	os/linux/glibc/signals.c
	os/linux/glibc/system.c
	os/linux/glibc/thread.c
	os/linux/glibc/util.c
)

install (FILES
	Lists.h
	LockWQ.h
	Magic.h
	Mutex.h
	Performer.h
	String.h
	TLSObject.h
	ToString.h
	atomic.h
	${CMAKE_CURRENT_BINARY_DIR}/build.h
	clock.h
	compiler.h
	conf.h
	debug.h
	direct.h
	fastlz.h
	fifo.h
	filesystem.h
	flz.h
	hash.h
	init.h
	interface.h
	interface_implementation.h
	list.h
	log.h
	log_domain.h
	map.h
	mem.h
	memcpy.h
	messages.h
	modules.h
	perf.h
	print.h
	processor.h
	result.h
	serial.h
	signals.h
	stream.h
	system.h
	thread.h
	trace.h
	tree.h
	types.h
	utf8.h
	util.h
	uuid.h

	DESTINATION include/directfb/direct
)

install (FILES
	os/clock.h
	os/filesystem.h
	os/log.h
	os/mem.h
	os/mutex.h
	os/signals.h
	os/system.h
	os/thread.h
	os/types.h
	os/waitqueue.h

	DESTINATION include/directfb/direct/os
)

if (LINUX)
	install (FILES
		os/linux/glibc/filesystem.h
		os/linux/glibc/mutex.h
		os/linux/glibc/thread.h
		os/linux/glibc/types.h
		os/linux/glibc/waitqueue.h
	
		DESTINATION include/directfb/direct/os/linux/glibc
	)
endif()

add_library (direct SHARED ${LIBDIRECT_SRC} ${LIBDIRECT_GLIBC_SRC})

target_link_libraries (direct
	pthread
	dl
	rt
)

INSTALL_DIRECTFB_LIB (direct)

install(
	FILES ${CMAKE_CURRENT_BINARY_DIR}/direct.pc
	DESTINATION lib/pkgconfig
)


