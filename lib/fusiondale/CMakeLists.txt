include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")

configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/fusiondale_version.h.in"
                "${CMAKE_CURRENT_BINARY_DIR}/fusiondale_version.h")

configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/fusiondale.pc.in"
                "${CMAKE_CURRENT_BINARY_DIR}/fusiondale.pc" @ONLY)

set (LIBFUSIONDALE_SRC
	fusiondale.c
	ifusiondale.c

	coma/coma.c
	coma/component.c
	coma/icoma.c
	coma/icomacomponent.c
	coma/policy.c
	coma/thread.c

	core/dale_core.c
	core/messenger.c
	core/messenger_port.c

	messenger/ifusiondalemessenger.c

	misc/dale_config.c
)

install (FILES
	fusiondale.h
	fusiondale_util.h
	${CMAKE_CURRENT_BINARY_DIR}/fusiondale_version.h
	
	DESTINATION include/directfb/fusiondale
)

install (FILES
	coma/coma.h
	coma/coma_types.h
	coma/component.h
	coma/icomacomponent.h
	coma/icoma.h
	coma/policy.h
	coma/thread.h

	DESTINATION include/directfb/fusiondale/coma
)

install (FILES
	core/dale_core.h
	core/dale_types.h
	core/messenger.h
	core/messenger_port.h

	DESTINATION include/directfb/fusiondale/core
)

add_library (fusiondale SHARED ${LIBFUSIONDALE_SRC} ${LIBFUSIONDALE_NON_PURE_VOODOO_SRC} ${LIBFUSIONDALE_SHM_SOURCES})

target_link_libraries (fusiondale
        pthread
        dl
        rt
	m
	direct
	fusion
)

INSTALL_DIRECTFB_LIB (fusiondale)

install(
	FILES ${CMAKE_CURRENT_BINARY_DIR}/fusiondale.pc
	DESTINATION lib/pkgconfig
)


if (ENABLE_ONE)
	add_subdirectory (one)
endif()
