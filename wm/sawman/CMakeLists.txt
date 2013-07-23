include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories ("${PROJECT_BINARY_DIR}/lib/sawman")
include_directories ("${PROJECT_SOURCE_DIR}/lib/sawman")

set (WM_DEFAULT_SRC
	sawman_wm.c
)

add_library (wm_sawman MODULE ${WM_DEFAULT_SRC})

target_link_libraries (wm_sawman
	pthread
	dl
	rt
	fusion
	direct
	directfb
	sawman
)

set_target_properties(wm_sawman PROPERTIES OUTPUT_NAME directfbwm_sawman)

install (TARGETS wm_sawman LIBRARY DESTINATION ${WM_DIR})
