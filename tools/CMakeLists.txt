add_definitions (-DBINDIR="${CMAKE_INSTALL_PREFIX}/bin")

DEFINE_DIRECTFB_EXECUTABLE (dfbfx.c direct)
DEFINE_DIRECTFB_EXECUTABLE (dfbg.c directfb)
DEFINE_DIRECTFB_EXECUTABLE (dfbinfo.c directfb)
DEFINE_DIRECTFB_EXECUTABLE (dfbinspector.c directfb)
DEFINE_DIRECTFB_EXECUTABLE (dfblayer.c directfb)
DEFINE_DIRECTFB_EXECUTABLE (dfbmaster.c directfb)
DEFINE_DIRECTFB_EXECUTABLE (dfbscreen.c directfb)
DEFINE_DIRECTFB_EXECUTABLE (dfbpenmount.c directfb)

if (LINUX)
	DEFINE_DIRECTFB_EXECUTABLE (fusion_bench.c directfb)
	target_link_libraries (fusion_bench directfb)

	add_executable (raw15toraw24 raw15toraw24.c)
	add_executable (raw16toraw24 raw16toraw24.c)
	add_executable (raw32toraw24 raw32toraw24.c)

	install (TARGETS
		fusion_bench
		raw15toraw24
		raw16toraw24
		raw32toraw24

		DESTINATION bin
	)
endif()

if (ENABLE_VOODOO)
	DEFINE_DIRECTFB_EXECUTABLE( dfbproxy.c directfb)
	DEFINE_DIRECTFB_EXECUTABLE( voodooplay.c directfb)
	DEFINE_DIRECTFB_EXECUTABLE( voodooplay_client.c directfb)
	DEFINE_DIRECTFB_EXECUTABLE( voodooplay_server.c directfb)
endif()

if (ENABLE_SAWMAN)
	include_directories ("${PROJECT_BINARY_DIR}/lib/sawman")
	include_directories ("${PROJECT_SOURCE_DIR}/lib/sawman")

	DEFINE_DIRECTFB_EXECUTABLE (swmdump.c sawman)
endif()

if (ENABLE_FUSIONDALE)
	include_directories ("${PROJECT_BINARY_DIR}/lib/fusiondale")
	include_directories ("${PROJECT_SOURCE_DIR}/lib/fusiondale")

	DEFINE_DIRECTFB_EXECUTABLE (fddump.c fusiondale)
	DEFINE_DIRECTFB_EXECUTABLE (fdmaster.c fusiondale)
endif()

if (ENABLE_FUSIONSOUND)
	include_directories ("${PROJECT_BINARY_DIR}/lib/fusionsound")
	include_directories ("${PROJECT_SOURCE_DIR}/lib/fusionsound")

	DEFINE_DIRECTFB_EXECUTABLE (fsdump.c fusionsound)
	DEFINE_DIRECTFB_EXECUTABLE (fsmaster.c fusionsound)
	DEFINE_DIRECTFB_EXECUTABLE (fsplay.c fusionsound)
	DEFINE_DIRECTFB_EXECUTABLE (fsvolume.c fusionsound)


	if (ENABLE_VOODO)
		DEFINE_DIRECTFB_EXECUTABLE (fsproxy.c fusionsound)
	endif()
endif()

if (ENABLE_DIVINE)
	include_directories ("${PROJECT_BINARY_DIR}/lib/divine")
	include_directories ("${PROJECT_SOURCE_DIR}/lib/divine")

	set (SPOOKY_LIBS divine m)
	DEFINE_DIRECTFB_EXECUTABLE (spooky.c "${SPOOKY_LIBS}")
endif()

if (PNG_FOUND)
	include_directories ("${PNG_INCLUDE_DIRS}")

	DEFINE_DIRECTFB_EXECUTABLE (directfb-csource.c "${PNG_LIBRARIES}")

	set (MKDFIFF_LIBS ${PNG_LIBRARIES} direct)
	DEFINE_DIRECTFB_EXECUTABLE (mkdfiff.c "${MKDFIFF_LIBS}")

	set (MKDGIFFT_LIBS ${PNG_LIBRARIES} directfb)
	DEFINE_DIRECTFB_EXECUTABLE (mkdgifft.cpp "${MKDGIFFT_LIBS}")
endif()

if (FREETYPE_FOUND)
	include_directories ("${FREETYPE_INCLUDE_DIRS}")

	set (MKDGIFF_LIBS ${FREETYPE_LIBRARIES} direct)
	DEFINE_DIRECTFB_EXECUTABLE (mkdgiff.c "${MKDGIFF_LIBS}")
endif()



