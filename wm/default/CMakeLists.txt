include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories ("${CMAKE_CURRENT_BINARY_DIR}")

set (WM_DEFAULT_SRC
	default.c
)

add_library (wm_default MODULE ${WM_DEFAULT_SRC})

target_link_libraries (wm_default
	pthread
	dl
	rt
	fusion
	direct
	directfb
)

set_target_properties(wm_default PROPERTIES OUTPUT_NAME directfbwm_default)

install (TARGETS wm_default LIBRARY DESTINATION ${WM_DIR})
