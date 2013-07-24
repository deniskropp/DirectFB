add_definitions (-DBINDIR="${CMAKE_INSTALL_PREFIX}/bin")

add_executable (dfbdump dfbdump.c)
target_link_libraries (dfbdump directfb)

add_executable (dfbfx dfbfx.c)
target_link_libraries (dfbfx direct)

add_executable (dfbg dfbg.c)
target_link_libraries (dfbg directfb)

add_executable (dfbinfo dfbinfo.c)
target_link_libraries (dfbinfo directfb)

add_executable (dfbinspector dfbinspector.c)
target_link_libraries (dfbinspector directfb)

add_executable (dfblayer dfblayer.c)
target_link_libraries (dfblayer directfb)

add_executable (dfbmaster dfbmaster.c)
target_link_libraries (dfbmaster directfb)

add_executable (dfbscreen dfbscreen.c)
target_link_libraries (dfbscreen directfb)

add_executable (dfbpenmount dfbpenmount.c)
target_link_libraries (dfbpenmount directfb)

install (TARGETS
	dfbdump
	dfbfx
	dfbg
	dfbinfo
	dfbinspector
	dfblayer
	dfbmaster
	dfbscreen
	dfbpenmount

	DESTINATION bin
)

if (LINUX)
	add_executable (fusion_bench fusion_bench.c)
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
	add_executable (dfbproxy dfbproxy.c)
	target_link_libraries (dfbproxy directfb)

	add_executable (voodooplay voodooplay.c)
	target_link_libraries (voodooplay directfb)

	add_executable (voodooplay_client voodooplay_client.c)
	target_link_libraries (voodooplay_client directfb)

	add_executable (voodooplay_server voodooplay_server.c)
	target_link_libraries (voodooplay_server directfb)

	install (TARGETS
		dfbproxy
		voodooplay
		voodooplay_client
		voodooplay_server

		DESTINATION bin
	)
endif()

if (ENABLE_SAWMAN)
	include_directories ("${PROJECT_BINARY_DIR}/lib/sawman")
	include_directories ("${PROJECT_SOURCE_DIR}/lib/sawman")

	add_executable (swmdump swmdump.c)
	target_link_libraries (swmdump sawman)

	install (TARGETS swmdump DESTINATION bin)
endif()

if (ENABLE_FUSIONDALE)
	include_directories ("${PROJECT_BINARY_DIR}/lib/fusiondale")
	include_directories ("${PROJECT_SOURCE_DIR}/lib/fusiondale")

	add_executable (fddump fddump.c)
	target_link_libraries (fddump fusiondale)

	add_executable (fdmaster fdmaster.c)
	target_link_libraries (fdmaster fusiondale)

	install (TARGETS
		fddump
		fdmaster

		DESTINATION bin
	)
endif()

if (ENABLE_FUSIONSOUND)
	include_directories ("${PROJECT_BINARY_DIR}/lib/fusionsound")
	include_directories ("${PROJECT_SOURCE_DIR}/lib/fusionsound")

	add_executable (fsdump fsdump.c)
	target_link_libraries (fsdump fusionsound)

	add_executable (fsmaster fsmaster.c)
	target_link_libraries (fsmaster fusionsound)

	add_executable (fsplay fsplay.c)
	target_link_libraries (fsplay fusionsound)

	add_executable (fsvolume fsvolume.c)
	target_link_libraries (fsvolume fusionsound)

	install (TARGETS
		fsdump
		fsmaster
		fsplay
		fsvolume

		DESTINATION bin
	)

	if (ENABLE_VOODO)
		add_executable (fsproxy fsproxy.c)
		target_link_libraries (fsproxy fusionsound)

		install (TARGETS fsproxy DESTINATION bin)
	endif()
endif()

if (ENABLE_DIVINE)
	include_directories ("${PROJECT_BINARY_DIR}/lib/divine")
	include_directories ("${PROJECT_SOURCE_DIR}/lib/divine")

	add_executable (spooky spooky.c)
	target_link_libraries (spooky divine m)

	install (TARGETS spooky DESTINATION bin)
endif()

if (PNG_FOUND)
	include_directories ("${PNG_INCLUDE_DIRS}")

	add_executable (directfb-csource directfb-csource.c)
	target_link_libraries (directfb-csource ${PNG_LIBRARIES})

	add_executable (mkdfiff mkdfiff.c)
	target_link_libraries (mkdfiff ${PNG_LIBRARIES} direct)

	add_executable (mkdgifft mkdgifft.cpp)
	target_link_libraries (mkdgifft ${PNG_LIBRARIES} directfb)

	install (TARGETS
		directfb-csource
		mkdfiff
		mkdgifft

		DESTINATION bin
	)
endif()

if (FREETYPE_FOUND)
	include_directories ("${FREETYPE_INCLUDE_DIRS}")

	add_executable (mkdgiff mkdgiff.c)
	target_link_libraries (mkdgiff ${FREETYPE_LIBRARIES} direct)

	install (TARGETS mkdgiff DESTINATION bin )
endif()



