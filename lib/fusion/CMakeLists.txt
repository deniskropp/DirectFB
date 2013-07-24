include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")

configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/build.h.in"
                "${CMAKE_CURRENT_BINARY_DIR}/build.h" )

configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/fusion.pc.in"
                "${CMAKE_CURRENT_BINARY_DIR}/fusion.pc" @ONLY)

set (LIBFUSION_SRC
	conf.c
	hash.c
	init.c
	shmalloc.c
	vector.c
)

set (LIBFUSION_NON_PURE_VOODOO_SRC
	Debug.cpp
	arena.c
	call.c
	fusion.c
	lock.c
	object.c
	property.c
	reactor.c
	ref.c
)

if (ENABLE_MULTI)
	set (LIBFUSION_SHM_SOURCES 
		shm/heap.c
		shm/pool.c
		shm/shm.c
	)
else()
	set (LIBFUSION_SHM_SOURCES 
		shm/fake.c
	)
endif()


install (FILES
	Debug.h
	arena.h
	${CMAKE_CURRENT_BINARY_DIR}/build.h
	call.h
	conf.h
	fusion.h
	fusion_internal.h
	hash.h
	init.h
	lock.h
	object.h
	property.h
	protocol.h
	reactor.h
	ref.h
	shmalloc.h
	types.h
	vector.h

	DESTINATION include/directfb/fusion
)

add_library (fusion SHARED ${LIBFUSION_SRC} ${LIBFUSION_NON_PURE_VOODOO_SRC} ${LIBFUSION_SHM_SOURCES})

target_link_libraries (fusion
	direct
)

INSTALL_DIRECTFB_LIB (fusion)

install(
	FILES ${CMAKE_CURRENT_BINARY_DIR}/fusion.pc
	DESTINATION lib/pkgconfig
)


